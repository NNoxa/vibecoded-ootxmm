# SoH / 2Ship Crossover Launcher

This is the process-switching spine for the Ship of Harkinian / 2Ship2Harkinian crossover prototype.

The launcher starts `extracted_win/soh/soh.exe` first, watches `crossover_state.json`, and switches between `soh.exe` and `2ship.exe` when a port-side hook requests the other game.

## State Protocol

`crossover_state.json` is owned by both the launcher and the game hooks:

```json
{
  "ActiveGame": "Oot",
  "RequestedGame": "Mm",
  "RequestedEntrance": "mm:clock_tower_interior_exit",
  "SaveSlot": 0,
  "CommandNonce": 1,
  "Status": "running",
  "LastError": null,
  "UpdatedAt": "2026-05-09T00:00:00Z"
}
```

The hooks should:

1. Save the current game.
2. Update `requestedGame`, `requestedEntrance`, and increment `commandNonce`.
3. Let the launcher close the current port and start the target port.

The target port should read `crossover_handoff.json` during startup/save-load and force its save slot plus entrance before gameplay begins.

## Entrance Names

Initial planned names:

- `oot:happy_mask_shop_return` starts OoT outside the Happy Mask Shop.
- `oot:happy_mask_shop_return_after_file_select` keeps the OoT title/file select flow, then starts the selected save outside the Happy Mask Shop.
- `oot:title_screen` keeps the OoT title-screen flow without forcing a save entrance.
- `mm:clock_tower_interior_exit` starts MM as if leaving Clock Tower Interior into Clock Town.

The current port patches wire these to `ENTR_MARKET_DAY_OUTSIDE_HAPPY_MASK_SHOP` and
`ENTRANCE(SOUTH_CLOCK_TOWN, 0)`.

## Native Hooks

The prototype hook files live at:

- `source files/Shipwright-9.2.3/soh/soh/Crossover/Crossover.cpp`
- `source files/2ship2harkinian-4.0.2/mm/2s2h/Crossover/Crossover.cpp`

SoH requests MM when Happy Mask Shop loads from the normal shop entrances. 2Ship requests SoH when Clock Tower
Interior loads from the South Clock Town return entrance.

The MM startup handoff currently uses the same debug-save boot path as 2Ship's developer warp point. Replacing that
with the unified crossover save abstraction is the next save-layer step.

## Build

```powershell
dotnet build .\crossover_launcher\CrossoverLauncher.csproj -c Release
```

The executable will be emitted under:

```text
crossover_launcher/bin/Release/net9.0-windows/SoH2ShipCrossover.exe
```
