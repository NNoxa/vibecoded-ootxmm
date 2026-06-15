param(
    [string]$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"

$patches = @(
    @{
        Repo = "source files/Shipwright-9.2.3/libultraship"
        Patch = "tools/patches/shipwright-libultraship-crossover.patch"
    }
)

foreach ($entry in $patches) {
    $repoPath = Join-Path $Root $entry.Repo
    $patchPath = Join-Path $Root $entry.Patch

    if (-not (Test-Path -LiteralPath $repoPath)) {
        throw "Submodule path not found: $repoPath"
    }

    if (-not (Test-Path -LiteralPath $patchPath)) {
        throw "Patch not found: $patchPath"
    }

    git -C $repoPath apply --check $patchPath 2>$null
    if ($LASTEXITCODE -eq 0) {
        git -C $repoPath apply $patchPath
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to apply patch: $patchPath"
        }
        Write-Host "Applied $($entry.Patch)"
        continue
    }

    git -C $repoPath apply --reverse --check $patchPath 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Already applied $($entry.Patch)"
        continue
    }

    throw "Patch does not apply cleanly and does not appear to be applied already: $patchPath"
}
