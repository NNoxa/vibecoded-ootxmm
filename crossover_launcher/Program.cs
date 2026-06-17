using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace SoH2ShipCrossover;

internal enum GameId {
    Oot,
    Mm
}

internal sealed class CrossoverState {
    [JsonConverter(typeof(JsonStringEnumConverter))]
    public GameId ActiveGame { get; set; } = GameId.Oot;

    [JsonConverter(typeof(JsonStringEnumConverter))]
    public GameId RequestedGame { get; set; } = GameId.Oot;

    public string RequestedEntrance { get; set; } = "oot:resume";
    public string? ReturnEntrance { get; set; }
    public string? OriginEntrance { get; set; }
    public int SaveSlot { get; set; } = 0;
    public long CommandNonce { get; set; } = 0;
    public string Status { get; set; } = "idle";
    public string? LastError { get; set; }
    public DateTimeOffset UpdatedAt { get; set; } = DateTimeOffset.UtcNow;
}

internal sealed class LauncherConfig {
    public string SohExe { get; set; } = Path.Combine("soh", "soh.exe");
    public string TwoShipExe { get; set; } = Path.Combine("2ship", "2ship.exe");
    public string StateFile { get; set; } = "crossover_state.json";
    public string HandoffFile { get; set; } = "crossover_handoff.json";
    public int PollMilliseconds { get; set; } = 250;
    public int GracefulExitMilliseconds { get; set; } = 5000;
}

internal static class Program {
    private const string OotResumeEntrance = "oot:resume";
    private const string OotHappyMaskShopReturnEntrance = "oot:happy_mask_shop_return";
    private const string OotHappyMaskShopReturnAfterFileSelectEntrance = "oot:happy_mask_shop_return_after_file_select";
    private const string OotTitleScreenEntrance = "oot:title_screen";
    private const int SaveSlotCount = 3;

    private static readonly TimeSpan SaveDeletionDebounce = TimeSpan.FromSeconds(2);
    private static readonly Dictionary<int, DateTimeOffset> MissingOotSaveSince = new();

    private static readonly JsonSerializerOptions JsonOptions = new() {
        WriteIndented = true,
        Converters = { new JsonStringEnumConverter() },
    };

    private static int Main(string[] args) {
        try {
            var root = FindProjectRoot(AppContext.BaseDirectory);
            var config = LoadConfig(root);
            var statePath = Resolve(root, config.StateFile);
            var handoffPath = Resolve(root, config.HandoffFile);
            EnsureStateFile(statePath);
            PrepareFirstRunArchives(root);
            BootstrapMissingGameArchives(root, config);

            Console.WriteLine("SoH / 2Ship Crossover Launcher");
            Console.WriteLine($"Root: {root}");
            Console.WriteLine($"State: {statePath}");
            Console.WriteLine("Press Ctrl+C to exit.");

            using var cts = new CancellationTokenSource();
            Console.CancelKeyPress += (_, eventArgs) => {
                eventArgs.Cancel = true;
                cts.Cancel();
            };

            RunLoop(root, config, statePath, handoffPath, cts.Token);
            return 0;
        } catch (Exception ex) {
            Console.Error.WriteLine(ex);
            ShowFatalError(ex);
            return 1;
        }
    }

    private static void RunLoop(string root, LauncherConfig config, string statePath, string handoffPath, CancellationToken token) {
        Process? currentProcess = null;
        GameId currentGame = GameId.Oot;
        long seenNonce = -1;

        try {
            SyncCorrelatedSaves(root, force: true);

            var state = ReadState(statePath);
            if (ShouldQueueOotReturnFromClosedMm(state)) {
                QueueOotReturnFromClosedMm(statePath, handoffPath, state);
            } else if (IsPendingOotReturnAfterFileSelect(state)) {
                WriteHandoff(handoffPath, state);
            } else if (IsInterruptedSwitch(state)) {
                WriteHandoff(handoffPath, state);
            }

            currentGame = state.RequestedGame;
            currentProcess = StartGame(root, config, currentGame);
            state.ActiveGame = currentGame;
            state.Status = "running";
            state.LastError = null;
            WriteState(statePath, state);

            while (!token.IsCancellationRequested) {
                Thread.Sleep(Math.Max(50, config.PollMilliseconds));
                SyncCorrelatedSaves(root, force: false);

                state = ReadState(statePath);
                if (currentProcess is { HasExited: true }) {
                    if (state.RequestedGame != currentGame) {
                        Console.WriteLine($"Switch requested after {currentGame} exited: {currentGame} -> {state.RequestedGame} ({state.RequestedEntrance})");
                        state.Status = "switching";
                        state.LastError = null;
                        state.UpdatedAt = DateTimeOffset.UtcNow;
                        WriteState(statePath, state);
                        WriteHandoff(handoffPath, state);

                        currentGame = state.RequestedGame;
                        currentProcess = StartGame(root, config, currentGame);

                        state.ActiveGame = currentGame;
                        state.Status = "running";
                        state.UpdatedAt = DateTimeOffset.UtcNow;
                        WriteState(statePath, state);
                        seenNonce = state.CommandNonce;
                        continue;
                    }

                    if (currentGame == GameId.Mm && state.RequestedGame == GameId.Mm) {
                        Console.WriteLine("MM closed while active; queueing OoT return after file select.");
                        QueueOotReturnFromClosedMm(statePath, handoffPath, state);
                        currentGame = GameId.Oot;
                        currentProcess = StartGame(root, config, currentGame);
                        state = ReadState(statePath);
                        state.ActiveGame = currentGame;
                        state.Status = "running";
                        state.LastError = null;
                        WriteState(statePath, state);
                        seenNonce = state.CommandNonce;
                        continue;
                    }

                    return;
                }

                if (state.CommandNonce == seenNonce && currentProcess is { HasExited: false }) {
                    continue;
                }

                seenNonce = state.CommandNonce;
                if (state.RequestedGame == currentGame) {
                    continue;
                }

                Console.WriteLine($"Switch requested: {currentGame} -> {state.RequestedGame} ({state.RequestedEntrance})");
                state.Status = "switching";
                state.LastError = null;
                state.UpdatedAt = DateTimeOffset.UtcNow;
                WriteState(statePath, state);
                WriteHandoff(handoffPath, state);

                StopGame(currentProcess, config.GracefulExitMilliseconds);
                currentGame = state.RequestedGame;
                currentProcess = StartGame(root, config, currentGame);

                state.ActiveGame = currentGame;
                state.Status = "running";
                state.UpdatedAt = DateTimeOffset.UtcNow;
                WriteState(statePath, state);
            }
        } finally {
            StopGame(currentProcess, config.GracefulExitMilliseconds);
        }
    }

    private static void SyncCorrelatedSaves(string root, bool force) {
        for (var slot = 1; slot <= SaveSlotCount; slot++) {
            var ootSavePath = GetOotSavePath(root, slot);
            if (File.Exists(ootSavePath)) {
                MissingOotSaveSince.Remove(slot);
                continue;
            }

            var mmSavePath = GetMmSavePath(root, slot);
            var mmBackupPath = GetMmBackupSavePath(root, slot);
            if (!File.Exists(mmSavePath) && !File.Exists(mmBackupPath)) {
                MissingOotSaveSince.Remove(slot);
                continue;
            }

            if (!force) {
                if (!MissingOotSaveSince.TryGetValue(slot, out var missingSince)) {
                    MissingOotSaveSince[slot] = DateTimeOffset.UtcNow;
                    continue;
                }

                if (DateTimeOffset.UtcNow - missingSince < SaveDeletionDebounce) {
                    continue;
                }
            }

            DeleteFileIfExists(mmSavePath);
            DeleteFileIfExists(mmBackupPath);
            MissingOotSaveSince.Remove(slot);
            Console.WriteLine($"Deleted MM save slot {slot} because OoT file{slot}.sav is missing.");
        }
    }

    private static string GetOotSavePath(string root, int slot) {
        return Path.Combine(GetSohDir(root), "Save", $"file{slot}.sav");
    }

    private static string GetMmSavePath(string root, int slot) {
        return Path.Combine(GetTwoShipDir(root), "saves", $"file{slot}.json");
    }

    private static string GetMmBackupSavePath(string root, int slot) {
        return Path.Combine(GetTwoShipDir(root), "saves", $"file{slot}backup.json");
    }

    private static string GetSohDir(string root) {
        var releasePath = Path.Combine(root, "soh");
        if (Directory.Exists(releasePath)) {
            return releasePath;
        }

        return Path.Combine(root, "extracted_win", "soh");
    }

    private static string GetTwoShipDir(string root) {
        var releasePath = Path.Combine(root, "2ship");
        if (Directory.Exists(releasePath)) {
            return releasePath;
        }

        return Path.Combine(root, "extracted_win", "2ship");
    }

    private static void DeleteFileIfExists(string path) {
        try {
            if (File.Exists(path)) {
                File.Delete(path);
            }
        } catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) {
            Console.WriteLine($"Could not delete correlated save file {path}: {ex.Message}");
        }
    }

    private static Process StartGame(string root, LauncherConfig config, GameId game) {
        var exe = Resolve(root, game == GameId.Oot ? config.SohExe : config.TwoShipExe);
        if (!File.Exists(exe)) {
            throw new FileNotFoundException($"Missing executable for {game}: {exe}", exe);
        }

        var startInfo = new ProcessStartInfo {
            FileName = exe,
            WorkingDirectory = Path.GetDirectoryName(exe)!,
            UseShellExecute = false,
        };

        var process = Process.Start(startInfo) ?? throw new InvalidOperationException($"Failed to start {exe}");
        Console.WriteLine($"Started {game}: pid {process.Id}");
        return process;
    }

    private static void PrepareFirstRunArchives(string root) {
        var romDir = Path.Combine(root, "roms");
        if (!Directory.Exists(romDir)) {
            return;
        }

        CopyFirstRunRom(root, GameId.Oot);
        CopyFirstRunRom(root, GameId.Mm);
    }

    private static void BootstrapMissingGameArchives(string root, LauncherConfig config) {
        var bootstrapped = false;

        if (!HasGameArchive(root, GameId.Oot)) {
            Console.WriteLine("oot.o2r is missing. Starting SoH first so it can generate the OoT game archive.");
            RunUntilArchiveExists(root, config, GameId.Oot);
            CopyGeneratedArchiveToRelease(root, GameId.Oot);
            bootstrapped = true;
        }

        if (!HasGameArchive(root, GameId.Mm)) {
            Console.WriteLine("mm.o2r is missing. Starting 2Ship so it can generate the MM game archive.");
            RunUntilArchiveExists(root, config, GameId.Mm);
            CopyGeneratedArchiveToRelease(root, GameId.Mm);
            bootstrapped = true;
        }

        if (bootstrapped) {
            Console.WriteLine("First-run archive bootstrap complete. Starting normal crossover flow.");
        }
    }

    private static void ShowFatalError(Exception ex) {
        var message = ex switch {
            FileNotFoundException fileNotFound when !string.IsNullOrWhiteSpace(fileNotFound.FileName) =>
                $"{fileNotFound.Message}{Environment.NewLine}{Environment.NewLine}Missing file:{Environment.NewLine}{fileNotFound.FileName}",
            _ => ex.Message,
        };

        _ = MessageBoxW(IntPtr.Zero, message, "OoTxMM Launcher Error", 0x00000010);
    }

    [DllImport("user32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern int MessageBoxW(IntPtr hWnd, string text, string caption, uint type);

    private static void RunUntilArchiveExists(string root, LauncherConfig config, GameId game) {
        using var process = StartGame(root, config, game);
        while (!process.HasExited) {
            if (HasGameArchive(root, game)) {
                StopGame(process, config.GracefulExitMilliseconds);
                return;
            }

            Thread.Sleep(500);
        }

        if (!HasGameArchive(root, game)) {
            throw new InvalidOperationException($"{game} exited before generating its game archive. Check that the release includes the port support archive and assets folder, then try again.");
        }
    }

    private static bool HasGameArchive(string root, GameId game) {
        return FindGameArchive(root, game) != null;
    }

    private static string? FindGameArchive(string root, GameId game) {
        var targetDir = game == GameId.Oot ? GetSohDir(root) : GetTwoShipDir(root);
        var archiveName = game == GameId.Oot ? "oot.o2r" : "mm.o2r";
        var mqArchiveName = game == GameId.Oot ? "oot-mq.o2r" : archiveName;
        foreach (var dir in GetArchiveSearchDirs(root, game)) {
            var archivePath = Path.Combine(dir, archiveName);
            if (File.Exists(archivePath)) {
                return archivePath;
            }

            var mqArchivePath = Path.Combine(dir, mqArchiveName);
            if (File.Exists(mqArchivePath)) {
                return mqArchivePath;
            }
        }

        return null;
    }

    private static IEnumerable<string> GetArchiveSearchDirs(string root, GameId game) {
        var targetDir = game == GameId.Oot ? GetSohDir(root) : GetTwoShipDir(root);
        yield return targetDir;

        var localAppData = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData);
        var appData = Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData);
        var names = game == GameId.Oot
            ? new[] { "shipofharkinian", "soh", "Ship of Harkinian" }
            : new[] { "2ship2harkinian", "2ship", "2 Ship 2 Harkinian" };

        foreach (var baseDir in new[] { localAppData, appData }.Where(path => !string.IsNullOrWhiteSpace(path))) {
            foreach (var name in names) {
                yield return Path.Combine(baseDir, name);
            }
        }
    }

    private static void CopyGeneratedArchiveToRelease(string root, GameId game) {
        var archive = FindGameArchive(root, game);
        if (archive == null) {
            return;
        }

        var targetDir = game == GameId.Oot ? GetSohDir(root) : GetTwoShipDir(root);
        var targetPath = Path.Combine(targetDir, Path.GetFileName(archive));
        if (!Path.GetFullPath(archive).Equals(Path.GetFullPath(targetPath), StringComparison.OrdinalIgnoreCase)) {
            File.Copy(archive, targetPath, overwrite: true);
            Console.WriteLine($"Copied generated {Path.GetFileName(archive)} to {targetPath}.");
        }
    }

    private static bool HasFirstRunRom(string root, GameId game) {
        var targetDir = game == GameId.Oot ? GetSohDir(root) : GetTwoShipDir(root);
        var romName = game == GameId.Oot ? "oot.z64" : "mm.z64";
        return File.Exists(Path.Combine(targetDir, romName)) || FindRom(root, game) != null;
    }

    private static void CopyFirstRunRom(string root, GameId game) {
        var targetDir = game == GameId.Oot ? GetSohDir(root) : GetTwoShipDir(root);
        if (!Directory.Exists(targetDir)) {
            return;
        }

        var archiveName = game == GameId.Oot ? "oot.o2r" : "mm.o2r";
        var romName = game == GameId.Oot ? "oot.z64" : "mm.z64";
        var archivePath = Path.Combine(targetDir, archiveName);
        var romPath = Path.Combine(targetDir, romName);
        if (File.Exists(archivePath) || File.Exists(romPath)) {
            return;
        }

        var rom = FindRom(root, game);
        if (rom == null) {
            return;
        }

        File.Copy(rom, romPath, overwrite: false);
        Console.WriteLine($"Copied {Path.GetFileName(rom)} to {romPath} for first-run extraction.");
    }

    private static string? FindRom(string root, GameId game) {
        var searchDirs = new[] { Path.Combine(root, "roms"), root }
            .Where(Directory.Exists)
            .Distinct(StringComparer.OrdinalIgnoreCase);

        var files = searchDirs.SelectMany(Directory.EnumerateFiles)
            .Where(path => IsRomExtension(Path.GetExtension(path)))
            .Select(path => new FileInfo(path))
            .Where(file => file.Exists)
            .ToList();

        if (game == GameId.Oot) {
            return files.FirstOrDefault(file => file.Length == 67_108_864)?.FullName ??
                   files.FirstOrDefault(file => file.Name.Contains("Ocarina", StringComparison.OrdinalIgnoreCase) ||
                                                file.Name.Contains("OoT", StringComparison.OrdinalIgnoreCase))?.FullName;
        }

        return files.FirstOrDefault(file => file.Length == 33_554_432)?.FullName ??
               files.FirstOrDefault(file => file.Name.Contains("Majora", StringComparison.OrdinalIgnoreCase) ||
                                            file.Name.Contains("MM", StringComparison.OrdinalIgnoreCase))?.FullName;
    }

    private static bool IsRomExtension(string extension) {
        return extension.Equals(".z64", StringComparison.OrdinalIgnoreCase) ||
               extension.Equals(".n64", StringComparison.OrdinalIgnoreCase) ||
               extension.Equals(".v64", StringComparison.OrdinalIgnoreCase);
    }

    private static void StopGame(Process? process, int gracefulExitMilliseconds) {
        if (process == null || process.HasExited) {
            return;
        }

        try {
            process.CloseMainWindow();
            if (process.WaitForExit(gracefulExitMilliseconds)) {
                return;
            }

            process.Kill(entireProcessTree: true);
            process.WaitForExit();
        } catch (InvalidOperationException) {
        }
    }

    private static LauncherConfig LoadConfig(string root) {
        var path = Path.Combine(root, "crossover_launcher.json");
        if (!File.Exists(path)) {
            return new LauncherConfig();
        }

        return JsonSerializer.Deserialize<LauncherConfig>(File.ReadAllText(path), JsonOptions) ?? new LauncherConfig();
    }

    private static void EnsureStateFile(string path) {
        if (File.Exists(path)) {
            return;
        }

        WriteState(path, new CrossoverState {
            ActiveGame = GameId.Oot,
            RequestedGame = GameId.Oot,
            RequestedEntrance = OotResumeEntrance,
            Status = "idle",
        });
    }

    private static bool ShouldQueueOotReturnFromClosedMm(CrossoverState state) {
        return state.ActiveGame == GameId.Mm &&
               state.RequestedGame == GameId.Mm &&
               state.Status == "running";
    }

    private static bool IsPendingOotReturnAfterFileSelect(CrossoverState state) {
        return state.RequestedGame == GameId.Oot &&
               (state.RequestedEntrance == OotHappyMaskShopReturnAfterFileSelectEntrance ||
                state.RequestedEntrance == OotTitleScreenEntrance);
    }

    private static bool IsInterruptedSwitch(CrossoverState state) {
        return state.Status is "requested" or "switching";
    }

    private static void QueueOotReturnFromClosedMm(string statePath, string handoffPath, CrossoverState state) {
        state.RequestedGame = GameId.Oot;
        state.RequestedEntrance = OotHappyMaskShopReturnAfterFileSelectEntrance;
        state.ReturnEntrance = null;
        state.OriginEntrance = null;
        state.CommandNonce++;
        state.Status = "pending_oot_return";
        state.LastError = null;
        WriteState(statePath, state);
        WriteHandoff(handoffPath, state);
    }

    private static CrossoverState ReadState(string path) {
        for (var attempt = 0; attempt < 5; attempt++) {
            try {
                return JsonSerializer.Deserialize<CrossoverState>(File.ReadAllText(path), JsonOptions) ?? new CrossoverState();
            } catch (IOException) {
                Thread.Sleep(25);
            } catch (JsonException) {
                Thread.Sleep(25);
            }
        }

        return new CrossoverState {
            Status = "error",
            LastError = $"Could not read {path}",
        };
    }

    private static void WriteState(string path, CrossoverState state) {
        state.UpdatedAt = DateTimeOffset.UtcNow;
        var tempPath = path + ".tmp";
        File.WriteAllText(tempPath, JsonSerializer.Serialize(state, JsonOptions));
        File.Move(tempPath, path, overwrite: true);
    }

    private static void WriteHandoff(string path, CrossoverState state) {
        var handoff = new {
            targetGame = state.RequestedGame.ToString(),
            targetEntrance = state.RequestedEntrance,
            returnEntrance = state.ReturnEntrance,
            originEntrance = state.OriginEntrance,
            saveSlot = state.SaveSlot,
            commandNonce = state.CommandNonce,
            updatedAt = DateTimeOffset.UtcNow,
        };
        var tempPath = path + ".tmp";
        File.WriteAllText(tempPath, JsonSerializer.Serialize(handoff, JsonOptions));
        File.Move(tempPath, path, overwrite: true);
    }

    private static string FindProjectRoot(string start) {
        var dir = new DirectoryInfo(start);
        while (dir != null) {
            if (File.Exists(Path.Combine(dir.FullName, "crossover_launcher.json")) ||
                (Directory.Exists(Path.Combine(dir.FullName, "soh")) &&
                 Directory.Exists(Path.Combine(dir.FullName, "2ship")))) {
                return dir.FullName;
            }

            if (Directory.Exists(Path.Combine(dir.FullName, "extracted_win")) &&
                Directory.Exists(Path.Combine(dir.FullName, "source files"))) {
                return dir.FullName;
            }

            dir = dir.Parent;
        }

        return Directory.GetCurrentDirectory();
    }

    private static string Resolve(string root, string path) {
        return Path.GetFullPath(Path.IsPathRooted(path) ? path : Path.Combine(root, path));
    }
}
