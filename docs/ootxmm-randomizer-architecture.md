# OoTxMM Randomizer Architecture

## Goal

Build one shared placement layer above the native SoH and 2Ship randomizers. The native menus remain the source of truth for user settings, but OoTxMM generation emits a shared manifest that both games can consume at runtime.

## Current Contract

`crossover_seed.json` is now the handoff point for seed and settings metadata.

```json
{
  "mode": "OoTxMM",
  "seedString": "...",
  "sohSeedString": "...",
  "twoShipSeedString": "...",
  "settings": {
    "oot": {},
    "mm": {},
    "enabledCheckGroups": []
  },
  "mods": {
    "manifests": [],
    "items": [],
    "checks": []
  },
  "placementManifest": {
    "version": 1,
    "format": "ootxmm-shared-placement",
    "status": "settings-snapshot-only",
    "checks": [],
    "items": [],
    "placements": []
  }
}
```

## Native Settings Sources

SoH settings are persisted in:

`extracted_win/soh/shipofharkinian.json`

Path:

`CVars.gRandoSettings`

2Ship settings are persisted in:

`extracted_win/2ship/2ship2harkinian.json`

Path:

`CVars.gRando`

Important 2Ship options live under:

`CVars.gRando.Options`

## Shared Id Shape

Use stable string ids rather than native integer enum values in the shared manifest.

Checks:

```text
oot:<soh location name>
mm:<2ship RandoCheckId enum name>
```

Items:

```text
oot:<soh item name or RandomizerGet name>
mm:<2ship RandoItemId enum name>
shared:<semantic item id>
```

Examples:

```text
oot:GC Pot Freestanding PoH
mm:RC_CLOCK_TOWN_SOUTH_OWL_STATUE
oot:Roc's Feather
mm:RI_OWL_CLOCK_TOWN_SOUTH
shared:heart_piece
```

Modded content should use a globally stable namespace:

```text
mod:<author-or-team>.<mod-id>:<item-or-check-id>
```

Example:

```text
mod:example.magicpack:moon_pearl
```

## Native Spoiler Sources

SoH spoiler logs contain:

`locations`

The values are either strings or objects with item/model metadata.

2Ship spoiler logs contain:

`checks`

The values are either item strings or objects such as:

```json
{ "randoItemId": "RI_BOTTLE_MILK", "price": 66 }
```

## First Placement Strategy

1. Generate native SoH and 2Ship seeds using the user-selected settings.
2. Read both native spoiler outputs.
3. Convert each native check into a canonical `oot:*` or `mm:*` check id.
4. Convert each native item into a canonical item id.
5. Pool enabled checks from both games according to `settings.enabledCheckGroups`.
6. Shuffle the combined item pool across the combined check pool.
7. Write `placementManifest.placements`.
8. Runtime override layers in each game check the manifest first. If a local check has a foreign item, grant it through the crossover item adapter.

`tools/Build-CrossoverPlacementPreview.ps1` now implements steps 2-7 for the current preview layer. It writes:

```text
crossover_placements_preview.json
crossover_placements.json
```

It also updates `crossover_seed.json.placementManifest` unless run with `-NoSeedManifestUpdate`.

The placement manifest records one row per enabled shuffled check:

```json
{
  "check": "oot:Market Potion Shop Item 6",
  "checkGame": "oot",
  "checkGroup": "pots",
  "nativeCheck": "Market Potion Shop Item 6",
  "item": "mm:RI_ARROW_LIGHT",
  "itemSourceGame": "mm",
  "itemSourceCheck": "mm:RC_TERMINA_FIELD_POT",
  "model": "mm:RI_ARROW_LIGHT",
  "price": null,
  "isCrossGame": true
}
```

Current algorithm:

```text
stable-sha256-sort-v1
```

This is deterministic for a given OoTxMM seed string and source spoiler pair. It is not final logic validation; it is the first shared placement table for runtime adapter work.

## Important Runtime Work

SoH needs:

- Manifest reader on save load.
- Check override lookup before native item grant.
- Foreign MM item grant adapter.
- Foreign MM item model/icon fallback for visible checks.

2Ship needs:

- Manifest reader on save load.
- Check override lookup before native item grant.
- Foreign OoT item grant adapter.
- Foreign OoT item model/icon fallback for visible checks.

## Model Display Notes

Models should be handled separately from item ownership. The manifest should carry both:

```json
{
  "item": "mm:RI_OWL_CLOCK_TOWN_SOUTH",
  "model": "mm:RI_OWL_CLOCK_TOWN_SOUTH"
}
```

For early playtests, unknown foreign models can intentionally fall back to a neutral visible model while the grant behavior is implemented first.

The preview builder now resolves cross-game model ids into the shared namespace:

- `xshared.*` for models that both games can represent naturally, like heart pieces, bottles, rupees, masks shared by both games, and common songs.
- `xmm.*` for Majora's Mask-native models that need to render inside SoH, like stray fairies, owl statues, MM-only masks, and MM-only quest items.
- `xoot.*` for Ocarina of Time-native models that need to render inside 2Ship, like Roc's Feather, OoT hookshot/longshot, medallions, spiritual stones, and spells.

The staging manifest lives at `shared_assets/ootxmm/manifest.ootxmm-assets.json` and is copied into both release ports under `mods/OoTxMM/`. Renderer hooks should read `SharedPlacement.model` through each port's crossover model lookup and then resolve that id to either a native draw id or a locally generated display list.

## Mod Compatibility

The ports can load asset archives from their `mods` folders, but release packages must not ship ROM-derived archives or extracted model payloads. OoTxMM therefore uses an opt-in sidecar manifest for randomizer-aware metadata while any required game asset payload is generated locally from the player's ROMs.

Supported manifest locations:

```text
extracted_win/soh/mods/*.ootxmm.json
extracted_win/2ship/mods/*.ootxmm.json
```

Manifest format:

```json
{
  "format": "ootxmm-mod",
  "version": 1,
  "modId": "author.mod_name",
  "displayName": "Readable Mod Name",
  "game": "oot",
  "items": [
    {
      "id": "mod:author.mod_name:item_id",
      "name": "Readable Item Name",
      "pool": { "enabledBy": true, "count": 1 },
      "native": { "oot": "native item id if one exists" },
      "models": { "oot": "native model id", "mm": "fallback or custom model id" },
      "grant": { "oot": "native grant id", "mm": "adapter grant id" }
    }
  ],
  "checks": [
    {
      "id": "mod:author.mod_name:check_id",
      "game": "oot",
      "group": "pots",
      "native": "native check id if one exists"
    }
  ]
}
```

Rules:

- A mod item enters the shared item pool only when its manifest says it is poolable.
- If a mod item can only be granted inside its native game, it should either be marked local-only or provide a foreign-game adapter in `grant`.
- If a mod adds visible pickups, chest contents, shops, or other modeled checks, it should provide `models` data. Unknown foreign models can still fall back to the neutral model while grant behavior is being wired.
- Code-mod forks can also generate this manifest at runtime or ship it beside their archive; the important part is the stable ids.

The preview builder already scans these manifests and reports discovered mod items/checks, so this gives us a testable path before we wire the runtime grant adapters.
