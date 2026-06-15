param(
    [string]$Root = (Resolve-Path "$PSScriptRoot\..").Path,
    [string]$Output = "",
    [switch]$NoSeedManifestUpdate,
    [switch]$TestingModelSeed
)

$ErrorActionPreference = "Stop"

function Read-JsonFile {
    param([string]$Path)

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Get-SoHDir {
    param([string]$Root)

    $release = Join-Path $Root "soh"
    if (Test-Path -LiteralPath $release) {
        return $release
    }

    return Join-Path $Root "extracted_win\soh"
}

function Get-2ShipDir {
    param([string]$Root)

    $release = Join-Path $Root "2ship"
    if (Test-Path -LiteralPath $release) {
        return $release
    }

    return Join-Path $Root "extracted_win\2ship"
}

function Get-JsonInt {
    param($Object, [string]$Name)

    if ($null -eq $Object) {
        return 0
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return 0
    }

    try {
        return [int]$property.Value
    } catch {
        return 0
    }
}

function Get-CVarIntOrNull {
    param($Config, [string]$Name)

    if ($null -eq $Config -or $null -eq $Config.CVars) {
        return $null
    }

    $flat = $Config.CVars.PSObject.Properties[$Name]
    if ($null -ne $flat) {
        try { return [int]$flat.Value } catch { return $null }
    }

    $current = $Config.CVars
    foreach ($part in $Name.Split('.')) {
        if ($null -eq $current) {
            return $null
        }

        $property = $current.PSObject.Properties[$part]
        if ($null -eq $property) {
            return $null
        }
        $current = $property.Value
    }

    try { return [int]$current } catch { return $null }
}

function Resolve-SoHSpoilerPath {
    param([string]$Root)

    $sohDir = Get-SoHDir $Root
    $configPath = Join-Path $sohDir "shipofharkinian.json"
    $config = Read-JsonFile $configPath
    $configured = $config.CVars.gGeneral.SpoilerLog
    if ($configured) {
        $candidate = Join-Path $sohDir $configured
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $randomizerDir = Join-Path $sohDir "Randomizer"
    if (-not (Test-Path -LiteralPath $randomizerDir)) {
        return $null
    }

    return Get-ChildItem -LiteralPath $randomizerDir -Filter "*.json" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

function Resolve-2ShipSpoilerPath {
    param([string]$Root)

    $twoShipDir = Get-2ShipDir $Root
    $configPath = Join-Path $twoShipDir "2ship2harkinian.json"
    $config = Read-JsonFile $configPath
    $configured = $config.CVars.gRando.SpoilerFile
    if ($configured) {
        $candidate = Join-Path $twoShipDir $configured
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $randomizerDir = Join-Path $twoShipDir "randomizer"
    if (-not (Test-Path -LiteralPath $randomizerDir)) {
        return $null
    }

    return Get-ChildItem -LiteralPath $randomizerDir -Filter "*.json" |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1 -ExpandProperty FullName
}

function Get-CheckGroup {
    param([string]$Game, [string]$NativeId)

    $name = $NativeId.ToLowerInvariant()

    if ($Game -eq "mm") {
        if ($name -match "owl_statue") { return "owl_statues" }
        if ($name -match "stray_fairy") { return "stray_fairies" }
        if ($name -match "grass") { return "grass" }
        if ($name -match "pot") { return "pots" }
        if ($name -match "crate") { return "crates" }
        if ($name -match "barrel") { return "barrels" }
        if ($name -match "cow") { return "cows" }
        if ($name -match "tingle.*shop|tingle.*map") { return "tingle_maps" }
        if ($name -match "shop") { return "shops" }
        if ($name -match "skulltula|skull_token|gs_token") { return "tokens" }
        if ($name -match "frog") { return "frogs" }
        if ($name -match "snowball") { return "snowballs" }
        if ($name -match "boss.*remain|remains") { return "boss_remains" }
        if ($name -match "boss.*soul|soul_boss") { return "boss_souls" }
        if ($name -match "enemy.*soul|soul_enemy") { return "enemy_souls" }
        if ($name -match "enemy.*drop") { return "enemy_drops" }
        if ($name -match "freestanding|piece_of_heart|scrub") { return "freestanding" }
        return "other"
    }

    if ($name -match "frogs.*(rain|ocarina game)") { return "frog_song_rupees" }
    if ($name -match "fishing pole") { return "fishing_pole" }
    if ($name -match "kokiri sword") { return "kokiri_sword" }
    if ($name -match "master sword") { return "master_sword" }
    if ($name -match "ocarina") { return "ocarinas" }
    if ($name -match "weird egg") { return "weird_egg" }
    if ($name -match "gerudo membership|gerudo card|gerudo token") { return "gerudo_card" }
    if ($name -match "adult trade|biggoron|claim check|eyedrops|eyeball frog|prescription|broken goron|poacher|odd mushroom|odd potion|cojiro|pocket cucco") { return "adult_trade" }
    if ($name -match "100 gs|100 skull|100 gold skulltula") { return "hundred_gs_reward" }
    if ($name -match "gossip stone.*fairy") { return "gossip_stone_fairies" }
    if ($name -match "fairy grotto fairy|shield grave fairy|scrubs fairy|fountain fairy") { return "fountain_fairies" }
    if ($name -match "sun.*fairy|storms.*fairy|song.*fairy|fairy spot") { return "fairy_spots" }
    if ($name -match "^song from| song$") { return "songs" }
    if ($name -match "\bgs\b|skulltula") { return "tokens" }
    if ($name -match "diving minigame|lab dive|skull kid|ocarina memory game|target in woods|deku theater|bombchu bowling|horseback archery|dampe|10 big poes|big poe|dog lady|richard|man on roof|frog|darunia|saria|ocarina of time|windmill|royal family|impa|malon") { return "npc_rewards" }
    if ($name -match "fish|loach") { return "fishsanity" }
    if ($name -match "shop item|bazaar item") { return "shopsanity" }
    if ($name -match "deku scrub") { return "scrubs" }
    if ($name -match "merchant|medigoron|carpet salesman|bean seller") { return "merchants" }
    if ($name -match "\bbush\b| bushes| bush ") { return "bushes" }
    if ($name -match "\btree\b| trees| tree ") { return "trees" }
    if ($name -match "map chest|compass chest") { return "maps_compasses" }
    if ($name -match "small key|maze path|maze right|maze left|gtg") { return "small_keys" }
    if ($name -match "boss key") { return "boss_keys" }
    if ($name -match "heart container|queen gohma|king dodongo|barinade|phantom ganon|volvagia|morpha|bongo bongo|twinrova") { return "dungeon_rewards" }
    if ($name -match "gerudo fortress.*key|thieves hideout.*key|carpenter") { return "gerudo_fortress_keys" }
    if ($name -match "boss soul|soul") { return "boss_souls" }
    if ($name -match "key ring|keyring") { return "keyrings" }
    if ($name -match "beehive") { return "beehives" }
    if ($name -match "grass") { return "grass" }
    if ($name -match "pot") { return "pots" }
    if ($name -match "crate") { return "crates" }
    if ($name -match "cow") { return "cows" }
    if ($name -match "freestanding") { return "freestanding" }
    if ($name -match "scrub|merchant|shop|song|boss|token|skulltula|fountain fairy") { return "major" }
    return "other"
}

function Test-IsDungeonCheck {
    param([string]$Game, [string]$NativeId)

    $name = $NativeId.ToLowerInvariant()
    if ($Game -eq "mm") {
        return ($name -match "woodfall temple|snowhead temple|great bay temple|stone tower temple|temple_|_temple|moon trial")
    }

    return ($name -match "deku tree|dodongo|jabu|forest temple|fire temple|water temple|spirit temple|shadow temple|bottom of the well|ice cavern|gerudo training|gtg|ganon.*castle|ganon's castle")
}

function Get-CheckArea {
    param([string]$Game, [string]$NativeId)

    $name = $NativeId.ToLowerInvariant()
    if ($name -match "woodfall temple") { return "woodfall_temple" }
    if ($name -match "snowhead temple") { return "snowhead_temple" }
    if ($name -match "great bay temple") { return "great_bay_temple" }
    if ($name -match "stone tower") { return "stone_tower_temple" }
    if ($name -match "clock town") { return "clock_town" }
    if ($name -match "termina field") { return "termina_field" }
    if ($name -match "southern swamp") { return "southern_swamp" }
    if ($name -match "deku palace") { return "deku_palace" }
    if ($name -match "woodfall") { return "woodfall" }
    if ($name -match "mountain village") { return "mountain_village" }
    if ($name -match "goron village") { return "goron_village" }
    if ($name -match "snowhead") { return "snowhead" }
    if ($name -match "romani ranch") { return "romani_ranch" }
    if ($name -match "milk road") { return "milk_road" }
    if ($name -match "great bay") { return "great_bay" }
    if ($name -match "pirate|pirates") { return "pirates_fortress" }
    if ($name -match "zora") { return "zora_hall" }
    if ($name -match "ikana canyon") { return "ikana_canyon" }
    if ($name -match "ikana graveyard") { return "ikana_graveyard" }
    if ($name -match "moon|trial") { return "moon" }
    if ($name -match "deku tree") { return "deku_tree" }
    if ($name -match "dodongo") { return "dodongos_cavern" }
    if ($name -match "jabu") { return "jabu_jabus_belly" }
    if ($name -match "^song from windmill") { return "kakariko_village" }
    if ($name -match "^song from saria") { return "lost_woods" }
    if ($name -match "^song from royal familys tomb|^song from composer") { return "graveyard" }
    if ($name -match "^song from ocarina of time") { return "market" }
    if ($name -match "forest temple|phantom ganon") { return "forest_temple" }
    if ($name -match "water temple|morpha") { return "water_temple" }
    if ($name -match "shadow temple|bongo bongo") { return "shadow_temple" }
    if ($name -match "bottom of the well") { return "bottom_of_the_well" }
    if ($name -match "ice cavern") { return "ice_cavern" }
    if ($name -match "gerudo training|gtg") { return "gerudo_training_ground" }
    if ($name -match "ganon.*castle|ganon's castle") { return "ganons_castle" }
    if ($name -match "fire temple|volvagia") { return "fire_temple" }
    if ($name -match "spirit temple|twinrova") { return "spirit_temple" }
    if ($name -match "kokiri forest|^kf\b") { return "kokiri_forest" }
    if ($name -match "sacred forest|^sfm\b") { return "sacred_forest_meadow" }
    if ($name -match "lost woods|^lw\b") { return "lost_woods" }
    if ($name -match "hyrule field|^hf\b") { return "hyrule_field" }
    if ($name -match "market") { return "market" }
    if ($name -match "kakariko|^kak\b") { return "kakariko_village" }
    if ($name -match "graveyard") { return "graveyard" }
    if ($name -match "death mountain crater|^dmc\b") { return "death_mountain_crater" }
    if ($name -match "death mountain|^dmt\b") { return "death_mountain_trail" }
    if ($name -match "goron city") { return "goron_city" }
    if ($name -match "zoras river|zora's river") { return "zoras_river" }
    if ($name -match "zoras domain|zora's domain") { return "zoras_domain" }
    if ($name -match "zoras fountain|zora's fountain|^zf\b") { return "zoras_fountain" }
    if ($name -match "lake hylia") { return "lake_hylia" }
    if ($name -match "lon lon|^llr\b") { return "lon_lon_ranch" }
    if ($name -match "gerudo valley|^gv\b") { return "gerudo_valley" }
    if ($name -match "gerudo fortress") { return "gerudo_fortress" }
    if ($name -match "desert colossus") { return "desert_colossus" }
    return ($NativeId -replace "[^A-Za-z0-9]+.*$", "").ToLowerInvariant()
}

function Convert-ToHintTitle {
    param([string]$Raw)

    if ([string]::IsNullOrWhiteSpace($Raw)) {
        return "Unknown"
    }

    $text = $Raw
    if ($text.Contains(":")) {
        $text = $text.Substring($text.IndexOf(":") + 1)
    }

    $text = $text -replace "^(RI|RG|RC)_", ""
    $text = $text -creplace "([a-z])([A-Z])", '$1 $2'
    $text = $text -replace "[_\-]+", " "
    $text = $text -replace "\s+", " "
    $text = $text.Trim()

    $specials = @{
        "SONG BOLERO OF FIRE" = "Bolero of Fire"
        "BOLERO OF FIRE" = "Bolero of Fire"
        "SONG MINUET OF FOREST" = "Minuet of Forest"
        "MINUET OF FOREST" = "Minuet of Forest"
        "SONG SERENADE OF WATER" = "Serenade of Water"
        "SERENADE OF WATER" = "Serenade of Water"
        "SONG REQUIEM OF SPIRIT" = "Requiem of Spirit"
        "REQUIEM OF SPIRIT" = "Requiem of Spirit"
        "SONG NOCTURNE OF SHADOW" = "Nocturne of Shadow"
        "NOCTURNE OF SHADOW" = "Nocturne of Shadow"
        "SONG PRELUDE OF LIGHT" = "Prelude of Light"
        "PRELUDE OF LIGHT" = "Prelude of Light"
        "SONG EPONA" = "Epona's Song"
        "EPONAS SONG" = "Epona's Song"
        "SONG TIME" = "Song of Time"
        "SONG OF TIME" = "Song of Time"
        "SONG STORMS" = "Song of Storms"
        "SONG OF STORMS" = "Song of Storms"
        "SUNS SONG" = "Sun's Song"
        "SARIAS SONG" = "Saria's Song"
        "ZELDAS LULLABY" = "Zelda's Lullaby"
        "SONG SOARING" = "Song of Soaring"
        "SONG SONATA" = "Sonata of Awakening"
        "SONG LULLABY" = "Goron Lullaby"
        "SONG NOVA" = "New Wave Bossa Nova"
        "SONG ELEGY" = "Elegy of Emptiness"
        "SONG OATH" = "Oath to Order"
        "SONG" = "Ocarina Songs"
        "SONGS" = "Ocarina Songs"
        "PIRATES FORTRESS" = "Pirates' Fortress"
        "ZORAS DOMAIN" = "Zora's Domain"
        "ZORAS FOUNTAIN" = "Zora's Fountain"
        "ZF" = "Zora's Fountain"
        "ZORAS RIVER" = "Zora's River"
        "JABU JABUS BELLY" = "Jabu-Jabu's Belly"
        "DODONGOS CAVERN" = "Dodongo's Cavern"
        "GANONS CASTLE" = "Ganon's Castle"
        "LON LON RANCH" = "Lon Lon Ranch"
        "LAKE HYLIA" = "Lake Hylia"
        "LH" = "Lake Hylia"
        "KOKIRI FOREST" = "Kokiri Forest"
        "KF" = "Kokiri Forest"
        "LOST WOODS" = "Lost Woods"
        "LW" = "Lost Woods"
        "SACRED FOREST MEADOW" = "Sacred Forest Meadow"
        "SFM" = "Sacred Forest Meadow"
        "HYRULE FIELD" = "Hyrule Field"
        "HF" = "Hyrule Field"
        "DEATH MOUNTAIN TRAIL" = "Death Mountain Trail"
        "DMT" = "Death Mountain Trail"
        "DEATH MOUNTAIN CRATER" = "Death Mountain Crater"
        "DMC" = "Death Mountain Crater"
        "KAK" = "Kakariko"
        "GERUDO VALLEY" = "Gerudo Valley"
        "GV" = "Gerudo Valley"
        "GERUDO FORTRESS" = "Gerudo Fortress"
        "GF" = "Gerudo Fortress"
        "CLOCK TOWN" = "Clock Town"
        "TERMINA FIELD" = "Termina Field"
    }

    $upper = $text.ToUpperInvariant()
    if ($specials.ContainsKey($upper)) {
        return $specials[$upper]
    }

    $culture = [System.Globalization.CultureInfo]::InvariantCulture
    $title = $culture.TextInfo.ToTitleCase($text.ToLowerInvariant())
    $title = $title -replace "\bOot\b", "OoT"
    $title = $title -replace "\bMm\b", "MM"
    $title = $title -replace "\bGs\b", "Gold Skulltula"
    $title = $title -replace "\bGtg\b", "GTG"
    $title = $title -replace "\bHp\b", "HP"
    $title = $title -replace "^Kf\b", "Kokiri Forest"
    $title = $title -replace "^Lw\b", "Lost Woods"
    $title = $title -replace "^Sfm\b", "Sacred Forest Meadow"
    $title = $title -replace "^Hf\b", "Hyrule Field"
    $title = $title -replace "^Lh\b", "Lake Hylia"
    $title = $title -replace "^Dmt\b", "Death Mountain Trail"
    $title = $title -replace "^Dmc\b", "Death Mountain Crater"
    $title = $title -replace "^Kak\b", "Kakariko"
    $title = $title -replace "^Llr\b", "Lon Lon Ranch"
    $title = $title -replace "^Gv\b", "Gerudo Valley"
    $title = $title -replace "^Gf\b", "Gerudo Fortress"
    $title = $title -replace "^Zd\b", "Zora's Domain"
    $title = $title -replace "^Zf\b", "Zora's Fountain"
    $title = $title -replace "^Zr\b", "Zora's River"
    $title = $title -replace "\bPo H\b", "Piece of Heart"
    return $title
}

function Get-HintGameSuffix {
    param([string]$Game)

    if ($Game -eq "oot") { return "OoT" }
    if ($Game -eq "mm") { return "MM" }
    return $Game
}

function Get-HintItemName {
    param($Item)

    $name = Convert-ToHintTitle $Item.id
    return "$name ($(Get-HintGameSuffix $Item.sourceGame))"
}

function Get-HintPlaceName {
    param($Check)

    $name = Convert-ToHintTitle $Check.nativeId
    return "$name ($(Get-HintGameSuffix $Check.game))"
}

function Get-HintAreaName {
    param($Check)

    $area = if ($Check.area) { $Check.area } else { Get-CheckArea $Check.game $Check.nativeId }
    $name = Convert-ToHintTitle $area
    $fullName = "$name ($(Get-HintGameSuffix $Check.game))"
    $renames = @{
        "Hc (OoT)" = "Castle Grounds"
        "Ganon's Castle (OoT)" = "Ganon's Castle"
        "Moon (OoT)" = "Ganon's Castle"
        "Ocarina Songs (OoT)" = "Child Song Checks (OoT)"
        "Rc (MM)" = "Ikana Castle"
        "Sheik (OoT)" = "Finding Bolero of Fire"
        "Zora's Domain (OoT)" = "Zora's Domain"
        "Zora Hall (OoT)" = "Zora's Domain"
    }

    if ($renames.ContainsKey($fullName)) {
        return $renames[$fullName]
    }

    return $fullName
}

function Test-HintMajorItem {
    param($Item, [string]$ModelId)

    if ($Item.isTrap) {
        return $false
    }

    return ((Get-CrossoverItemTier $Item.id $ModelId) -eq "major")
}

function Get-CheckPlacementTags {
    param([string]$Game, [string]$NativeId, [string]$Group)

    $name = $NativeId.ToLowerInvariant()
    $tags = @($Group)
    if (Test-IsDungeonCheck $Game $NativeId) { $tags += "dungeon" } else { $tags += "overworld" }
    if ($name -match "volvagia|fire temple.*(heart container|boss|reward)") { $tags += "volvagia_reward" }
    if ($name -match "twinrova|spirit temple.*(heart container|boss|reward)") { $tags += "twinrova_reward" }
    if ($name -match "ganon.*castle|ganon's castle") { $tags += "ganons_castle" }
    if ($Game -eq "mm" -and $name -match "woodfall temple") { $tags += "woodfall_temple" }
    if ($Game -eq "mm" -and $name -match "snowhead temple") { $tags += "snowhead_temple" }
    if ($Game -eq "mm" -and $name -match "great bay temple") { $tags += "great_bay_temple" }
    if ($Game -eq "mm" -and $name -match "moon|trial") { $tags += "moon" }
    return @($tags | Select-Object -Unique)
}

function Get-CheckSourceSetting {
    param([string]$Game, [string]$Group)

    if ($Game -eq "mm") {
        switch ($Group) {
            "grass" { return "RO_SHUFFLE_GRASS_DROPS" }
            "pots" { return "RO_SHUFFLE_POT_DROPS" }
            "crates" { return "RO_SHUFFLE_CRATE_DROPS" }
            "barrels" { return "RO_SHUFFLE_BARREL_DROPS" }
            "owl_statues" { return "RO_SHUFFLE_OWL_STATUES" }
            "cows" { return "RO_SHUFFLE_COWS" }
            "freestanding" { return "RO_SHUFFLE_FREESTANDING_ITEMS" }
            "shops" { return "RO_SHUFFLE_SHOPS" }
            "tingle_maps" { return "RO_SHUFFLE_TINGLE_SHOPS" }
            "tokens" { return "RO_SHUFFLE_GOLD_SKULLTULAS" }
            "stray_fairies" { return "RO_STRAY_FAIRIES_MAX" }
            "frogs" { return "RO_SHUFFLE_FROGS" }
            "snowballs" { return "RO_SHUFFLE_SNOWBALL_DROPS" }
            "boss_remains" { return "RO_SHUFFLE_BOSS_REMAINS" }
            "boss_souls" { return "RO_SHUFFLE_BOSS_SOULS" }
            "enemy_drops" { return "RO_SHUFFLE_ENEMY_DROPS" }
            "enemy_souls" { return "RO_SHUFFLE_ENEMY_SOULS" }
            default { return $null }
        }
    }

    switch ($Group) {
        "grass" { return "ShuffleGrass" }
        "pots" { return "ShufflePots" }
        "crates" { return "ShuffleCrates" }
        "fishsanity" { return "Fishsanity" }
        "shopsanity" { return "Shopsanity" }
        "scrubs" { return "ShuffleScrubs" }
        "tokens" { return "ShuffleTokens" }
        default { return $Group }
    }
}

function Convert-SoHLocations {
    param($Spoiler)

    $checks = @()
    foreach ($property in $Spoiler.locations.PSObject.Properties) {
        $value = $property.Value
        $item = $value
        $model = $null
        if ($value -is [pscustomobject]) {
            $item = $value.item
            $model = $value.model
        }

        $group = Get-CheckGroup "oot" $property.Name
        $checks += [pscustomobject]@{
            id = "oot:$($property.Name)"
            game = "oot"
            nativeId = $property.Name
            group = $group
            type = $group
            area = Get-CheckArea "oot" $property.Name
            isDungeon = Test-IsDungeonCheck "oot" $property.Name
            sourceSetting = Get-CheckSourceSetting "oot" $group
            placementTags = Get-CheckPlacementTags "oot" $property.Name $group
            item = "oot:$item"
            model = if ($model) { "oot:$model" } else { $null }
        }
    }

    return $checks
}

function Convert-2ShipChecks {
    param($Spoiler)

    $checks = @()
    foreach ($property in $Spoiler.checks.PSObject.Properties) {
        $value = $property.Value
        $item = $value
        $price = $null
        if ($value -is [pscustomobject]) {
            $item = $value.randoItemId
            $price = $value.price
        }

        $group = Get-CheckGroup "mm" $property.Name
        $checks += [pscustomobject]@{
            id = "mm:$($property.Name)"
            game = "mm"
            nativeId = $property.Name
            group = $group
            type = $group
            area = Get-CheckArea "mm" $property.Name
            isDungeon = Test-IsDungeonCheck "mm" $property.Name
            sourceSetting = Get-CheckSourceSetting "mm" $group
            placementTags = Get-CheckPlacementTags "mm" $property.Name $group
            item = "mm:$item"
            price = $price
        }
    }

    return $checks
}

function Read-ModManifests {
    param([string]$ModsPath, [string]$Game)

    if (-not (Test-Path -LiteralPath $ModsPath)) {
        return @()
    }

    $manifests = @()
    $files = Get-ChildItem -LiteralPath $ModsPath -Recurse -File -Filter "*.json" |
        Where-Object { $_.Name -like "*.ootxmm.json" -or $_.Name -like "*ootxmm*.json" }

    foreach ($file in $files) {
        try {
            $manifest = Read-JsonFile $file.FullName
            if ($manifest.format -ne "ootxmm-mod") {
                continue
            }

            $manifest | Add-Member -NotePropertyName sourceGame -NotePropertyValue $Game -Force
            $manifest | Add-Member -NotePropertyName sourcePath -NotePropertyValue $file.FullName -Force
            $manifests += $manifest
        } catch {
            Write-Warning "Skipping invalid OoTxMM mod manifest: $($file.FullName)"
        }
    }

    return $manifests
}

function Build-ModCatalog {
    param([string]$Root)

    $manifests = @()
    $manifests += Read-ModManifests (Join-Path (Get-SoHDir $Root) "mods") "oot"
    $manifests += Read-ModManifests (Join-Path (Get-2ShipDir $Root) "mods") "mm"

    $items = @()
    $modChecks = @()
    foreach ($manifest in $manifests) {
        $modId = $manifest.modId
        if (-not $modId) {
            $modId = "unknown"
        }

        foreach ($item in @($manifest.items)) {
            if ($null -eq $item) {
                continue
            }

            $item | Add-Member -NotePropertyName modId -NotePropertyValue $modId -Force
            $item | Add-Member -NotePropertyName sourceGame -NotePropertyValue $manifest.sourceGame -Force
            $items += $item
        }

        foreach ($check in @($manifest.checks)) {
            if ($null -eq $check) {
                continue
            }

            $check | Add-Member -NotePropertyName modId -NotePropertyValue $modId -Force
            $check | Add-Member -NotePropertyName sourceGame -NotePropertyValue $manifest.sourceGame -Force
            $modChecks += $check
        }
    }

    return [pscustomobject]@{
        manifests = $manifests
        items = $items
        checks = $modChecks
    }
}

function Build-EnabledCheckGroups {
    param($SeedManifest, [string]$Root)

    $sohConfig = Read-JsonFile (Join-Path (Get-SoHDir $Root) "shipofharkinian.json")
    $twoShipConfig = Read-JsonFile (Join-Path (Get-2ShipDir $Root) "2ship2harkinian.json")
    $ootSettings = $sohConfig.CVars.gRandoSettings
    $mmOptions = $twoShipConfig.CVars.gRando.Options

    $groups = @()
    $smallKeys = Get-JsonInt $ootSettings "Keysanity"
    $bossKeys = Get-JsonInt $ootSettings "BossKeysanity"
    $mapsCompasses = Get-JsonInt $ootSettings "StartingMapsCompasses"
    $gerudoKeys = Get-JsonInt $ootSettings "GerudoKeys"
    $dungeonRewards = Get-JsonInt $ootSettings "ShuffleDungeonRewards"
    $keyrings = Get-JsonInt $ootSettings "ShuffleKeyRings"
    $comboStrayFairies = Get-CVarIntOrNull $sohConfig "gRandomizer.Combo.Items.MM.StrayFairies"
    if ($null -eq $comboStrayFairies) {
        $comboStrayFairies = Get-CVarIntOrNull $twoShipConfig "gRandomizer.Combo.Items.MM.StrayFairies"
    }
    $comboEnemySouls = Get-CVarIntOrNull $sohConfig "gRandomizer.Combo.Shuffles.MM.EnemySouls"
    if ($null -eq $comboEnemySouls) {
        $comboEnemySouls = Get-CVarIntOrNull $twoShipConfig "gRandomizer.Combo.Shuffles.MM.EnemySouls"
    }
    $comboShops = Get-CVarIntOrNull $sohConfig "gRandomizer.Combo.Shuffles.Shops"
    if ($null -eq $comboShops) {
        $comboShops = Get-CVarIntOrNull $twoShipConfig "gRandomizer.Combo.Shuffles.Shops"
    }
    $mmStrayFairies = Get-JsonInt $mmOptions "RO_STRAY_FAIRIES_MAX"
    if ($null -ne $comboStrayFairies) {
        $mmStrayFairies = if ($comboStrayFairies -ne 0) { 15 } else { 0 }
    }
    $mmEnemySouls = Get-JsonInt $mmOptions "RO_SHUFFLE_ENEMY_SOULS"
    if ($null -ne $comboEnemySouls) {
        $mmEnemySouls = if ($comboEnemySouls -ne 0) { 1 } else { 0 }
    }
    $mmTingleShops = Get-JsonInt $mmOptions "RO_SHUFFLE_TINGLE_SHOPS"
    if ($null -ne $comboShops) {
        $mmTingleShops = if ($comboShops -ne 0) { 1 } else { 0 }
    }
    $candidates = @(
        @("oot", "grass", "ShuffleGrass", (Get-JsonInt $ootSettings "ShuffleGrass")),
        @("oot", "pots", "ShufflePots", (Get-JsonInt $ootSettings "ShufflePots")),
        @("oot", "crates", "ShuffleCrates", (Get-JsonInt $ootSettings "ShuffleCrates")),
        @("oot", "beehives", "ShuffleBeehives", (Get-JsonInt $ootSettings "ShuffleBeehives")),
        @("oot", "cows", "ShuffleCows", (Get-JsonInt $ootSettings "ShuffleCows")),
        @("oot", "freestanding", "ShuffleFreestanding", (Get-JsonInt $ootSettings "ShuffleFreestanding")),
        @("oot", "fishing_pole", "ShuffleFishingPole", (Get-JsonInt $ootSettings "ShuffleFishingPole")),
        @("oot", "roc_feather", "RocsFeather", (Get-JsonInt $ootSettings "RocsFeather")),
        @("oot", "kokiri_sword", "ShuffleKokiriSword", (Get-JsonInt $ootSettings "ShuffleKokiriSword")),
        @("oot", "master_sword", "ShuffleMasterSword", (Get-JsonInt $ootSettings "ShuffleMasterSword")),
        @("oot", "ocarinas", "ShuffleOcarinas", (Get-JsonInt $ootSettings "ShuffleOcarinas")),
        @("oot", "ocarina_buttons", "ShuffleOcarinaButtons", (Get-JsonInt $ootSettings "ShuffleOcarinaButtons")),
        @("oot", "weird_egg", "ShuffleWeirdEgg", (Get-JsonInt $ootSettings "ShuffleWeirdEgg")),
        @("oot", "gerudo_card", "ShuffleGerudoToken", (Get-JsonInt $ootSettings "ShuffleGerudoToken")),
        @("oot", "frog_song_rupees", "ShuffleFrogSongRupees", (Get-JsonInt $ootSettings "ShuffleFrogSongRupees")),
        @("oot", "adult_trade", "ShuffleAdultTrade", (Get-JsonInt $ootSettings "ShuffleAdultTrade")),
        @("oot", "hundred_gs_reward", "Shuffle100GSReward", (Get-JsonInt $ootSettings "Shuffle100GSReward")),
        @("oot", "fountain_fairies", "ShuffleFountainFairies", (Get-JsonInt $ootSettings "ShuffleFountainFairies")),
        @("oot", "gossip_stone_fairies", "ShuffleStoneFairies", (Get-JsonInt $ootSettings "ShuffleStoneFairies")),
        @("oot", "fairy_spots", "ShuffleFairySpots", (Get-JsonInt $ootSettings "ShuffleFairySpots")),
        @("oot", "songs", "ShuffleSongs", (Get-JsonInt $ootSettings "ShuffleSongs")),
        @("oot", "tokens", "ShuffleTokens", (Get-JsonInt $ootSettings "ShuffleTokens")),
        @("oot", "npc_rewards", "NPCRewards", 1),
        @("oot", "fishsanity", "Fishsanity", (Get-JsonInt $ootSettings "Fishsanity")),
        @("oot", "shopsanity", "Shopsanity", (Get-JsonInt $ootSettings "Shopsanity")),
        @("oot", "scrubs", "ShuffleScrubs", (Get-JsonInt $ootSettings "ShuffleScrubs")),
        @("oot", "merchants", "ShuffleMerchants", (Get-JsonInt $ootSettings "ShuffleMerchants")),
        @("oot", "bombchu_bag", "BombchuBag", (Get-JsonInt $ootSettings "BombchuBag")),
        @("oot", "infinite_upgrades", "InfiniteUpgrades", (Get-JsonInt $ootSettings "InfiniteUpgrades")),
        @("oot", "skeleton_key", "SkeletonKey", (Get-JsonInt $ootSettings "SkeletonKey")),
        @("oot", "maps_compasses", "StartingMapsCompasses", $(if ($mapsCompasses -ge 2) { $mapsCompasses } else { 0 })),
        @("oot", "small_keys", "Keysanity", $(if ($smallKeys -ge 2) { $smallKeys } else { 0 })),
        @("oot", "boss_keys", "BossKeysanity", $(if ($bossKeys -ge 2) { $bossKeys } else { 0 })),
        @("oot", "dungeon_rewards", "ShuffleDungeonRewards", $(if ($dungeonRewards -ne 0) { $dungeonRewards } else { 0 })),
        @("oot", "gerudo_fortress_keys", "GerudoKeys", $gerudoKeys),
        @("oot", "boss_souls", "ShuffleBossSouls", (Get-JsonInt $ootSettings "ShuffleBossSouls")),
        @("oot", "keyrings", "ShuffleKeyRings", $keyrings),
        @("mm", "grass", "RO_SHUFFLE_GRASS_DROPS", (Get-JsonInt $mmOptions "RO_SHUFFLE_GRASS_DROPS")),
        @("mm", "pots", "RO_SHUFFLE_POT_DROPS", (Get-JsonInt $mmOptions "RO_SHUFFLE_POT_DROPS")),
        @("mm", "crates", "RO_SHUFFLE_CRATE_DROPS", (Get-JsonInt $mmOptions "RO_SHUFFLE_CRATE_DROPS")),
        @("mm", "barrels", "RO_SHUFFLE_BARREL_DROPS", (Get-JsonInt $mmOptions "RO_SHUFFLE_BARREL_DROPS")),
        @("mm", "owl_statues", "RO_SHUFFLE_OWL_STATUES", (Get-JsonInt $mmOptions "RO_SHUFFLE_OWL_STATUES")),
        @("mm", "cows", "RO_SHUFFLE_COWS", (Get-JsonInt $mmOptions "RO_SHUFFLE_COWS")),
        @("mm", "freestanding", "RO_SHUFFLE_FREESTANDING_ITEMS", (Get-JsonInt $mmOptions "RO_SHUFFLE_FREESTANDING_ITEMS")),
        @("mm", "shops", "RO_SHUFFLE_SHOPS", (Get-JsonInt $mmOptions "RO_SHUFFLE_SHOPS")),
        @("mm", "tingle_maps", "RO_SHUFFLE_TINGLE_SHOPS", $mmTingleShops),
        @("mm", "tokens", "RO_SHUFFLE_GOLD_SKULLTULAS", (Get-JsonInt $mmOptions "RO_SHUFFLE_GOLD_SKULLTULAS")),
        @("mm", "stray_fairies", "RO_STRAY_FAIRIES_MAX", $mmStrayFairies),
        @("mm", "snowballs", "RO_SHUFFLE_SNOWBALL_DROPS", (Get-JsonInt $mmOptions "RO_SHUFFLE_SNOWBALL_DROPS")),
        @("mm", "boss_remains", "RO_SHUFFLE_BOSS_REMAINS", (Get-JsonInt $mmOptions "RO_SHUFFLE_BOSS_REMAINS")),
        @("mm", "boss_souls", "RO_SHUFFLE_BOSS_SOULS", (Get-JsonInt $mmOptions "RO_SHUFFLE_BOSS_SOULS")),
        @("mm", "enemy_drops", "RO_SHUFFLE_ENEMY_DROPS", (Get-JsonInt $mmOptions "RO_SHUFFLE_ENEMY_DROPS")),
        @("mm", "enemy_souls", "RO_SHUFFLE_ENEMY_SOULS", $mmEnemySouls)
    )

    foreach ($candidate in $candidates) {
        if ($candidate[3] -eq 0) {
            continue
        }

        $groups += [pscustomobject]@{
            game = $candidate[0]
            group = $candidate[1]
            setting = $candidate[2]
            value = $candidate[3]
        }
    }

    return $groups
}

function Get-EnabledTrapPolicy {
    param([string]$Root)

    $sohConfig = Read-JsonFile (Join-Path (Get-SoHDir $Root) "shipofharkinian.json")
    $twoShipConfig = Read-JsonFile (Join-Path (Get-2ShipDir $Root) "2ship2harkinian.json")
    $ootSettings = $sohConfig.CVars.gRandoSettings
    $mmOptions = $twoShipConfig.CVars.gRando.Options

    return [pscustomobject]@{
        ootBaseIceTraps = (Get-JsonInt $ootSettings "BaseIceTraps")
        ootAdditionalIceTraps = (Get-JsonInt $ootSettings "AdditionalIceTraps")
        ootIceTrapPercent = (Get-JsonInt $ootSettings "IceTrapPercent")
        mmShuffleTraps = (Get-JsonInt $mmOptions "RO_SHUFFLE_TRAPS")
        mmTrapAmount = (Get-JsonInt $mmOptions "RO_TRAP_AMOUNT")
    }
}

function Test-TrapEnabledForItem {
    param($Item, $TrapPolicy)

    if (-not $Item.isTrap) {
        return $true
    }

    switch ("$($Item.sourceGame)") {
        "oot" {
            return ([int]$TrapPolicy.ootBaseIceTraps -ne 0) -or
                   ([int]$TrapPolicy.ootAdditionalIceTraps -gt 0) -or
                   ([int]$TrapPolicy.ootIceTrapPercent -gt 0)
        }
        "mm" {
            return ([int]$TrapPolicy.mmShuffleTraps -ne 0) -and
                   ([int]$TrapPolicy.mmTrapAmount -gt 0)
        }
        default {
            return $true
        }
    }
}

function Test-CheckEnabled {
    param($Check, $EnabledGroups)

    foreach ($group in @($EnabledGroups)) {
        if ($group.game -eq $Check.game -and $group.group -eq $Check.group) {
            if ($Check.game -eq "oot" -and $Check.group -in @("grass", "pots", "crates", "freestanding", "tokens")) {
                switch ([int]$group.value) {
                    1 { return [bool]$Check.isDungeon }
                    2 { return -not [bool]$Check.isDungeon }
                    3 { return $true }
                    default { return $true }
                }
            }

            if ($Check.game -eq "oot" -and $Check.group -eq "scrubs") {
                if ([int]$group.value -eq 1) {
                    return ($Check.nativeId -match "LW Deku Scrub Grotto Front|LW Deku Scrub Near Bridge|HF Deku Scrub Grotto")
                }
            }

            if ($Check.game -eq "oot" -and $Check.group -eq "fishsanity") {
                switch ([int]$group.value) {
                    1 {
                        return ($Check.nativeId -match "Loach")
                    }
                    2 {
                        return ($Check.nativeId -match "Pond|Fishing")
                    }
                    3 {
                        return ($Check.nativeId -match "Grotto Fish|ZD Fish")
                    }
                    4 {
                        return $true
                    }
                    default {
                        return $false
                    }
                }
            }

            if ($Check.game -eq "oot" -and $Check.group -eq "merchants") {
                switch ([int]$group.value) {
                    1 {
                        return ($Check.nativeId -match "bean")
                    }
                    2 {
                        return ($Check.nativeId -notmatch "bean")
                    }
                    3 {
                        return $true
                    }
                    default {
                        return $false
                    }
                }
            }

            return $true
        }
    }

    return $false
}

function Test-OoTxMmModelTestCheck {
    param($Check)

    if ($Check.game -eq "oot" -and $Check.nativeId -match "^Market Guard House Child Pot ") {
        return $true
    }

    if ($Check.game -eq "mm" -and $Check.nativeId -match "^RC_TERMINA_FIELD_.*GRASS_") {
        return $true
    }

    return $false
}

function Get-StableHash {
    param([string]$Text)

    $sha = [System.Security.Cryptography.SHA256]::Create()
    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        return [System.BitConverter]::ToString($sha.ComputeHash($bytes)).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha.Dispose()
    }
}

function Get-StableHashInt {
    param([string]$Text)

    $hash = Get-StableHash $Text
    return [uint32][Convert]::ToUInt32($hash.Substring(0, 8), 16)
}

function Test-ShopPriceCheck {
    param($Check)

    return "$($Check.group)" -in @("shopsanity", "shops", "tingle_maps", "scrubs", "merchants")
}

function Get-PlacementPrice {
    param(
        [string]$SeedString,
        $Check,
        $Item,
        $SettingValues
    )

    if (-not (Test-ShopPriceCheck $Check)) {
        return $Item.price
    }

    $affordable = $false
    if ($SettingValues.ContainsKey("ShopsAffordablePrices")) {
        $affordable = ([int]$SettingValues["ShopsAffordablePrices"] -ne 0)
    }

    $maxPrice = 99
    if (-not $affordable) {
        $maxPrice = 500
        if ($Check.game -eq "oot" -and $SettingValues.ContainsKey("IncludeTycoonWallet") -and
            [int]$SettingValues["IncludeTycoonWallet"] -ne 0) {
            $maxPrice = 999
        }
    }

    $hashInput = "$SeedString|shop-price|$($Check.game)|$($Check.group)|$($Check.id)|$($Check.nativeId)"
    return [int]((Get-StableHashInt $hashInput) % ($maxPrice + 1))
}

function Get-ObjectPropertyValue {
    param($Object, [string]$Name)

    if ($null -eq $Object) {
        return $null
    }

    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }

    return $property.Value
}

function Convert-ToModelSlug {
    param([string]$Text)

    if (-not $Text) {
        return ""
    }

    return $Text.ToLowerInvariant().
        Replace("oot:", "").
        Replace("mm:", "").
        Replace("ri_", "").
        Replace("rg_", "").
        Replace("(", "").
        Replace(")", "").
        Replace("'", "").
        Replace("&", "and").
        Replace("+", "plus").
        Replace(" ", "_").
        Replace("-", "_")
}

function Resolve-SharedModelId {
    param([string]$ItemId)

    $item = $ItemId.ToUpperInvariant()
    $itemKey = $item.Replace(" ", "_").Replace("-", "_")

    if ($itemKey -match "HEART_PIECE|PIECE_OF_HEART") { return "xshared.heart_piece" }
    if ($itemKey -match "HEART_CONTAINER") { return "xshared.heart_container" }
    if ($itemKey -match "DOUBLE_DEFENSE") { return "xshared.double_defense" }
    if ($itemKey -match "OCARINA(_)?A(_)?BUTTON|OCARINA_BUTTON.*A|BUTTON_A|A_BUTTON") { return "xshared.ocarina_button.a" }
    if ($itemKey -match "OCARINA(_)?C(_)?UP(_)?BUTTON|OCARINA_BUTTON.*C_UP|BUTTON_C_UP|C_UP_BUTTON") { return "xshared.ocarina_button.c_up" }
    if ($itemKey -match "OCARINA(_)?C(_)?LEFT(_)?BUTTON|OCARINA_BUTTON.*C_LEFT|BUTTON_C_LEFT|C_LEFT_BUTTON") { return "xshared.ocarina_button.c_left" }
    if ($itemKey -match "OCARINA(_)?C(_)?DOWN(_)?BUTTON|OCARINA_BUTTON.*C_DOWN|BUTTON_C_DOWN|C_DOWN_BUTTON") { return "xshared.ocarina_button.c_down" }
    if ($itemKey -match "OCARINA(_)?C(_)?RIGHT(_)?BUTTON|OCARINA_BUTTON.*C_RIGHT|BUTTON_C_RIGHT|C_RIGHT_BUTTON") { return "xshared.ocarina_button.c_right" }
    if ($itemKey -match "FIRE_ARROWS|FIRE_ARROW|ARROW_FIRE") { return "xshared.arrows.fire" }
    if ($itemKey -match "ICE_ARROWS|ICE_ARROW|ARROW_ICE") { return "xshared.arrows.ice" }
    if ($itemKey -match "LIGHT_ARROWS|LIGHT_ARROW|ARROW_LIGHT") { return "xshared.arrows.light" }
    if ($itemKey -match "BOMB_BAG") { return "xshared.bomb_bag" }
    if ($itemKey -match "ARROW|ARROWS") { return "xshared.arrows" }
    if ($itemKey -match "BOMBCHU") { return "xshared.bombchu" }
    if ($itemKey -match "BOMB") { return "xshared.bombs" }
    if ($itemKey -match "MAGIC_BEAN|BEANS") { return "xshared.magic_beans" }
    if ($itemKey -match "LENS_OF_TRUTH|LENS") { return "xshared.lens" }
    if ($itemKey -match "PROGRESSIVE_WALLET|WALLET") { return "xshared.wallet" }
    if ($itemKey -match "MAGIC.*LARGE|LARGE.*MAGIC|MAGIC_JAR_BIG|BIG_MAGIC") { return "xshared.magic_jar_large" }
    if ($itemKey -match "MAGIC") { return "xshared.magic_jar_small" }
    if ($itemKey -match "RECOVERY_HEART|HEART$|HEART_") { return "xshared.recovery_heart" }
    if ($itemKey -match "SILVER_RUPEE|RUPEE_SILVER") { return "xshared.rupee.silver" }
    if ($itemKey -match "HUGE_RUPEE|RUPEE_HUGE") { return "xshared.rupee.huge" }
    if ($itemKey -match "PURPLE_RUPEE|RUPEE_PURPLE") { return "xshared.rupee.purple" }
    if ($itemKey -match "RED_RUPEE|RUPEE_RED") { return "xshared.rupee.red" }
    if ($itemKey -match "BLUE_RUPEE|RUPEE_BLUE") { return "xshared.rupee.blue" }
    if ($itemKey -match "GREEN_RUPEE|RUPEE_GREEN") { return "xshared.rupee.green" }
    if ($itemKey -match "RUPEE") { return "xshared.rupee" }
    if ($itemKey -match "DEKU_SEEDS|SEEDS") { return "xshared.deku_seeds" }
    if ($itemKey -match "DEKU_NUT|NUTS") { return "xshared.deku_nuts" }
    if ($itemKey -match "DEKU_STICK|STICKS") { return "xshared.deku_sticks" }
    if ($itemKey -match "BOTTLE.*EMPTY|EMPTY_BOTTLE") { return "xshared.bottle.empty" }
    if ($itemKey -match "BOTTLE.*BUG|BUG") { return "xshared.bottle.bug" }
    if ($itemKey -match "BOTTLE.*FISH|FISH") { return "xshared.bottle.fish" }
    if ($itemKey -match "BOTTLE.*FAIRY|FAIRY_BOTTLE|FAIRY_REFILL") { return "xshared.bottle.fairy" }
    if ($itemKey -match "HALF_MILK") { return "xshared.bottle.half_milk" }
    if ($itemKey -match "BOTTLE.*MILK|MILK_BOTTLE|RI_BOTTLE_MILK") { return "xshared.bottle.milk" }
    if ($itemKey -match "RED_POTION") { return "xshared.bottle.red_potion" }
    if ($itemKey -match "BLUE_POTION") { return "xshared.bottle.blue_potion" }
    if ($itemKey -match "SMALL_POE") { return "xshared.bottle.small_poe" }
    if ($itemKey -match "BIG_POE|BOTTLE_WITH_A_POE|BOTTLE_WITH_POE") { return "xshared.bottle.big_poe" }
    if ($itemKey -match "MASK_GORON|GORON_MASK") { return "xshared.mask.goron" }
    if ($itemKey -match "MASK_ZORA|ZORA_MASK") { return "xshared.mask.zora" }
    if ($itemKey -match "MASK_KEATON|KEATON_MASK") { return "xshared.mask.keaton" }
    if ($itemKey -match "MASK_TRUTH|MASK_OF_TRUTH|TRUTH_MASK") { return "xshared.mask.truth" }
    if ($itemKey -match "MASK_BUNNY|BUNNY_HOOD") { return "xshared.mask.bunny" }
    if ($itemKey -match "OCARINA") { return "xshared.ocarina_of_time" }
    if ($itemKey -match "SONG_OF_TIME|SONG_TIME") { return "xshared.song.time" }
    if ($itemKey -match "SUNS_SONG|SUN_SONG") { return "xshared.song.sun" }
    if ($itemKey -match "SARIAS_SONG|SARIA_SONG") { return "xshared.song.saria" }
    if ($itemKey -match "EPONAS_SONG|EPONA_SONG") { return "xshared.song.epona" }
    if ($itemKey -match "SONG_OF_STORMS|STORMS") { return "xshared.song.storms" }

    return $null
}

function Get-LogicalItemKey {
    param($Item)

    $id = "$($Item.id)".ToUpperInvariant().Replace(" ", "_").Replace("-", "_")
    if ($id -match "MASK_GORON|GORON_MASK") { return "mask:goron" }
    if ($id -match "MASK_ZORA|ZORA_MASK") { return "mask:zora" }
    if ($id -match "MASK_TRUTH|MASK_OF_TRUTH|TRUTH_MASK") { return "mask:truth" }
    if ($id -match "MASK_BUNNY|BUNNY_HOOD") { return "mask:bunny" }
    if ($id -match "MASK_KEATON|KEATON_MASK") { return "mask:keaton" }
    if ($id -match "MASK_DEKU|DEKU_MASK") { return "mask:deku" }
    if ($id -match "MASK_FIERCE_DEITY|FIERCE_DEITY_MASK") { return "mask:fierce_deity" }
    if ($id -match "MASK_SPOOKY|SPOOKY_MASK") { return "mask:oot:spooky" }
    if ($id -match "MASK_SKULL|SKULL_MASK") { return "mask:oot:skull" }
    if ($id -match "MASK_GERUDO|GERUDO_MASK") { return "mask:oot:gerudo" }
    if ($id -match "OCARINA(_)?A(_)?BUTTON|OCARINA_BUTTON.*A|BUTTON_A|A_BUTTON") { return "ocarina_button:a" }
    if ($id -match "OCARINA(_)?C(_)?UP(_)?BUTTON|OCARINA_BUTTON.*C_UP|BUTTON_C_UP|C_UP_BUTTON") { return "ocarina_button:c_up" }
    if ($id -match "OCARINA(_)?C(_)?LEFT(_)?BUTTON|OCARINA_BUTTON.*C_LEFT|BUTTON_C_LEFT|C_LEFT_BUTTON") { return "ocarina_button:c_left" }
    if ($id -match "OCARINA(_)?C(_)?DOWN(_)?BUTTON|OCARINA_BUTTON.*C_DOWN|BUTTON_C_DOWN|C_DOWN_BUTTON") { return "ocarina_button:c_down" }
    if ($id -match "OCARINA(_)?C(_)?RIGHT(_)?BUTTON|OCARINA_BUTTON.*C_RIGHT|BUTTON_C_RIGHT|C_RIGHT_BUTTON") { return "ocarina_button:c_right" }

    return "$($Item.sourceGame):$($Item.id):$($Item.sourceCheck):$($Item.sourceGroup)"
}

function Get-LogicalItemPreference {
    param($Item)

    $logicalKey = Get-LogicalItemKey $Item
    if ($logicalKey -match "^mask:(goron|zora|truth|bunny|keaton|deku|fierce_deity)$") {
        if ($Item.sourceGame -eq "mm") {
            return 0
        }
        return 1
    }

    return 0
}

function Remove-DuplicateLogicalItems {
    param($Items)

    $deduped = @()
    $groups = @($Items | Group-Object { Get-LogicalItemKey $_ })
    foreach ($group in $groups) {
        $deduped += @($group.Group | Sort-Object @{ Expression = { Get-LogicalItemPreference $_ } },
                                           @{ Expression = { "$($_.sourceGame)" } },
                                           @{ Expression = { "$($_.id)" } },
                                           @{ Expression = { "$($_.sourceCheck)" } } |
            Select-Object -First 1)
    }

    return $deduped
}

function Get-CrossoverItemTier {
    param([string]$ItemId, [string]$ModelId)

    $item = "$ItemId $ModelId".ToUpperInvariant().Replace(" ", "_").Replace("-", "_")
    if ($item -match "HEART_PIECE|PIECE_OF_HEART|HEART_CONTAINER|DOUBLE_DEFENSE") { return "heart" }
    if ($item -match "NONE|NOTHING|NO_ITEM|^UNKNOWN$") { return "unknown" }
    if ($item -match "HUGE_RUPEE|RUPEE_HUGE") { return "quest" }
    if ($item -match "ARROWS?_\(?[0-9]+|BOMBS?_\(?[0-9]+|BOMBCHU_\(?[0-9]+") { return "junk" }
    if ($item -match "SILVER_RUPEE|RUPEE_SILVER") { return "major" }
    if ($item -match "RI_PROGRESSIVE_MAGIC|PROGRESSIVE_MAGIC_METER|PROGRESSIVE_MAGIC|GREG_THE_GREEN_RUPEE") { return "major" }
    if ($item -match "STRAY_FAIRY|MAP|COMPASS|DEED|LETTER|PENDANT|MOONS_TEAR|MOONS_TEAR|NOTEBOOK|ROOM_KEY|SKULLTULA|GS_TOKEN") {
        return "quest"
    }
    if ($item -match "FIRE_ARROWS|ICE_ARROWS|LIGHT_ARROWS|SONG_OF_TIME|SUNS_SONG|SARIAS_SONG|EPONAS_SONG|SONG_OF_STORMS|MINUET|BOLERO|SERENADE|REQUIEM|NOCTURNE|PRELUDE|LULLABY|OATH|SOARING|SONATA|ELEGY|NOVA|BUTTON_A|C_UP_BUTTON|C_DOWN_BUTTON|C_LEFT_BUTTON|C_RIGHT_BUTTON|OCARINA_BUTTON|MASK|SOUL|REMAINS|BOSS_KEY|SMALL_KEY|KEY_RING|SKELETON_KEY|HOOKSHOT|LONGSHOT|BOW|SLINGSHOT|SHIELD|SWORD|LENS|STONE_OF_AGONY|WALLET|BOMB_BAG|QUIVER|BULLET_BAG|OCARINA|SONG|MEDALLION|SPIRITUAL_STONE|KOKIRI_EMERALD|GORON_RUBY|ZORA_SAPPHIRE|ROCS_FEATHER|TUNIC|BOOTS|GAUNTLET|BRACELET|STRENGTH|SCALE|SPELL|DINS_FIRE|NAYRUS_LOVE|FARORES_WIND|MEMBERSHIP|GERUDO_CARD|GERUDO_MEMBERSHIP|CAPACITY|UPGRADE|MAGIC_BEAN") {
        return "major"
    }
    if ($item -match "RUPEE|ARROWS?\(|BOMBS?\(|BOMBCHU\(|MAGIC_JAR|RECOVERY_HEART|DEKU_NUT|DEKU_STICK|DEKU_SEED|REFILL|JUNK|BUY_DEKU|BUY_BOMB|BUY_ARROW|BUY_HEART") {
        return "junk"
    }
    return "major"
}

function Get-ItemSourceSetting {
    param([string]$Game, [string]$ItemId)

    $item = "$ItemId".ToUpperInvariant().Replace(" ", "_").Replace("-", "_")

    if ($Game -eq "mm") {
        if ($item -match "STRAY_FAIRY") { return "RO_STRAY_FAIRIES_MAX" }
        if ($item -match "OWL") { return "RO_SHUFFLE_OWL_STATUES" }
        if ($item -match "TINGLE.*MAP") { return "RO_SHUFFLE_TINGLE_SHOPS" }
        if ($item -match "REMAINS_|_REMAINS|ODOLWA|GOHT|GYORG|TWINMOLD") { return "RO_SHUFFLE_BOSS_REMAINS" }
        if ($item -match "SOUL_BOSS|BOSS_SOUL") { return "RO_SHUFFLE_BOSS_SOULS" }
        if ($item -match "SOUL_ENEMY|ENEMY_SOUL") { return "RO_SHUFFLE_ENEMY_SOULS" }
        if ($item -match "OCARINA(_)?A(_)?BUTTON|OCARINA_BUTTON|BUTTON_A|BUTTON_C_") { return "RO_SHUFFLE_OCARINA_BUTTONS" }
        if ($item -match "SONG_SUN|SUNS_SONG|SUN_SONG") { return "RO_SHUFFLE_SONG_SUN" }
        if ($item -match "SONG_SARIA|SARIAS_SONG|SARIA_SONG") { return "RO_SHUFFLE_SONG_SARIA" }
        if ($item -match "TIME_NIGHT_|TIME_DAY_|PROGRESSIVE_TIME|CLOCK_") { return "RO_CLOCK_SHUFFLE" }
        if ($item -match "TRAP") { return "RO_SHUFFLE_TRAPS" }
        return $null
    }

    if ($item -match "SOUL") { return "ShuffleBossSouls" }
    if ($item -match "OCARINA(_)?A(_)?BUTTON|OCARINA_BUTTON|BUTTON_A|BUTTON_C_") { return "ShuffleOcarinaButtons" }
    if ($item -match "ROCS_FEATHER|ROC") { return "RocsFeather" }
    if ($item -match "FISHING_POLE") { return "ShuffleFishingPole" }
    if ($item -match "KOKIRI_SWORD") { return "ShuffleKokiriSword" }
    if ($item -match "MASTER_SWORD") { return "ShuffleMasterSword" }
    if ($item -match "OCARINA" -and $item -notmatch "SONG|BUTTON") { return "ShuffleOcarinas" }
    if ($item -match "WEIRD_EGG") { return "ShuffleWeirdEgg" }
    if ($item -match "GERUDO_MEMBERSHIP|GERUDO_CARD|GERUDO_TOKEN") { return "ShuffleGerudoToken" }
    if ($item -match "BOMBCHU_BAG") { return "BombchuBag" }
    if ($item -match "TYCOON") { return "IncludeTycoonWallet" }
    if ($item -match "SKELETON_KEY") { return "SkeletonKey" }
    if ($item -match "MAP|COMPASS") { return "StartingMapsCompasses" }
    if ($item -match "SMALL_KEY|KEY_RING") { return "Keysanity" }
    if ($item -match "BOSS_KEY") { return "BossKeysanity" }
    return $null
}

function Build-SettingValueMap {
    param([string]$Root)

    $sohConfig = Read-JsonFile (Join-Path (Get-SoHDir $Root) "shipofharkinian.json")
    $twoShipConfig = Read-JsonFile (Join-Path (Get-2ShipDir $Root) "2ship2harkinian.json")
    $ootSettings = $sohConfig.CVars.gRandoSettings
    $mmOptions = $twoShipConfig.CVars.gRando.Options

    $comboStrayFairies = Get-CVarIntOrNull $sohConfig "gRandomizer.Combo.Items.MM.StrayFairies"
    if ($null -eq $comboStrayFairies) {
        $comboStrayFairies = Get-CVarIntOrNull $twoShipConfig "gRandomizer.Combo.Items.MM.StrayFairies"
    }
    $comboEnemySouls = Get-CVarIntOrNull $sohConfig "gRandomizer.Combo.Shuffles.MM.EnemySouls"
    if ($null -eq $comboEnemySouls) {
        $comboEnemySouls = Get-CVarIntOrNull $twoShipConfig "gRandomizer.Combo.Shuffles.MM.EnemySouls"
    }
    $comboShopAffordablePrices = Get-CVarIntOrNull $sohConfig "gRandomizer.Combo.Shuffles.ShopsAffordablePrices"
    if ($null -eq $comboShopAffordablePrices) {
        $comboShopAffordablePrices = Get-CVarIntOrNull $twoShipConfig "gRandomizer.Combo.Shuffles.ShopsAffordablePrices"
    }
    $mmStrayFairies = Get-JsonInt $mmOptions "RO_STRAY_FAIRIES_MAX"
    if ($null -ne $comboStrayFairies) {
        $mmStrayFairies = if ($comboStrayFairies -ne 0) { 15 } else { 0 }
    }
    $mmEnemySouls = Get-JsonInt $mmOptions "RO_SHUFFLE_ENEMY_SOULS"
    if ($null -ne $comboEnemySouls) {
        $mmEnemySouls = if ($comboEnemySouls -ne 0) { 1 } else { 0 }
    }
    $shopAffordablePrices = Get-JsonInt $ootSettings "ShopsanityPricesAffordable"
    if ($null -ne $comboShopAffordablePrices) {
        $shopAffordablePrices = $comboShopAffordablePrices
    }

    return @{
        ShuffleBossSouls = (Get-JsonInt $ootSettings "ShuffleBossSouls")
        ShuffleOcarinaButtons = (Get-JsonInt $ootSettings "ShuffleOcarinaButtons")
        RocsFeather = (Get-JsonInt $ootSettings "RocsFeather")
        ShuffleFishingPole = (Get-JsonInt $ootSettings "ShuffleFishingPole")
        ShuffleKokiriSword = (Get-JsonInt $ootSettings "ShuffleKokiriSword")
        ShuffleMasterSword = (Get-JsonInt $ootSettings "ShuffleMasterSword")
        ShuffleOcarinas = (Get-JsonInt $ootSettings "ShuffleOcarinas")
        ShuffleWeirdEgg = (Get-JsonInt $ootSettings "ShuffleWeirdEgg")
        ShuffleGerudoToken = (Get-JsonInt $ootSettings "ShuffleGerudoToken")
        BombchuBag = (Get-JsonInt $ootSettings "BombchuBag")
        IncludeTycoonWallet = (Get-JsonInt $ootSettings "IncludeTycoonWallet")
        ShopsAffordablePrices = $shopAffordablePrices
        SkeletonKey = (Get-JsonInt $ootSettings "SkeletonKey")
        StartingMapsCompasses = (Get-JsonInt $ootSettings "StartingMapsCompasses")
        Keysanity = (Get-JsonInt $ootSettings "Keysanity")
        BossKeysanity = (Get-JsonInt $ootSettings "BossKeysanity")
        RO_STRAY_FAIRIES_MAX = $mmStrayFairies
        RO_SHUFFLE_OWL_STATUES = (Get-JsonInt $mmOptions "RO_SHUFFLE_OWL_STATUES")
        RO_SHUFFLE_TINGLE_SHOPS = (Get-JsonInt $mmOptions "RO_SHUFFLE_TINGLE_SHOPS")
        RO_SHUFFLE_BOSS_REMAINS = (Get-JsonInt $mmOptions "RO_SHUFFLE_BOSS_REMAINS")
        RO_SHUFFLE_BOSS_SOULS = (Get-JsonInt $mmOptions "RO_SHUFFLE_BOSS_SOULS")
        RO_SHUFFLE_ENEMY_SOULS = $mmEnemySouls
        RO_SHUFFLE_OCARINA_BUTTONS = (Get-JsonInt $mmOptions "RO_SHUFFLE_OCARINA_BUTTONS")
        RO_SHUFFLE_SONG_SUN = (Get-JsonInt $mmOptions "RO_SHUFFLE_SONG_SUN")
        RO_SHUFFLE_SONG_SARIA = (Get-JsonInt $mmOptions "RO_SHUFFLE_SONG_SARIA")
        RO_CLOCK_SHUFFLE = (Get-JsonInt $mmOptions "RO_CLOCK_SHUFFLE")
        RO_SHUFFLE_TRAPS = (Get-JsonInt $mmOptions "RO_SHUFFLE_TRAPS")
    }
}

function Test-ItemSettingEnabled {
    param($Item, $SettingValues)

    $setting = Get-ItemSourceSetting $Item.sourceGame $Item.id
    if (-not $setting) {
        return $true
    }

    $value = 0
    if ($SettingValues.ContainsKey($setting)) {
        $value = [int]$SettingValues[$setting]
    }

    switch ($setting) {
        "StartingMapsCompasses" { return ($value -ge 2) }
        "Keysanity" { return ($value -ge 2) }
        "BossKeysanity" { return ($value -ge 2) }
        "BombchuBag" { return ($value -gt 0) }
        default { return ($value -ne 0) }
    }
}

function New-CrossoverStandaloneItem {
    param(
        [string]$ItemId,
        [string]$SourceGroup,
        [string]$SourceCheck
    )

    $modelId = Resolve-ForeignModelId $ItemId "mm"
    $itemSourceSetting = Get-ItemSourceSetting "mm" $ItemId
    return [pscustomobject]@{
        id = $ItemId
        nativeId = $ItemId
        logicalId = $null
        sharedId = $null
        modelId = $modelId
        tier = Get-CrossoverItemTier $ItemId $modelId
        isTrap = ($ItemId -match "TRAP")
        isShared = ([bool](Resolve-SharedModelId $ItemId))
        sourceSetting = $itemSourceSetting
        sourceCheck = $SourceCheck
        sourceGame = "mm"
        sourceGroup = $SourceGroup
        model = $null
        price = $null
    }
}

function Add-StandaloneMmPoolItems {
    param($Items, $SettingValues, $TrapPolicy)

    $result = @($Items)

    $enemySouls = @(
        "RI_SOUL_ENEMY_ALIEN",
        "RI_SOUL_ENEMY_ARMOS",
        "RI_SOUL_ENEMY_BAD_BAT",
        "RI_SOUL_ENEMY_BEAMOS",
        "RI_SOUL_ENEMY_BOE",
        "RI_SOUL_ENEMY_BUBBLE",
        "RI_SOUL_ENEMY_CAPTAIN_KEETA",
        "RI_SOUL_ENEMY_CHUCHU",
        "RI_SOUL_ENEMY_DEATH_ARMOS",
        "RI_SOUL_ENEMY_DEEP_PYTHON",
        "RI_SOUL_ENEMY_DEKU_BABA",
        "RI_SOUL_ENEMY_DEXIHAND",
        "RI_SOUL_ENEMY_DINOLFOS",
        "RI_SOUL_ENEMY_DODONGO",
        "RI_SOUL_ENEMY_DRAGONFLY",
        "RI_SOUL_ENEMY_EENO",
        "RI_SOUL_ENEMY_EYEGORE",
        "RI_SOUL_ENEMY_FREEZARD",
        "RI_SOUL_ENEMY_GARO",
        "RI_SOUL_ENEMY_GEKKO",
        "RI_SOUL_ENEMY_GIANT_BEE",
        "RI_SOUL_ENEMY_GOMESS",
        "RI_SOUL_ENEMY_GUAY",
        "RI_SOUL_ENEMY_HIPLOOP",
        "RI_SOUL_ENEMY_IGOS_DU_IKANA",
        "RI_SOUL_ENEMY_IRON_KNUCKLE",
        "RI_SOUL_ENEMY_KEESE",
        "RI_SOUL_ENEMY_LEEVER",
        "RI_SOUL_ENEMY_LIKE_LIKE",
        "RI_SOUL_ENEMY_MAD_SCRUB",
        "RI_SOUL_ENEMY_NEJIRON",
        "RI_SOUL_ENEMY_OCTOROK",
        "RI_SOUL_ENEMY_PEAHAT",
        "RI_SOUL_ENEMY_PIRATE",
        "RI_SOUL_ENEMY_POE",
        "RI_SOUL_ENEMY_REDEAD",
        "RI_SOUL_ENEMY_SHELLBLADE",
        "RI_SOUL_ENEMY_SKULLFISH",
        "RI_SOUL_ENEMY_SKULLTULA",
        "RI_SOUL_ENEMY_SNAPPER",
        "RI_SOUL_ENEMY_STALCHILD",
        "RI_SOUL_ENEMY_TAKKURI",
        "RI_SOUL_ENEMY_TEKTITE",
        "RI_SOUL_ENEMY_WALLMASTER",
        "RI_SOUL_ENEMY_WART",
        "RI_SOUL_ENEMY_WIZROBE",
        "RI_SOUL_ENEMY_WOLFOS"
    )
    foreach ($enemySoul in $enemySouls) {
        $existing = @($result | Where-Object { $_.sourceGame -eq "mm" -and $_.id -eq $enemySoul }).Count
        if ($existing -gt 0) {
            continue
        }

        $candidateItem = New-CrossoverStandaloneItem $enemySoul "enemy_souls" "standalone:$enemySoul"
        if ((Test-ItemSettingEnabled $candidateItem $SettingValues) -and (Test-TrapEnabledForItem $candidateItem $TrapPolicy)) {
            $result += $candidateItem
        }
    }

    $strayFairyMax = 0
    if ($SettingValues.ContainsKey("RO_STRAY_FAIRIES_MAX")) {
        $strayFairyMax = [Math]::Max(0, [int]$SettingValues["RO_STRAY_FAIRIES_MAX"])
    }
    if ($strayFairyMax -gt 0) {
        foreach ($strayFairy in @("RI_WOODFALL_STRAY_FAIRY", "RI_SNOWHEAD_STRAY_FAIRY", "RI_GREAT_BAY_STRAY_FAIRY", "RI_STONE_TOWER_STRAY_FAIRY")) {
            $existing = @($result | Where-Object { $_.sourceGame -eq "mm" -and $_.id -eq $strayFairy }).Count
            for ($i = $existing; $i -lt $strayFairyMax; $i++) {
                $candidateItem = New-CrossoverStandaloneItem $strayFairy "stray_fairies" "standalone:$($strayFairy):$i"
                if ((Test-ItemSettingEnabled $candidateItem $SettingValues) -and (Test-TrapEnabledForItem $candidateItem $TrapPolicy)) {
                    $result += $candidateItem
                }
            }
        }
    }

    return $result
}

function Resolve-ForeignModelId {
    param([string]$ItemId, [string]$SourceGame)

    $slug = Convert-ToModelSlug $ItemId
    $item = $ItemId.ToUpperInvariant()

    if ($SourceGame -eq "mm") {
        if ($item -match "HOOKSHOT") { return "xmm.unique.hookshot" }
        if ($item -match "PROGRESSIVE_BOW|BOW") { return "xmm.unique.progressive_bow" }
        if ($item -match "HERO.*SHIELD|SHIELD_HERO") { return "xmm.unique.hero_shield" }
        if ($item -match "MIRROR.*SHIELD|SHIELD_MIRROR") { return "xmm.unique.mirror_shield" }
        if ($item -match "PROGRESSIVE_BOMB_BAG|BOMB_BAG") { return "xmm.unique.progressive_bomb_bag" }
    }

    $shared = Resolve-SharedModelId $ItemId
    if ($shared) {
        return $shared
    }

    if ($SourceGame -eq "mm") {
        if ($item -match "STRAY_FAIRY") { return "xmm.stray_fairy" }
        if ($item -match "OCARINA_BUTTON.*A|BUTTON_A") { return "xmm.ocarina_button.a" }
        if ($item -match "OCARINA_BUTTON.*C_UP|BUTTON_C_UP") { return "xmm.ocarina_button.c_up" }
        if ($item -match "OCARINA_BUTTON.*C_LEFT|BUTTON_C_LEFT") { return "xmm.ocarina_button.c_left" }
        if ($item -match "OCARINA_BUTTON.*C_DOWN|BUTTON_C_DOWN") { return "xmm.ocarina_button.c_down" }
        if ($item -match "OCARINA_BUTTON.*C_RIGHT|BUTTON_C_RIGHT") { return "xmm.ocarina_button.c_right" }
        if ($item -match "OWL") { return "xmm.owl_statue" }
        if ($item -match "GS_TOKEN|SKULLTULA") { return "xmm.skulltula_token" }
        if ($item -match "FROG_CYAN|CYAN.*FROG") { return "xmm.frog.cyan" }
        if ($item -match "FROG_PINK|PINK.*FROG") { return "xmm.frog.pink" }
        if ($item -match "FROG_WHITE|WHITE.*FROG") { return "xmm.frog.white" }
        if ($item -match "FROG_BLUE|BLUE.*FROG|FROG") { return "xmm.frog.blue" }
        if ($item -match "TINGLE.*MAP") { return "xmm.tingle_map.$slug" }
        if ($item -match "MAP") { return "xmm.map.$slug" }
        if ($item -match "COMPASS") { return "xmm.compass.$slug" }
        if ($item -match "SOUL_ENEMY") { return "xmm.enemy_soul.$slug" }
        if ($item -match "SOUL_BOSS|BOSS.*REMAIN|ODOLWA|GOHT|GYORG|TWINMOLD|MAJORA") { return "xmm.boss_remains.$slug" }
        if ($item -match "MASK_FIERCE_DEITY") { return "xmm.mask.fierce_deity" }
        if ($item -match "MASK_DEKU") { return "xmm.mask.deku" }
        if ($item -match "MASK_COUPLE") { return "xmm.mask.couple" }
        if ($item -match "POSTMAN.*MASK|MASK_POSTMAN") { return "xmm.mask.postman" }
        if ($item -match "GIANT.*MASK|MASK_GIANT|MASK_KYOJIN") { return "xmm.mask.giant" }
        if ($item -match "GREAT.*FAIRY.*MASK|MASK_GREAT_FAIRY|MASK_BIGELF") { return "xmm.mask.great_fairy" }
        if ($item -match "BLAST.*MASK|MASK_BLAST|MASK_BAKURETU") { return "xmm.mask.blast" }
        if ($item -match "GIBDO.*MASK|MASK_GIBDO|MASK_GIBUDO") { return "xmm.mask.gibdo" }
        if ($item -match "GARO.*MASK|MASK_GARO") { return "xmm.mask.garo" }
        if ($item -match "MASK_") { return "xmm.mask.$($slug.Replace('mask_', ''))" }
        if ($item -match "BOTTLE_CHATEAU|CHATEAU") { return "xmm.bottle.chateau_romani" }
        if ($item -match "BOTTLE_GOLD_DUST|GOLD_DUST") { return "xmm.bottle.gold_dust" }
        if ($item -match "BOTTLE_ZORA_EGG|ZORA_EGG") { return "xmm.bottle.zora_egg" }
        if ($item -match "BOTTLE_SEAHORSE|SEAHORSE") { return "xmm.bottle.seahorse" }
        if ($item -match "BOTTLE_HYLIAN_LOACH|HYLIAN_LOACH|LOACH") { return "xmm.bottle.hylian_loach" }
        if ($item -match "PICTO|CAMERA") { return "xmm.picto_box" }
        if ($item -match "POWDER_KEG|BIGBOMB") { return "xmm.powder_keg" }
        if ($item -match "BOMBERS_NOTEBOOK|NOTEBOOK|SCHEDULE") { return "xmm.quest.bombers_notebook" }
        if ($item -match "SUN_MASK|MSSA") { return "xmm.quest.sun_mask" }
        if ($item -match "DEED_LAND|LAND.*DEED") { return "xmm.quest.deed_land" }
        if ($item -match "DEED_SWAMP|SWAMP.*DEED") { return "xmm.quest.deed_swamp" }
        if ($item -match "DEED_MOUNTAIN|MOUNTAIN.*DEED") { return "xmm.quest.deed_mountain" }
        if ($item -match "DEED_OCEAN|OCEAN.*DEED") { return "xmm.quest.deed_ocean" }
        if ($item -match "PENDANT|LETTER|MOON|TITLE_DEED|_DEED|DEED_|ROOM_KEY|EXPRESS_MAIL") { return "xmm.quest.$slug" }
        if ($item -match "TIME_NIGHT_1|NIGHT.*1") { return "xmm.time.night_1" }
        if ($item -match "TIME_NIGHT_2|NIGHT.*2") { return "xmm.time.night_2" }
        if ($item -match "TIME_NIGHT_3|NIGHT.*3") { return "xmm.time.night_3" }
        if ($item -match "TIME_DAY_1|DAY.*1") { return "xmm.time.day_1" }
        if ($item -match "TIME_DAY_2|DAY.*2") { return "xmm.time.day_2" }
        if ($item -match "TIME_DAY_3|DAY.*3") { return "xmm.time.day_3" }
        if ($item -match "TIME_PROGRESSIVE|PROGRESSIVE_TIME") { return "xmm.time.progressive" }
        if ($item -match "PROGRESSIVE_SWORD") { return "xootxmm.placeholder.mm_progressive_sword" }
        if ($item -match "HOOKSHOT") { return "xmm.equipment.hookshot" }
        if ($item -match "LENS") { return "xmm.equipment.lens" }
        if ($item -match "PROGRESSIVE_BOW|BOW") { return "xmm.equipment.bow" }
        if ($item -match "BOMB_BAG") { return "xmm.equipment.bomb_bag" }
        if ($item -match "QUIVER|ARROWCASE") { return "xmm.equipment.quiver" }
        if ($item -match "WALLET|PURSE") { return "xmm.equipment.wallet" }
        if ($item -match "SWORD|SHIELD") { return "xmm.equipment.$slug" }
        if ($item -match "SMALL_KEY") { return "xmm.key.small" }
        if ($item -match "BOSS_KEY") { return "xmm.key.boss" }
        if ($item -match "SONG_EPONA|EPONA.*SONG") { return "xmm.song.epona" }
        if ($item -match "SONG_NOVA|BOSSA|NEW_WAVE") { return "xmm.song.nova" }
        if ($item -match "SONG_SONATA|SONATA") { return "xmm.song.sonata" }
        if ($item -match "SONG_SOARING|SOARING") { return "xmm.song.soaring" }
        if ($item -match "SONG_ELEGY|ELEGY") { return "xmm.song.elegy" }
        if ($item -match "SONG_OATH|OATH") { return "xmm.song.oath" }
        if ($item -match "LULLABY_INTRO") { return "xmm.song.lullaby_intro" }
        if ($item -match "PROGRESSIVE_LULLABY|SONG_LULLABY|LULLABY") { return "xmm.song.lullaby" }
        if ($item -match "SONG|SONATA|LULLABY|BOSSA|ELEGY|OATH|NOVA") { return "xmm.song.$slug" }
        return "xmm.$slug"
    }

    if ($item -match "OCARINA_BUTTON.*A|BUTTON_A") { return "xoot.ocarina_button.a" }
    if ($item -match "OCARINA_BUTTON.*C_UP|BUTTON_C_UP") { return "xoot.ocarina_button.c_up" }
    if ($item -match "OCARINA_BUTTON.*C_LEFT|BUTTON_C_LEFT") { return "xoot.ocarina_button.c_left" }
    if ($item -match "OCARINA_BUTTON.*C_DOWN|BUTTON_C_DOWN") { return "xoot.ocarina_button.c_down" }
    if ($item -match "OCARINA_BUTTON.*C_RIGHT|BUTTON_C_RIGHT") { return "xoot.ocarina_button.c_right" }
    if ($item -match "ROCS_FEATHER|ROC") { return "xoot.rocs_feather" }
    if ($item -match "LONGSHOT") { return "xoot.unique.longshot" }
    if ($item -match "PROGRESSIVE HOOKSHOT") { return "xoot.progressive_hookshot" }
    if ($item -match "HOOKSHOT") { return "xoot.unique.hookshot" }
    if ($item -match "MEDALLION") { return "xoot.medallion.$slug" }
    if ($item -match "SPIRITUAL_STONE|KOKIRI_EMERALD|GORON_RUBY|ZORA_SAPPHIRE") { return "xoot.spiritual_stone.$slug" }
    if ($item -match "DINS_FIRE|NAYRUS_LOVE|FARORES_WIND") { return "xoot.spell.$slug" }
    if ($item -match "BOOTS") { return "xoot.boots.$slug" }
    if ($item -match "TUNIC") { return "xoot.tunic.$slug" }
    if ($item -match "STRENGTH UPGRADE") { return "xoot.strength.progressive_strength" }
    if ($item -match "GAUNTLET|BRACELET") { return "xoot.strength.$slug" }
    if ($item -match "SCALE") { return "xoot.scale.$slug" }
    if ($item -match "PROGRESSIVE_NUT_CAPACITY|NUT_CAPACITY") { return "xoot.progressive_nut_capacity" }
    if ($item -match "PROGRESSIVE_STICK_CAPACITY|STICK_CAPACITY") { return "xoot.progressive_stick_capacity" }
    if ($item -match "FISHING_POLE") { return "xoot.fishing_pole" }
    if ($item -match "MASK_") { return "xoot.mask.$($slug.Replace('mask_', ''))" }
    if ($item -match "SMALL_KEY") { return "xoot.key.small" }
    if ($item -match "BOSS_KEY") { return "xoot.key.boss" }

    return "xoot.$slug"
}

function Test-OoTxMmModelTestItem {
    param($Item)

    if (-not $Item.id) {
        return $false
    }

    $id = $Item.id.ToUpperInvariant()
    $modelId = Resolve-ForeignModelId $Item.id $Item.sourceGame
    if ($id -match "REFILL|RUPEE|ARROWS? \(|BOMBS? \(|BOMBCHU \(|DEKU_NUTS? \(|DEKU_STICKS? \(|RECOVERY_HEART|JUNK|BUY DEKU|BUY BOMB|BUY ARROW|BUY HEART") {
        return $false
    }

    $metadataText = "$($Item.type) $($Item.category) $($Item.tier) $($Item.pool.type) $($Item.pool.category) $($Item.pool.tier)".ToUpperInvariant()
    if ($metadataText -match "MAJOR|MASK|OWL|STRAY_FAIRY|QUEST|SOUL") {
        return $true
    }

    if ($modelId -match "^xmm\.(mask|owl_statue|skulltula_token|frog|stray_fairy|boss_remains|enemy_soul|quest|equipment|key|ocarina_button)\b") {
        return $true
    }

    if ($modelId -match "^xoot\.(rocs_feather|hookshot|longshot|medallion|spiritual_stone|spell|boots|tunic|strength|scale|fishing_pole|mask|key|ocarina_button)\b") {
        return $true
    }

    if ($modelId -match "^xshared\.(heart_piece|heart_container|double_defense|bomb_bag|magic_beans|mask|ocarina_of_time|song)\b") {
        return $true
    }

    if ($modelId -match "^x(mm|oot)\." -and $id -notmatch "HEART$|HEART_|MAGIC|SEEDS|STICKS|NUTS|BOMBS|ARROWS|RUPEE|REFILL") {
        return $true
    }

    if ($id -match "MASK|OWL|FROG|STRAY_FAIRY|SOUL|BOSS_KEY|SMALL_KEY|MAP|COMPASS|SONG|OCARINA_BUTTON|BUTTON_C_|BUTTON_A|MEDALLION|SPIRITUAL_STONE|KOKIRI_EMERALD|GORON_RUBY|ZORA_SAPPHIRE|HOOKSHOT|LONGSHOT|ROCS_FEATHER|ROC'S FEATHER|BOW|LENS|BOTTLE|SWORD|SHIELD|TUNIC|BOOTS|GAUNTLET|BRACELET|SCALE|SPELL|DINS_FIRE|NAYRUS_LOVE|FARORES_WIND|MAGIC_BEAN|BOMB_BAG|QUIVER|WALLET|HEART_PIECE|PIECE OF HEART|HEART_CONTAINER|DOUBLE_DEFENSE") {
        return $true
    }

    return $false
}

function Resolve-PlacementModel {
    param($Item, $Check)

    if ($Item.sourceGame -eq $Check.game) {
        if ($Item.model) {
            if ($Item.model -is [string]) {
                return $Item.model
            }

            $nativeModel = Get-ObjectPropertyValue $Item.model $Check.game
            if ($nativeModel) {
                return $nativeModel
            }
        }

        return $Item.id
    }

    if ($Item.model -and -not ($Item.model -is [string])) {
        $targetModel = Get-ObjectPropertyValue $Item.model $Check.game
        if ($targetModel) {
            return $targetModel
        }
    }

    return Resolve-ForeignModelId $Item.id $Item.sourceGame
}

function Test-PlacementAllowed {
    param($Check, $Item)

    $tags = @($Check.placementTags)
    $id = "$($Item.id)".ToUpperInvariant().Replace(" ", "_").Replace("-", "_")
    $sourceGame = "$($Item.sourceGame)"

    if ($tags -contains "volvagia_reward" -and $id -match "MEGATON_HAMMER|HAMMER") { return $false }
    if ($tags -contains "twinrova_reward" -and $sourceGame -eq "oot" -and $id -match "MIRROR_SHIELD|SHIELD_MIRROR") { return $false }
    if ($tags -contains "ganons_castle" -and $sourceGame -eq "oot" -and $id -match "MEDALLION|KOKIRI_EMERALD|GORON_RUBY|ZORA_SAPPHIRE|SPIRITUAL_STONE") { return $false }
    if ($tags -contains "woodfall_temple" -and $sourceGame -eq "mm" -and $id -match "MASK_DEKU|DEKU_MASK|SONATA") { return $false }
    if ($tags -contains "snowhead_temple" -and $sourceGame -eq "mm" -and $id -match "MASK_GORON|GORON_MASK|LULLABY") { return $false }
    if ($tags -contains "great_bay_temple" -and $sourceGame -eq "mm" -and $id -match "MASK_ZORA|ZORA_MASK|BOSSA|NOVA") { return $false }
    if ($tags -contains "moon" -and $sourceGame -eq "mm" -and $id -match "ODOLWA|GYORG|GOHT|OATH") { return $false }

    return $true
}

function Select-AllowedItemForCheck {
    param($Items, [int]$StartIndex, $Check)

    if ($Items.Count -eq 0) {
        throw "No enabled items are available for placement."
    }

    for ($offset = 0; $offset -lt $Items.Count; $offset++) {
        $item = $Items[($StartIndex + $offset) % $Items.Count]
        if (Test-PlacementAllowed $Check $item) {
            return $item
        }
    }

    throw "No item satisfied Nearly No Logic restrictions for check $($Check.id)."
}

function Build-SharedPlacementManifest {
    param(
        [string]$SeedString,
        $Checks,
        $ModCatalog,
        $EnabledGroups,
        [string]$Root,
        [bool]$TestingModelSeed = $false
    )

    $shufflableChecks = @($Checks | Where-Object { Test-CheckEnabled $_ $EnabledGroups })
    $trapPolicy = Get-EnabledTrapPolicy $Root
    $settingValues = Build-SettingValueMap $Root
    $items = @()
    foreach ($check in $shufflableChecks) {
        $modelId = if ($check.model) { $check.model } else { Resolve-ForeignModelId $check.item $check.game }
        $itemSourceSetting = Get-ItemSourceSetting $check.game $check.item
        $candidateItem = [pscustomobject]@{
            id = $check.item
            nativeId = $check.item
            logicalId = $null
            sharedId = $null
            modelId = $modelId
            tier = Get-CrossoverItemTier $check.item $modelId
            isTrap = ($check.item -match "TRAP")
            isShared = ([bool](Resolve-SharedModelId $check.item))
            sourceSetting = if ($itemSourceSetting) { $itemSourceSetting } else { $check.sourceSetting }
            sourceCheck = $check.id
            sourceGame = $check.game
            sourceGroup = $check.group
            model = $check.model
            price = $check.price
        }
        if ((Test-ItemSettingEnabled $candidateItem $settingValues) -and (Test-TrapEnabledForItem $candidateItem $trapPolicy)) {
            $items += $candidateItem
        }
    }

    foreach ($item in @($ModCatalog.items)) {
        if ($null -eq $item) {
            continue
        }

        $pool = $item.pool
        if ($null -ne $pool -and $pool.enabledBy -eq $false) {
            continue
        }

        $count = 1
        if ($null -ne $pool -and $null -ne $pool.count) {
            $count = [Math]::Max(1, [int]$pool.count)
        }

        for ($i = 0; $i -lt $count; $i++) {
            $modelId = Resolve-ForeignModelId $item.id $item.sourceGame
            $itemSourceSetting = Get-ItemSourceSetting $item.sourceGame $item.id
            $candidateItem = [pscustomobject]@{
                id = $item.id
                nativeId = $item.id
                logicalId = $null
                sharedId = $null
                modelId = $modelId
                tier = Get-CrossoverItemTier $item.id $modelId
                isTrap = ("$($item.id)" -match "TRAP")
                isShared = ([bool](Resolve-SharedModelId $item.id))
                sourceSetting = $itemSourceSetting
                sourceCheck = $null
                sourceGame = $item.sourceGame
                sourceGroup = "mod"
                model = $item.models
                price = $null
            }
            if ((Test-ItemSettingEnabled $candidateItem $settingValues) -and (Test-TrapEnabledForItem $candidateItem $trapPolicy)) {
                $items += $candidateItem
            }
        }
    }

    $items = @(Add-StandaloneMmPoolItems $items $settingValues $trapPolicy)
    $items = @(Remove-DuplicateLogicalItems $items)
    foreach ($item in $items) {
        $logicalId = Get-LogicalItemKey $item
        $item.logicalId = $logicalId
        if ($item.isShared) {
            $item.sharedId = $logicalId
        }
    }

    if ($TestingModelSeed) {
        $targetChecks = @($shufflableChecks | Where-Object { Test-OoTxMmModelTestCheck $_ } | Sort-Object game, nativeId)
        $testingItems = @($items | Where-Object { Test-OoTxMmModelTestItem $_ } | Sort-Object id, sourceCheck, sourceGame)
        if ($targetChecks.Count -eq 0) {
            throw "OoTxMM model test seed did not find Market Guard House Child pots or Termina Field grass checks."
        }

        if ($testingItems.Count -eq 0) {
            throw "OoTxMM model test seed did not find major/mask/owl/stray fairy items in the current pool."
        }

        $orderedTestingItems = @($testingItems | Sort-Object @{ Expression = { Get-StableHash "$SeedString|ootxmm-model-test|item|$($_.id)|$($_.sourceCheck)|$($_.sourceGame)|$($_.sourceGroup)" } }, id, sourceCheck)
        $orderedTargetChecks = @($targetChecks | Sort-Object @{ Expression = { Get-StableHash "$SeedString|ootxmm-model-test|check|$($_.id)" } }, id)
        $testingPlacements = @()
        $placementCount = [Math]::Min($orderedTargetChecks.Count, $orderedTestingItems.Count)
        for ($i = 0; $i -lt $placementCount; $i++) {
            $check = $orderedTargetChecks[$i]
            $item = Select-AllowedItemForCheck $orderedTestingItems $i $check
            $modelId = Resolve-PlacementModel $item $check
            $testingPlacements += [pscustomobject]@{
                check = $check.id
                checkGame = $check.game
                checkGroup = $check.group
                nativeCheck = $check.nativeId
                item = $item.id
                itemSourceGame = $item.sourceGame
                itemSourceCheck = $item.sourceCheck
                model = $modelId
                price = Get-PlacementPrice $SeedString $check $item $settingValues
                isCrossGame = ($check.game -ne $item.sourceGame -and $null -ne $item.sourceGame)
                hintItemName = Get-HintItemName $item
                hintPlaceName = Get-HintPlaceName $check
                hintAreaName = Get-HintAreaName $check
                hintIsMajor = Test-HintMajorItem $item $modelId
            }
        }

        return [pscustomobject]@{
            version = 1
            format = "ootxmm-shared-placement"
            status = "testing-model-seed"
            algorithm = "stable-sha256-model-sweep-v1"
            testMode = "model-sweep-market-guard-house-child-and-termina-field-grass"
            checks = $orderedTargetChecks
            items = $testingItems
            placements = $testingPlacements
        }
    }

    $orderedItems = @($items | Sort-Object @{ Expression = { Get-StableHash "$SeedString|item|$($_.id)|$($_.sourceCheck)|$($_.sourceGame)|$($_.sourceGroup)" } }, id, sourceCheck)
    $placements = @()
    for ($i = 0; $i -lt $shufflableChecks.Count; $i++) {
        $check = $shufflableChecks[$i]
        $item = Select-AllowedItemForCheck $orderedItems $i $check
        $modelId = Resolve-PlacementModel $item $check
        $placements += [pscustomobject]@{
            check = $check.id
            checkGame = $check.game
            checkGroup = $check.group
            nativeCheck = $check.nativeId
            item = $item.id
            itemSourceGame = $item.sourceGame
            itemSourceCheck = $item.sourceCheck
            model = $modelId
            price = Get-PlacementPrice $SeedString $check $item $settingValues
            isCrossGame = ($check.game -ne $item.sourceGame -and $null -ne $item.sourceGame)
            hintItemName = Get-HintItemName $item
            hintPlaceName = Get-HintPlaceName $check
            hintAreaName = Get-HintAreaName $check
            hintIsMajor = Test-HintMajorItem $item $modelId
        }
    }

    return [pscustomobject]@{
        version = 1
        format = "ootxmm-shared-placement"
        status = "generated-preview"
        algorithm = "stable-sha256-sort-v1"
        checks = $shufflableChecks
        items = $items
        placements = $placements
    }
}

function Update-SeedManifestPlacement {
    param(
        [string]$SeedManifestPath,
        $SeedManifest,
        $PlacementManifest,
        $ModCatalog,
        $EnabledGroups
    )

    if ($null -eq $SeedManifest) {
        return
    }

    $SeedManifest.mods = $ModCatalog
    if ($null -eq $SeedManifest.settings) {
        $SeedManifest | Add-Member -NotePropertyName settings -NotePropertyValue ([pscustomobject]@{}) -Force
    }
    $SeedManifest.settings | Add-Member -NotePropertyName enabledCheckGroups -NotePropertyValue $EnabledGroups -Force
    $SeedManifest.placementManifest = $PlacementManifest
    $SeedManifest | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $SeedManifestPath -Encoding UTF8
}

$sohSpoilerPath = Resolve-SoHSpoilerPath $Root
$twoShipSpoilerPath = Resolve-2ShipSpoilerPath $Root
$seedManifestPath = Join-Path $Root "crossover_seed.json"

if (-not $sohSpoilerPath -or -not (Test-Path -LiteralPath $sohSpoilerPath)) {
    Write-Host "Waiting for SoH spoiler before building crossover placements."
    exit 0
}

if (-not $twoShipSpoilerPath -or -not (Test-Path -LiteralPath $twoShipSpoilerPath)) {
    Write-Host "Waiting for 2Ship spoiler before building crossover placements."
    exit 0
}

$sohSpoiler = Read-JsonFile $sohSpoilerPath
$twoShipSpoiler = Read-JsonFile $twoShipSpoilerPath
$seedManifest = Read-JsonFile $seedManifestPath
$modCatalog = Build-ModCatalog $Root
$enabledGroups = Build-EnabledCheckGroups $seedManifest $Root
$seedString = if ($seedManifest.seedString) { $seedManifest.seedString } else { "ootxmm" }

$checks = @()
$checks += Convert-SoHLocations $sohSpoiler
$checks += Convert-2ShipChecks $twoShipSpoiler
$checks += $modCatalog.checks
$testingModelSeedEnabled = $TestingModelSeed
$placementManifest = Build-SharedPlacementManifest $seedString $checks $modCatalog $enabledGroups $Root $testingModelSeedEnabled

$preview = [pscustomobject]@{
    version = 1
    format = "ootxmm-shared-placement-preview"
    generatedAt = (Get-Date).ToUniversalTime().ToString("o")
    sources = [pscustomobject]@{
        sohSpoiler = $sohSpoilerPath
        twoShipSpoiler = $twoShipSpoilerPath
        seedManifest = if (Test-Path -LiteralPath $seedManifestPath) { $seedManifestPath } else { $null }
    }
    settings = [pscustomobject]@{
        oot = $seedManifest.settings.oot
        mm = $seedManifest.settings.mm
        enabledCheckGroups = $enabledGroups
    }
    counts = [pscustomobject]@{
        totalChecks = $checks.Count
        ootChecks = @($checks | Where-Object { $_.game -eq "oot" }).Count
        mmChecks = @($checks | Where-Object { $_.game -eq "mm" }).Count
        enabledChecks = @($placementManifest.checks).Count
        placementItems = @($placementManifest.items).Count
        placements = @($placementManifest.placements).Count
        crossGamePlacements = @($placementManifest.placements | Where-Object { $_.isCrossGame }).Count
        modChecks = @($modCatalog.checks).Count
        modItems = @($modCatalog.items).Count
        testingMode = $testingModelSeedEnabled
    }
    mods = $modCatalog
    checks = $checks
    placementManifest = $placementManifest
}

if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $Root "crossover_placements_preview.json"
}

$preview | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $Output -Encoding UTF8
if (-not $NoSeedManifestUpdate -and (Test-Path -LiteralPath $seedManifestPath)) {
    Update-SeedManifestPlacement $seedManifestPath $seedManifest $placementManifest $modCatalog $enabledGroups
}

$placementOutput = Join-Path $Root "crossover_placements.json"
$placementManifest | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $placementOutput -Encoding UTF8

Write-Host "Wrote $Output"
Write-Host "Wrote $placementOutput"
Write-Host "Checks: $($preview.counts.totalChecks) (OoT $($preview.counts.ootChecks), MM $($preview.counts.mmChecks))"
Write-Host "Placements: $($preview.counts.placements), cross-game: $($preview.counts.crossGamePlacements), enabled checks: $($preview.counts.enabledChecks)"
Write-Host "Mod manifests: $(@($modCatalog.manifests).Count), mod items: $($preview.counts.modItems), mod checks: $($preview.counts.modChecks)"
