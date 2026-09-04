// ARK server-configuration schema (ASE + ASA), generated 2026-07-03 from the
// ark wiki via a 7-agent extraction. 370 settings. Loaded as a plain global so
// it works from file:// in Sciter AND the browser preview (no fetch/CORS).
const CONFIG_SCHEMA = {
  "generated": "2026-07-03",
  "source": "ark.wiki.gg / ark.fandom.com Server_configuration (7-agent extraction)",
  "count": 370,
  "settings": [
    {
      "key": "XPMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Global multiplier for all experience points gained by players and tamed creatures. 1.0 equals official rates; higher values level everything faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "TamingSpeedMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for how quickly creatures are tamed. Higher values make taming faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "HarvestAmountMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for the quantity of resources gathered from each harvest action. Higher values yield more materials per hit.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "HarvestHealthMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for the health (durability) of harvestable objects like trees and rocks, controlling how many hits they take before depleting.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ResourcesRespawnPeriodMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for the time it takes harvested resources (trees, rocks, bushes) to respawn. Lower values make resources come back faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DinoCharacterFoodDrainMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for how quickly tamed and wild creatures consume food. Higher values drain hunger faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PlayerCharacterFoodDrainMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for how quickly players lose food. Higher values increase hunger drain.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PlayerCharacterWaterDrainMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for how quickly players lose water/hydration. Higher values increase thirst drain.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DinoCharacterStaminaDrainMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for how quickly creatures consume stamina when performing actions like running or flying.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PlayerCharacterStaminaDrainMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for how quickly players consume stamina from sprinting and other exertion.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DinoCharacterHealthRecoveryMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for the rate at which creatures regenerate health. Higher values heal creatures faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PlayerCharacterHealthRecoveryMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for the rate at which players regenerate health. Higher values heal players faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DinoCountMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for the number of wild creatures spawned in the world. Higher values increase creature population density.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DayCycleSpeedScale",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Time & Weather",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the overall speed of the day/night cycle. Lower values lengthen full days; 1.0 matches singleplayer/official pacing.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DayTimeSpeedScale",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Time & Weather",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the length of daytime relative to night. Lower values make days last longer.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "NightTimeSpeedScale",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Time & Weather",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the length of nighttime relative to day. Lower values make nights last longer.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DisableWeatherFog",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Time & Weather",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If true, disables fog weather effects across the map.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DifficultyOffset",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Sets the difficulty scaling (0.0-1.0) that together with the official-difficulty value determines maximum wild creature levels and loot quality. Not present by default and must be added manually; ASA fresh servers behave as low difficulty until set.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "OverrideOfficialDifficulty",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides the server difficulty level directly, setting the max wild creature level (e.g. 5.0 gives level 150 wild dinos). Not set by default; when added it takes precedence over DifficultyOffset.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "OverrideStructurePlatformPrevention",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, allows normally-restricted structures (such as turrets and spike walls) to be placed on platform saddles.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerPVE",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, runs the server in Player-vs-Environment mode, disabling player-vs-player combat.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerHardcore",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, enables hardcore mode where a player's character is reset to level 1 on death (permadeath of progress).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerCrosshair",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "true",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If False, disables the aiming crosshair HUD element for all players.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerForceNoHUD",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, forces the floating player-name HUD to stay hidden for all players.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowThirdPersonPlayer",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "true",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Controls whether players may use the third-person camera view. Disable to force first-person only.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalVoiceChat",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If true, voice chat is global and heard by everyone on the server rather than by proximity.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ProximityChat",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, text/voice chat is only visible/audible to nearby players rather than server-wide.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ShowMapPlayerLocation",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "true",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, players can see their own precise location on the map; if False, location is hidden.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "EnablePVPGamma",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, allows players to adjust their gamma (brightness) setting on PvP servers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DisablePVEGamma",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, prevents players from adjusting gamma (brightness) on PvE servers to stop night-vision exploits.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ShowFloatingDamageText",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If true, shows floating damage numbers when dealing damage (RPG-style combat text).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowHitMarkers",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "true",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If true, displays hit markers when the player successfully hits a target with a ranged attack.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AlwaysNotifyPlayerLeft",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If true, all players are notified in chat whenever any player leaves the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DontAlwaysNotifyPlayerJoined",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, suppresses the server-wide chat notification when a player joins (inverse of alwaysNotifyPlayerJoined).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AutoSavePeriodMinutes",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "15.0",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Interval in minutes between automatic world saves. Lower values save more often at the cost of periodic save-hitch lag.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "KickIdlePlayersPeriod",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "3600.0",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Time in seconds a player may be idle before being kicked. Requires the -EnableIdlePlayerKick launch option to take effect.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ClampResourceHarvestDamage",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, clamps the damage dealt to resource nodes so extremely high harvest damage cannot instantly deplete resources in a single hit.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "UseOptimizedHarvestingHealth",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If true, uses an optimized harvesting health system intended to improve resource-gathering performance.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PreventDiseases",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If true, disables diseases such as Swamp Fever on the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxPlayers",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "70",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of simultaneous players allowed on the server. Also settable via the -WinLiveMaxPlayers command line arg in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "alwaysNotifyPlayerJoined",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, all players are notified in chat whenever any player joins the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bUseCorpseLocator",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, shows a green beam of light at the location of a player's death corpse to help recover items.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowUnlimitedRespecs",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, removes the cooldown on the Mindwipe Tonic so players can respec their stats as often as they want.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxNumberOfPlayersInTribe",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "0",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of players allowed in a single tribe. A value of 0 means unlimited tribe size.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PreventTribeAlliances",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, tribes are prevented from forming alliances with one another.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxAlliancesPerTribe",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "0",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of alliances a single tribe may belong to. 0 means unlimited (requires PreventTribeAlliances=False).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxTribesPerAlliance",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "0",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of tribes that may be part of a single alliance. 0 means unlimited.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "TribeNameChangeCooldown",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "0.0",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Cooldown time in minutes before a tribe is allowed to change its name again.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxTribeLogs",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "100",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of entries retained in the tribe activity log before older entries are dropped.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowFlyerCarryPvE",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, allows flying creatures to pick up and carry wild creatures (and players) in PvE mode. Also written AllowFlyerCarryPvE in some versions.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableFriendlyFire",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, prevents members of the same tribe (and allied tribes) from damaging each other and their structures/tames.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bPvEDisableFriendlyFire",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, disables friendly fire between members of the same tribe/alliance specifically on PvE servers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bPvPDinoDecay",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, unclaimed/tamed creatures are subject to decay (auto-destruction) on PvP servers, similar to structure decay.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bPvEAllowStructuresAtSupplyDrops",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, allows players to build structures near/on top of supply drop points in PvE mode.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowCaveBuildingPvE",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, allows players to build structures inside caves on PvE servers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowCaveBuildingPvP",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "True",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If False, disallows building inside caves on PvP servers (building in caves is permitted by default).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvPZoneStructureDamageMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "6.0",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier for damage structures take within PvP zones / volcano caves. Higher values make cave structures more fragile.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StructureDamageMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the damage that structures deal with their own attacks (e.g. spike walls, plant turrets). Higher values increase the damage output.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StructureDamageRepairCooldown",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "180",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Time in seconds after a structure takes damage before it can be repaired. Set to 0 to remove the repair cooldown.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bPassiveDefensesDamageRiderlessDinos",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, passive defenses such as spike walls and plant species X will damage riderless (unmounted) creatures.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowMultipleAttachedC4",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, allows more than one C4 charge to be attached to a single creature at a time.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PreventOfflinePvP",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, enables Offline Raid Protection (ORP) making a tribe's structures and creatures invulnerable while all members are offline.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PreventOfflinePvPInterval",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "900",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Delay in seconds after the last tribe member logs off before offline raid protection becomes active.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PreventOfflinePvPConnectionInvincibleInterval",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "5.0",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Grace period in seconds during which a tribe member remains invincible right after connecting, used with offline raid protection.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bIncreasePvPRespawnInterval",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "True",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, enables progressively increasing respawn timers for players repeatedly killed by the same enemy tribe (anti spawn-camp). Master toggle for the IncreasePvPRespawnInterval* settings.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "IncreasePvPRespawnIntervalCheckPeriod",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "300",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The time window (in seconds) within which repeated deaths to the same tribe count toward the increasing respawn interval.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "IncreasePvPRespawnIntervalMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The multiplier applied to the respawn delay for each additional death within the check period, increasing the wait each time.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "IncreasePvPRespawnIntervalBaseAmount",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "60",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The base amount of additional respawn time (in seconds) added when the escalating PvP respawn interval applies.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAutoPvETimer",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, enables an automatic PvE/PvP schedule that switches the server between PvE and PvP based on the configured start/stop times.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAutoPvEUseSystemTime",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, the Auto PvE timer uses real-world server system time; when false it uses in-game world time for the start/stop schedule.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AutoPvEStartTimeSeconds",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "0",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Time of day (in seconds from midnight, 0-86400) at which the server automatically switches into PvE mode. Requires bAutoPvETimer.",
      "enumValues": null,
      "example": "21600",
      "complex": false
    },
    {
      "key": "AutoPvEStopTimeSeconds",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "0",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Time of day (in seconds from midnight, 0-86400) at which the server automatically switches back out of PvE mode. Requires bAutoPvETimer.",
      "enumValues": null,
      "example": "64800",
      "complex": false
    },
    {
      "key": "DisableStructureDecayPvE",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, completely disables the auto-decay of player structures on PvE servers so buildings never decay from inactivity.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvEStructureDecayPeriodMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier scaling how long PvE structures take to decay. Higher values lengthen the time before structures can be demolished by others.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvEStructureDecayDestructionPeriod",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "0",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Time in seconds after which decayed PvE structures are destroyed. A value of 0 disables structure destruction from decay.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvPStructureDecay",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, applies the structure decay system on PvP servers (structures decay from tribe inactivity).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ClampItemStats",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, enables clamping of item stats to configured maximums to prevent over-rolled/exploited gear. Works with the ItemStatClamps overrides in Game.ini.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowRaidDinoFeeding",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "PvE",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, allows raid creatures such as the Titanosaur to be permanently tamed via feeding rather than despawning.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bForceCanRideFliers",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE"
      ],
      "description": "If True, forces flyers to be rideable on maps (such as Genesis Part 1) where flying is normally disabled. ASE-only.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowFlyingStaminaRecovery",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, allows a flyer's stamina to recover while a player is standing on it (rather than only while dismounted).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowAnyoneBabyImprintCuddle",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "If True, any tribe member (not only the imprinter) can perform baby-creature imprint care such as cuddling.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableStructurePlacementCollision",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, allows structures to be placed clipping through terrain/landscape, ignoring some placement collision (terrain-clipping enabled).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AdminLogging",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, all admin/cheat commands are logged to the in-game global chat and server logs for transparency.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "TheMaxStructuresInRange",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "10500",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of structures that can be built within a given radius across all players and tribes. Raising it allows larger/denser bases but increases server load.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxStructuresInSmallRadius",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Caps how many structures may be placed within a small radius of one another, limiting extreme density in a confined area.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StructureResistanceMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the resistance of structures to incoming damage. Higher values make structures tougher (take less damage); lower values make them weaker.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowPlatformSaddleMultiFloors",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, allows stacking multiple floors/levels of structures on platform saddles and rafts.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxPlatformSaddleStructureLimit",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "100",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Sets the maximum number of structures that can be built on a single platform saddle or raft. Acts as an absolute cap that the per-platform multiplier is measured against.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PerPlatformMaxStructuresMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplier applied to each platform's base structure limit. For example 2.0 doubles how many structures each platform saddle can hold, up to MaxPlatformSaddleStructureLimit.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "EnableExtraStructurePreventionVolumes",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Enables additional no-build zones on certain maps to prevent players from blocking key resource, artifact or spawn areas.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StructurePreventResourceRadiusMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the radius around resource nodes in which structures cannot be built. Lower values let players build closer to resources; higher values push structures further away. Functionally the ServerSettings alias of ResourceNoReplenishRadiusStructures.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableStructurePlacementCollision",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, allows structures to be placed while clipping through terrain and other geometry, ignoring most placement collision checks.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StructurePickupTimeAfterPlacement",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "30.0",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Number of seconds after placement during which a structure can still be quickly picked up. After this window the structure can no longer be picked back up unless AlwaysAllowStructurePickup is enabled.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StructurePickupHoldDuration",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "0.5",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "How long (in seconds) the pick-up key must be held to quick-pick-up a structure. Set to 0 for instant pickup.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AlwaysAllowStructurePickup",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, removes the time limit on the quick pick-up system so structures can be picked up at any time after placement.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ForceAllStructureLocking",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, all structures and containers default to a locked state, so any lockable structure must be explicitly unlocked to be used by others.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowCrateSpawnsOnTopOfStructures",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, airdropped supply crates are allowed to appear on top of player structures instead of being blocked by them.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvEAllowStructuresAtSupplyDrops",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled on PvE servers, allows structures to be built near supply drop points instead of blocking building in those areas.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvEStructureDecayPeriodMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how long it takes for structures to begin decaying on PvE servers. Higher values lengthen the time before decay starts.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DisableDinoDecayPvE",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, disables the auto-unclaim/decay of tamed creatures on PvE servers so tames are never released due to inactivity.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvPDinoDecay",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, applies the tamed-creature decay/auto-unclaim system on PvP servers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AutoDestroyStructures",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Master toggle that enables automatic destruction of old, abandoned structures. Must be enabled for AutoDestroyOldStructuresMultiplier to have any effect.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AutoDestroyOldStructuresMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "0.0",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how long structures must be without a nearby tribe member before they auto-destroy. 0 disables auto-destruction; higher values lengthen the grace period. Requires AutoDestroyStructures enabled.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "OnlyAutoDestroyCoreStructures",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, only core/foundation structures are eligible for auto-destruction, leaving attached non-core pieces alone.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "FastDecayUnsnappedCoreStructures",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, core structures that are not snapped to anything decay much faster, helping clean up single foundations/pillars used for land claiming.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalItemDecompositionTimeMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how long dropped items persist on the ground before disappearing. Higher values make dropped items last longer.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalCorpseDecompositionTimeMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how long corpses and death bags (loot bags) persist before decomposing. Higher values keep corpses around longer.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalSpoilingTimeMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how quickly perishable items (food, etc.) spoil. Higher values make items last longer before spoiling.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ClampItemSpoilingTimes",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "Decay",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, clamps all item spoiling times to the item's maximum spoil time, preventing exploits from stacking multipliers that extend spoilage indefinitely.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bUseSingleplayerSettings",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, applies the balanced single-player/non-dedicated tuning (adjusted rates for taming, harvesting, XP, breeding, etc.) on top of your other settings.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowHideDamageSourceFromLogs",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "true",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Controls whether damage sources are hidden in tribe logs. When disabled, tribe logs reveal what/who caused each bit of damage.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "SessionName",
      "file": "GameUserSettings",
      "section": "SessionSettings",
      "type": "string",
      "default": "ARK",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The server name advertised in the in-game and Steam/Epic server browsers. This is how players find and identify the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "Port",
      "file": "GameUserSettings",
      "section": "SessionSettings",
      "type": "int",
      "default": "7777",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The UDP game connection port players use to join. Commonly set on the command line as -Port instead; must be distinct from the QueryPort.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "QueryPort",
      "file": "GameUserSettings",
      "section": "SessionSettings",
      "type": "int",
      "default": "27015",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The Steam/query port used by the server browser to report status and player counts. Must differ from the game Port; often set on the command line as -QueryPort.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MultiHome",
      "file": "GameUserSettings",
      "section": "SessionSettings",
      "type": "string",
      "default": "",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The local IP address to bind the server to on a multi-interface host (the official wiki spells the key MULTIHOME - ini keys are case-insensitive). This one field is all you need: whenever it is set, AMU adds the matching -MULTIHOME command-line flag (ASE) or ?MultiHome= URL option (ASA) to the launch command automatically.",
      "enumValues": null,
      "example": "192.168.1.50",
      "complex": false
    },
    {
      "key": "MaxPlayers",
      "file": "GameUserSettings",
      "section": "/Script/Engine.GameSession",
      "type": "int",
      "default": "70",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of simultaneous players allowed on the server. In ASA this is effectively controlled by the -WinLiveMaxPlayers command-line flag, which overrides the .ini value.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "Message",
      "file": "GameUserSettings",
      "section": "MessageOfTheDay",
      "type": "string",
      "default": "",
      "category": "Messages",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The Message of the Day text shown to players when they join the server. Single line; do not wrap in quotes.",
      "enumValues": null,
      "example": "Welcome to our server! No PvP in green zones.",
      "complex": false
    },
    {
      "key": "Duration",
      "file": "GameUserSettings",
      "section": "MessageOfTheDay",
      "type": "int",
      "default": "20",
      "category": "Messages",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "How many seconds the Message of the Day stays on screen for a joining player.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ScalabilityGroups",
      "file": "GameUserSettings",
      "section": "ScalabilityGroups",
      "type": "array",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Section holding client/graphics quality group entries (sc.* keys). Not a server gameplay setting and generally irrelevant to dedicated servers; exposed only for completeness.",
      "enumValues": null,
      "example": "sg.ResolutionQuality=100",
      "complex": true
    },
    {
      "key": "listen",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Command-line/query token (appended as ?listen) that starts a non-dedicated listen server. Dedicated servers use -server instead and should NOT set this.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "server",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -server dash flag launches the process as a dedicated server (as opposed to the game client/editor).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "log",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -log dash flag opens a console window that streams the server log output live, useful for monitoring startup and errors.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerPassword",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Password players must enter to join the server. Passed as a ?ServerPassword query option on the launch command; leave empty for an open server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerAdminPassword",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Password granting in-game admin/cheat access and required for RCON authentication. Passed as a ?ServerAdminPassword query option; keep secret.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "SpectatorPassword",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Password that lets players enter spectator mode via the requestspectator console command. Passed as a ?SpectatorPassword query option.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "RCONEnabled",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "False",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Enables the remote console (RCON) TCP interface for out-of-game administration. Set as ?RCONEnabled=True; also readable from the [ServerSettings] .ini section.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "RCONPort",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "27020",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "TCP port the RCON interface listens on when RCON is enabled. Passed as ?RCONPort; documented default 27020 (many hosts use 32330).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AltSaveDirectoryName",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides the folder name used to store this server's save data (defaults to the map name). Passed as ?AltSaveDirectoryName; commonly used to keep per-map saves separate within a cluster.",
      "enumValues": null,
      "example": "TheIsland_Cluster1",
      "complex": false
    },
    {
      "key": "MultiHomeFlag",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -MultiHome dash flag activates multihoming; the actual bind IP comes from the MultiHome value in GameUserSettings.ini. AMU emits this flag automatically whenever that ini value is set, so it never needs to be configured separately.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "clusterid",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Shared cluster identifier (-clusterid=NAME). All servers sharing the same clusterid can transfer characters, items, and creatures between them.",
      "enumValues": null,
      "example": "myclusterA",
      "complex": false
    },
    {
      "key": "ClusterDirOverride",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides where cluster transfer data is stored (-ClusterDirOverride=PATH). Cluster files are saved under <ClusterDirOverride>/<clusterid>; all cluster members should point to the same path.",
      "enumValues": null,
      "example": "C:\\ARK\\clusters",
      "complex": false
    },
    {
      "key": "NoTransferFromFiltering",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -NoTransferFromFiltering flag blocks uploading characters/items into this cluster from other clusters or single player, keeping the cluster's economy self-contained.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "WinLiveMaxPlayers",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "70",
      "category": "Players",
      "games": [
        "ASA"
      ],
      "description": "ASA-only command-line flag (-WinLiveMaxPlayers=N) that sets the real maximum player count, overriding the MaxPlayers .ini value which ASA otherwise ignores.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerPlatform",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "PC",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASA"
      ],
      "description": "ASA crossplay control (-ServerPlatform=LIST) selecting which platforms may join, e.g. PC only or a '+'-joined combination. Replaces ASE's -crossplay/-epiconly flags.",
      "enumValues": null,
      "example": "PC+PS5+XSX",
      "complex": false
    },
    {
      "key": "crossplay",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE"
      ],
      "description": "The -crossplay flag (ASE) enables Steam and Epic Games Store players to share the server. Superseded in ASA by -ServerPlatform.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "epiconly",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE"
      ],
      "description": "The -epiconly flag (ASE) restricts the server to Epic Games Store clients only.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PublicIPForEpic",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "Specifies the public IP address advertised to Epic Games Store clients (-PublicIPForEpic=IP), needed for EGS/crossplay discovery behind NAT.",
      "enumValues": null,
      "example": "203.0.113.10",
      "complex": false
    },
    {
      "key": "NoBattlEye",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -NoBattlEye flag disables the BattlEye anti-cheat system for the server. Players must correspondingly join without BattlEye.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "UseBattlEye",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "true",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -UseBattlEye flag explicitly enables BattlEye anti-cheat (the default behaviour); provided to force-enable when defaults have been changed.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "insecure",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "The -insecure flag disables Valve Anti-Cheat (VAC) protection on the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "culture",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides the server's language/locale (-culture=CODE) for server-side text output, e.g. en, de, fr.",
      "enumValues": null,
      "example": "en",
      "complex": false
    },
    {
      "key": "exclusivejoin",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -exclusivejoin flag turns on whitelist-only mode: only players listed in PlayersExclusiveJoinList.txt may connect.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "EnableIdlePlayerKick",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -EnableIdlePlayerKick flag kicks players who remain idle beyond the configured timeout, freeing slots.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "servergamelog",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -servergamelog flag enables detailed admin/game logging that can be read over RCON via the getgamelog command.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "servergamelogincludetribelogs",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -servergamelogincludetribelogs flag adds tribe log entries into the server game log output.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ServerRCONOutputTribeLogs",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -ServerRCONOutputTribeLogs flag routes tribe log messages to RCON clients.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "NotifyAdminCommandsInChat",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Messages",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -NotifyAdminCommandsInChat flag broadcasts admin command usage; visibility is limited to admins in chat.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ForceAllowCaveFlyers",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -ForceAllowCaveFlyers flag permits flying creatures to enter caves, which are normally off-limits to flyers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "NoDinos",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -NoDinos flag prevents wild creatures from spawning on the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ForceRespawnDinos",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -ForceRespawnDinos flag destroys all existing wild creatures at startup so they respawn fresh (e.g. after changing spawn settings).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "NoWildBabies",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -NoWildBabies flag disables the spawning of wild baby creatures.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowFlyerSpeedLeveling",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -AllowFlyerSpeedLeveling flag re-enables leveling of movement speed on flying tames (disabled by default on official).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "imprintlimit",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -imprintlimit=N flag auto-destroys tames whose imprint quality exceeds N percent, curbing exploited imprint values.",
      "enumValues": null,
      "example": "101",
      "complex": false
    },
    {
      "key": "noundermeshchecking",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -noundermeshchecking flag fully disables the anti-meshing (under-map) detection system.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "noundermeshkilling",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -noundermeshkilling flag stops the anti-meshing system from killing offenders, only teleporting them instead.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "UseStructureStasisGrid",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -UseStructureStasisGrid flag enables a stasis grid for large bases to improve server performance.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "structurememopts",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -structurememopts flag turns on structure memory optimizations to reduce RAM use for structure-heavy servers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GBUsageToForceRestart",
      "file": "CommandLine",
      "section": "",
      "type": "float",
      "default": "0",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -GBUsageToForceRestart=N flag auto-restarts the server when memory use exceeds N gigabytes; 0 disables the check.",
      "enumValues": null,
      "example": "20",
      "complex": false
    },
    {
      "key": "NoHangDetection",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -NoHangDetection flag disables the watchdog that aborts a startup taking longer than ~45 minutes (useful for very large saves).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StasisKeepControllers",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -StasisKeepControllers flag keeps creature AI controllers loaded during stasis at the cost of extra memory.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "UseDynamicConfig",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -UseDynamicConfig flag makes the server periodically fetch a dynamic config file from a URL to change select rates live without restart.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ActiveEvent",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The -ActiveEvent=NAME flag force-enables a seasonal event (and its colors/spawns), e.g. Easter, WinterWonderland, Summer.",
      "enumValues": null,
      "example": "WinterWonderland",
      "complex": false
    },
    {
      "key": "newsaveformat",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "The -newsaveformat flag enables ARK's newer save format for faster saves (notably cryopod handling). Default already on in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "usestore",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "The -usestore flag adopts official-style stored player character data handling.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "mods",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASA"
      ],
      "description": "ASA CurseForge mod list (-mods=ID1,ID2,...). Comma-separated mod IDs loaded and enabled at startup. Replaces ASE's ?GameModIds.",
      "enumValues": null,
      "example": "928988,930430",
      "complex": false
    },
    {
      "key": "passivemods",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASA"
      ],
      "description": "ASA flag (-passivemods=ID1,ID2) that loads mod data without enabling functionality, used so clients can retain data for cross-server transfers.",
      "enumValues": null,
      "example": "928988",
      "complex": false
    },
    {
      "key": "GameModIds",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE Steam Workshop mod list passed as ?GameModIds=ID1,ID2 query option. Deprecated/replaced by -mods in ASA.",
      "enumValues": null,
      "example": "895711211,731604991",
      "complex": false
    },
    {
      "key": "bUseSingleplayerSettings",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When enabled, applies an additional set of balancing multipliers on top of the configured (or default) values, tuned for single-player and small unofficial sessions.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MatingIntervalMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the cooldown interval before two tames can mate again. Lower values shorten the wait so creatures can breed more frequently.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MatingSpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how fast tames complete the mating process. Higher values make mating finish faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "EggHatchSpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the incubation speed of fertilized eggs. Higher values make eggs hatch faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BabyMatureSpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how quickly babies mature into adults. Higher values reduce the time needed to raise a baby.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BabyFoodConsumptionSpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how fast babies consume food from their inventory while growing. Lower values slow food consumption so babies are easier to raise.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BabyImprintingStatScaleMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how much of a stat bonus a fully imprinted creature receives. Higher values make imprinting give a larger stat boost; 0 disables imprinting bonuses.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BabyImprintAmountMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the imprint percentage gained per cuddle/care action. For example 0.5 halves the imprint amount granted by each request.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BabyCuddleIntervalMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the interval between a baby's imprint (cuddle/care) requests. Lower values make imprint requests appear more frequently.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BabyCuddleGracePeriodMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the grace period you have to fulfill an imprint request before imprint quality starts dropping. Higher values give more time.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BabyCuddleLoseImprintQualitySpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how quickly imprint quality is lost after a missed imprint request's grace period expires. Lower values make missed cuddles less punishing.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "LayEggIntervalMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the interval between egg-laying by tamed creatures that lay eggs. Lower values make eggs drop more often.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PoopIntervalMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the interval between defecation (feces production) for players and creatures. Lower values increase how often poop is produced.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "HairGrowthSpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Breeding",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how fast player and creature hair/fur grows back after being cut. Higher values speed up hair regrowth.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalSpoilingTimeMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the time it takes for perishable items (food, etc.) to spoil globally. Higher values make items last longer before spoiling.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalItemDecompositionTimeMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the time dropped items take to decompose (despawn) on the ground. Higher values keep dropped items around longer.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalCorpseDecompositionTimeMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the time corpses and death bags take to decompose. Higher values keep corpses/loot bags around longer before despawning.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CropGrowthSpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how fast crops grow in plots. Higher values speed up crop growth.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CropDecaySpeedMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how fast crops decay/wilt when unwatered or unfertilized. Lower values slow crop decay.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ResourceNoReplenishRadiusPlayers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the radius around players in which resources (trees, rocks, etc.) will not respawn. Higher values push resource respawns farther from players.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ResourceNoReplenishRadiusStructures",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the radius around structures in which resources will not respawn. Higher values prevent resources from repopulating near bases.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "KillXPMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the amount of experience gained from killing creatures. Higher values grant more XP per kill.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "HarvestXPMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the amount of experience gained from harvesting resources. Higher values grant more XP for gathering.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CraftXPMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the amount of experience gained from crafting items. Higher values grant more XP per craft.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GenericXPMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the amount of passive/generic experience gained (e.g. simply being alive/riding). Higher values grant more idle XP.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "SpecialXPMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the amount of experience gained from special events/actions (e.g. explorer notes, boss events). Higher values grant more special XP.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CustomRecipeEffectivenessMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the effectiveness (stat output) of custom cooking-pot recipes. Higher values make custom recipes more potent.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CustomRecipeSkillMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales how much the crafter's Crafting Skill stat contributes to custom recipe quality. Higher values reward high crafting skill more.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowCustomRecipes",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "true",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Enables or disables the ability for players to create custom cooking-pot recipes (notes with custom item effects).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CraftingSkillBonusMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the bonus applied to crafted item stats derived from the player's Crafting Skill. Higher values increase crafting-skill-based quality bonuses.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "SupplyCrateLootQualityMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the quality of loot found in supply/loot crates and drops. Higher values increase the chance of higher-tier items.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "FishingLootQualityMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the quality of loot obtained from fishing. Higher values improve fishing rewards.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableLootCrates",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, disables the spawning of loot/supply crates on the map.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowFlyerSpeedLeveling",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Allows flyers to level up Movement Speed. In ASE this could also be enabled via the -AllowFlyerSpeedLeveling launch flag; in ASA only this ini setting exists and bAllowSpeedLeveling must also be considered.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bFlyerPlatformAllowUnalignedDinoBasing",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, allows non-allied creatures to stand on and be carried by flyer platform saddles (e.g. Quetzal platforms).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bUseCorpseLocator",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "true",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, shows a green beam of light at the location of a player's death bag/corpse to help them recover items.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "UseCorpseLifeSpanMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the lifespan of player corpses and dropped death caches/bags before they despawn. Higher values keep them around longer.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PlayerHarvestingDamageMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the harvesting damage/yield players deal when gathering resources by hand. Higher values increase resources gathered per hit.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DinoHarvestingDamageMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "3.2",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the harvesting damage/yield creatures deal when gathering resources. Higher values increase resources gathered per hit by tames.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DinoTurretDamageMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the damage auto-turrets and plant species X deal to creatures. Higher values make turrets hit tames harder.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowPlatformSaddleMultiFloors",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, allows building multiple stacked floors/ceilings on platform saddles.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PvPZoneStructureDamageMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "6.0",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the damage structures take inside designated PvP zones (e.g. volcano zones on The Island). Higher values increase structure damage there.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "StructureDamageRepairCooldown",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "180",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Number of seconds after a structure takes damage during which it cannot be repaired. Set 0 to allow immediate repairs.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "LimitTurretsNum",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "100",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of turrets (and Plant Species X) allowed within the turret-limit range when turret limiting is enabled.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bLimitTurretsInRange",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "true",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, enables the turret-density limit that caps how many turrets can be placed within a given range.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "LimitTurretsRange",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "10000",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The range (in Unreal units) used by the turret-density limiter to count turrets toward LimitTurretsNum.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bHardLimitTurretsInRange",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, hard-enforces the turret-in-range limit, preventing placement beyond LimitTurretsNum rather than just soft-limiting.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PerPlatformMaxStructuresMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Structures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the maximum number of structures that can be built on platform saddles and rafts. Higher values allow more structures per platform.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PreventOfflinePvPConnectionInvincibleInterval",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "5.0",
      "category": "PvP",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The duration (in seconds) a player/structure is granted invincibility on connection when Offline Raid Protection is active, to prevent instant-death on join.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bPvEAllowTribeWar",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "true",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, allows tribes on PvE servers to mutually declare war on each other for a set period, enabling PvP combat between them.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bPvEAllowTribeWarCancel",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, allows a declared PvE tribe war to be cancelled by mutual agreement before it naturally ends.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxNumberOfPlayersInTribe",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "0",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of players allowed in a single tribe. 0 means unlimited (no tribe size cap).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxAlliancesPerTribe",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of alliances a single tribe may be part of (used with tribe alliance limiting).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxTribesPerAlliance",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Maximum number of tribes allowed in a single alliance.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxTribeLogs",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "100",
      "category": "Tribes",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Number of tribe log entries retained in the tribe manager before older entries are dropped.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "OverrideMaxExperiencePointsPlayer",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides the maximum total experience points a player can accumulate, effectively setting the player level cap ceiling.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "OverrideMaxExperiencePointsDino",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides the maximum total experience points a creature can accumulate, effectively setting the tamed creature level cap ceiling.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "FuelConsumptionIntervalMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "1.0",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the interval at which fuel-burning structures (forges, fabricators, generators, etc.) consume fuel. Values affect how quickly fuel is used up.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "GlobalPoweredBatteryDurabilityDecreasePerSecond",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "4.0",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "The rate per second at which a charged Tek/battery item loses durability while powering structures. Higher values drain batteries faster.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAutoUnlockAllEngrams",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, automatically unlocks all engrams for players regardless of level or engram point cost.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bShowCreativeMode",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, enables creative mode capabilities on the server (unlimited resources/god-like building) for testing or creative play.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableDinoTaming",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, prevents players from taming any creatures on the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableDinoRiding",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "When true, prevents players from riding/mounting any creatures on the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "LevelExperienceRampOverrides",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Defines the full XP-per-level ramp for players (and separately for dinos). Uses indexed ExperiencePointsForLevel[n] entries; the number of entries sets the effective level cap. Two lines are used, one for the player ramp and one for the dino ramp.",
      "enumValues": null,
      "example": "LevelExperienceRampOverrides=(ExperiencePointsForLevel[0]=5,ExperiencePointsForLevel[1]=20,ExperiencePointsForLevel[2]=40,ExperiencePointsForLevel[3]=65)",
      "complex": true
    },
    {
      "key": "OverridePlayerLevelEngramPoints",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Sets how many engram points a player earns at each level. Written as one line per level containing a single integer; you must supply exactly one entry for every level defined by the player LevelExperienceRampOverrides, in order starting at level 1.",
      "enumValues": null,
      "example": "OverridePlayerLevelEngramPoints=8",
      "complex": true
    },
    {
      "key": "EngramEntryAutoUnlocks",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Automatically unlocks (and grants for free) a specific engram when a player reaches the given level. Repeat the line once per engram to auto-unlock.",
      "enumValues": null,
      "example": "EngramEntryAutoUnlocks=(EngramClassName=\"EngramEntry_TekTeleporter_C\",LevelToAutoUnlock=0)",
      "complex": true
    },
    {
      "key": "OverrideNamedEngramEntries",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides a single engram by its EngramClassName: hide it, change its point cost, its level requirement, or remove its prerequisites. Preferred over OverrideEngramEntries which uses a numeric index. Repeat once per engram.",
      "enumValues": null,
      "example": "OverrideNamedEngramEntries=(EngramClassName=\"EngramEntry_Saddle_Quetz_Platform_C\",EngramHidden=False,EngramPointsCost=60,EngramLevelRequirement=75,RemoveEngramPreReq=True)",
      "complex": true
    },
    {
      "key": "OverrideEngramEntries",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Same as OverrideNamedEngramEntries but identifies the engram by its numeric EngramIndex instead of its class name; used to hide an engram or change its cost, level requirement, or prerequisites. Index-based and more fragile than the named variant.",
      "enumValues": null,
      "example": "OverrideEngramEntries=(EngramIndex=0,EngramHidden=False,EngramPointsCost=3,EngramLevelRequirement=3,RemoveEngramPreReq=True)",
      "complex": true
    },
    {
      "key": "ExcludeItemIndices",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Excludes the item with the given numeric item index from the game (e.g. from supply drops / crafting). One index per line; repeat to exclude several items.",
      "enumValues": null,
      "example": "ExcludeItemIndices=1",
      "complex": true
    },
    {
      "key": "PlayerBaseStatMultipliers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "1.0",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the base (level-0) value of each player attribute using indexed bracket syntax [attributeIndex]. Affects the starting stat before any per-level gains. Default multiplier is 1.0 per stat.",
      "enumValues": null,
      "example": "PlayerBaseStatMultipliers[0]=1.5",
      "complex": true
    },
    {
      "key": "PerLevelStatsMultiplier_Player",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "1.0",
      "category": "Players",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the amount of a stat a player gains each time a point is spent in that attribute. Indexed by attribute [0-11]. 1.0 is normal; lower values slow progression, higher values speed it.",
      "enumValues": null,
      "example": "PerLevelStatsMultiplier_Player[0]=1.5",
      "complex": true
    },
    {
      "key": "PerLevelStatsMultiplier_DinoWild",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the per-level stat gain of wild creatures for each attribute, indexed [0-11]. Effectively scales how strong wild dinos become at a given level.",
      "enumValues": null,
      "example": "PerLevelStatsMultiplier_DinoWild[0]=1.5",
      "complex": true
    },
    {
      "key": "PerLevelStatsMultiplier_DinoTamed",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the per-level stat gain of tamed creatures for points spent after taming (the leveling the player does), indexed by attribute [0-11].",
      "enumValues": null,
      "example": "PerLevelStatsMultiplier_DinoTamed[0]=1.5",
      "complex": true
    },
    {
      "key": "PerLevelStatsMultiplier_DinoTamed_Add",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the taming-bonus 'additive' stat component gained on tame (the flat bonus applied when a creature is tamed), indexed by attribute [0-11].",
      "enumValues": null,
      "example": "PerLevelStatsMultiplier_DinoTamed_Add[0]=1.5",
      "complex": true
    },
    {
      "key": "PerLevelStatsMultiplier_DinoTamed_Affinity",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the taming-effectiveness 'affinity' stat component (the bonus scaled by taming effectiveness) gained on tame, indexed by attribute [0-11].",
      "enumValues": null,
      "example": "PerLevelStatsMultiplier_DinoTamed_Affinity[0]=1.5",
      "complex": true
    },
    {
      "key": "HarvestResourceItemAmountClassMultipliers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Harvesting",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Scales the amount of a specific harvested resource item on a per-resource-class basis, on top of the global harvest multiplier. Repeat once per resource class.",
      "enumValues": null,
      "example": "HarvestResourceItemAmountClassMultipliers=(ClassName=\"PrimalItemResource_Metal_C\",Multiplier=2.0)",
      "complex": true
    },
    {
      "key": "DinoSpawnWeightMultipliers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Adjusts how frequently a creature type (by DinoNameTag) spawns relative to others, and optionally caps its share of the population. Higher SpawnWeightMultiplier makes that type appear more often.",
      "enumValues": null,
      "example": "DinoSpawnWeightMultipliers=(DinoNameTag=Bronto,SpawnWeightMultiplier=10.0,OverrideSpawnLimitPercentage=True,SpawnLimitPercentage=0.5)",
      "complex": true
    },
    {
      "key": "NPCReplacements",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Globally replaces one creature class with another (or with nothing, to remove it) wherever it would spawn. Set ToClassName to an empty string to prevent the FromClassName creature from spawning. Reported non-functional in early ASA.",
      "enumValues": null,
      "example": "NPCReplacements=(FromClassName=\"Pegomastax_Character_BP_C\",ToClassName=\"\")",
      "complex": true
    },
    {
      "key": "DinoClassDamageMultipliers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the outgoing damage of a specific WILD creature class. Repeat once per creature class.",
      "enumValues": null,
      "example": "DinoClassDamageMultipliers=(DinoClass=\"Rex_Character_BP_C\",Multiplier=1.5)",
      "complex": true
    },
    {
      "key": "TamedDinoClassDamageMultipliers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the outgoing damage of a specific TAMED creature class. Repeat once per creature class.",
      "enumValues": null,
      "example": "TamedDinoClassDamageMultipliers=(DinoClass=\"Rex_Character_BP_C\",Multiplier=1.2)",
      "complex": true
    },
    {
      "key": "DinoClassResistanceMultipliers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the incoming damage a specific WILD creature class takes (a resistance/damage-received multiplier; lower than 1.0 makes it tankier). Repeat once per creature class.",
      "enumValues": null,
      "example": "DinoClassResistanceMultipliers=(DinoClass=\"Rex_Character_BP_C\",Multiplier=0.5)",
      "complex": true
    },
    {
      "key": "TamedDinoClassResistanceMultipliers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Multiplies the incoming damage a specific TAMED creature class takes. Values below 1.0 make the tamed creature take less damage. Repeat once per creature class.",
      "enumValues": null,
      "example": "TamedDinoClassResistanceMultipliers=(DinoClass=\"Rex_Character_BP_C\",Multiplier=0.8)",
      "complex": true
    },
    {
      "key": "ConfigOverrideItemCraftingCosts",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Fully overrides the crafting ingredient list for one item. BaseCraftingResourceRequirements holds a nested list of resource types, each with a base amount and whether an exact resource type is required. Repeat once per item.",
      "enumValues": null,
      "example": "ConfigOverrideItemCraftingCosts=(ItemClassString=\"PrimalItemResource_Element_Craft_C\",BaseCraftingResourceRequirements=((ResourceItemTypeString=\"PrimalItemResource_BlackPearl_C\",BaseResourceRequirement=1.0,bCraftingRequireExactResourceType=False),(ResourceItemTypeString=\"PrimalItemResource_Crystal_C\",BaseResourceRequirement=5.0,bCraftingRequireExactResourceType=False)))",
      "complex": true
    },
    {
      "key": "ConfigOverrideItemMaxQuantity",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Overrides the maximum stack size of a specific item. bIgnoreMultiplier controls whether the global ItemStackSizeMultiplier is also applied on top. Repeat once per item.",
      "enumValues": null,
      "example": "ConfigOverrideItemMaxQuantity=(ItemClassString=\"PrimalItemAmmo_ArrowTranq_C\",Quantity=(MaxItemQuantity=543,bIgnoreMultiplier=True))",
      "complex": true
    },
    {
      "key": "ConfigOverrideSupplyCrateItems",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Completely rewrites the loot table of a supply crate / beacon. Contains nested ItemSets, each with ItemEntries defining ItemClassStrings, weights, quantity and quality ranges, and blueprint chances. SupplyCrateClassString can be a partial name to affect multiple crates. This is the most complex line and needs a dedicated loot-table editor.",
      "enumValues": null,
      "example": "ConfigOverrideSupplyCrateItems=(SupplyCrateClassString=\"SupplyCrate_Level03_C\",MinItemSets=1,MaxItemSets=1,NumItemSetsPower=1.0,bSetsRandomWithoutReplacement=true,ItemSets=((MinNumItems=2,MaxNumItems=2,NumItemsPower=1.0,SetWeight=1.0,bItemsRandomWithoutReplacement=true,ItemEntries=((EntryWeight=1.0,ItemClassStrings=(\"PrimalItemResource_Stone_C\"),ItemsWeights=(1.0),MinQuantity=10.0,MaxQuantity=10.0,MinQuality=1.0,MaxQuality=1.0,bForceBlueprint=false,ChanceToBeBlueprintOverride=0.0)))))",
      "complex": true
    },
    {
      "key": "ConfigOverrideNPCSpawnEntriesContainer",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Replaces an entire spawn-entries container for a map region: which creatures spawn, their entry weights, and per-entry spawn limits. Overwrites the vanilla container identified by NPCSpawnEntriesContainerClassString. Needs a dedicated spawn-config editor.",
      "enumValues": null,
      "example": "ConfigOverrideNPCSpawnEntriesContainer=(NPCSpawnEntriesContainerClassString=\"DinoSpawnEntriesRender_TheIsland_C\",NPCSpawnEntries=((AnEntryName=\"Dodo\",EntryWeight=10.0,NPCsToSpawnStrings=(\"Dodo_Character_BP_C\"))),NPCSpawnLimits=((NPCClassString=\"Dodo_Character_BP_C\",MaxPercentageOfDesiredNumToAllow=1.0)))",
      "complex": true
    },
    {
      "key": "ConfigAddNPCSpawnEntriesContainer",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Adds new creature spawn entries to an existing container without replacing it, letting you inject extra creatures into a region's spawn table. Same nested structure as the override variant.",
      "enumValues": null,
      "example": "ConfigAddNPCSpawnEntriesContainer=(NPCSpawnEntriesContainerClassString=\"DinoSpawnEntriesRender_TheIsland_C\",NPCSpawnEntries=((AnEntryName=\"Phoenix\",EntryWeight=5.0,NPCsToSpawnStrings=(\"Phoenix_Character_BP_C\"))),NPCSpawnLimits=((NPCClassString=\"Phoenix_Character_BP_C\",MaxPercentageOfDesiredNumToAllow=0.5)))",
      "complex": true
    },
    {
      "key": "ConfigSubtractNPCSpawnEntriesContainer",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Creatures",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Removes specified creature spawn entries from an existing container, letting you delete particular creatures from a region's spawn table. Same container/entry structure as the add/override variants.",
      "enumValues": null,
      "example": "ConfigSubtractNPCSpawnEntriesContainer=(NPCSpawnEntriesContainerClassString=\"DinoSpawnEntriesRender_TheIsland_C\",NPCSpawnEntries=((AnEntryName=\"Rex\",NPCsToSpawnStrings=(\"Rex_Character_BP_C\"))))",
      "complex": true
    },
    {
      "key": "-mods",
      "file": "CommandLine",
      "section": "",
      "type": "array",
      "default": "",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "Comma-separated list of CurseForge Mod Project IDs to load; replaces the Steam Workshop mod system of ASE. Mods are downloaded and updated automatically at server start.",
      "enumValues": null,
      "example": "-mods=927090,928988",
      "complex": false
    },
    {
      "key": "-passivemods",
      "file": "CommandLine",
      "section": "",
      "type": "array",
      "default": "",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "Loads the listed CurseForge mods' data without enabling their functionality, e.g. so modded creatures can transfer through a map without spawning there. Downloads the mod itself; it does not need to also be in -mods.",
      "enumValues": null,
      "example": "-passivemods=927090",
      "complex": false
    },
    {
      "key": "-ServerPlatform",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "ALL",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Restricts which platforms may join: PC (Steam), PS5, XSX (Xbox), WINGDK (Microsoft Store) or ALL for full crossplay. Combine with +. Replaces ASE's -crossplay/-epiconly flags.",
      "enumValues": null,
      "example": "-ServerPlatform=PC+XSX+PS5",
      "complex": false
    },
    {
      "key": "-WinLiveMaxPlayers",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "70",
      "category": "Players",
      "games": [
        "ASA"
      ],
      "description": "Sets the maximum concurrent player count in ASA. Replaces the MaxPlayers ini setting from ASE, which is ignored/reset in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-port",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "7777",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "UDP game port the ASA dedicated server listens on; required to run on a port other than 7777. (ASE used ?Port= append syntax instead, which ASA no longer supports.)",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-GBUsageToForceRestart",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "35",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Out-of-memory protection: when server memory use reaches this many gigabytes, the world is force-saved and the server restarts to avoid crashes/rollbacks. 0 disables.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-AlwaysTickDedicatedSkeletalMeshes",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Disables the optimization that stops ticking idle creature animations when server FPS is low. Fixes client/server collision mismatches on idle creatures at a performance cost.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-disabledinonetrangescaling",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Disables the dynamic scaling of creature network replication range that ASA applies as player count grows. Improves replication accuracy at a performance cost on busy servers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-DoCustomCosmeticValidation",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Engrams & Items",
      "games": [
        "ASA"
      ],
      "description": "Enables server-side validation of Custom Cosmetic mods (skin-only mods clients download automatically).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-DisableCustomCosmetics",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Engrams & Items",
      "games": [
        "ASA"
      ],
      "description": "Turns off ASA's Custom Cosmetic system entirely, preventing players from using auto-downloaded cosmetic-only mods.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NoWildBabies",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Breeding",
      "games": [
        "ASA"
      ],
      "description": "Disables the wild baby creatures that spawn naturally in ASA (a feature that does not exist in ASE).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-EasterColors",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Gives wild creatures a chance to spawn with Easter event colors.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-ServerUseEventColors",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Enables active event color sets on wild creature spawns.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-disableCharacterTracker",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Players",
      "games": [
        "ASA"
      ],
      "description": "Disables ASA's character tracking feature. Overrides the UseCharacterTracker ini setting. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-EnableSteelShield",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Enables Nitrado SteelShield anti-DDoS protection; only functional on Nitrado-hosted servers. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-forceuseperfthreads",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Forces the server to use performance threads. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-noperfthreads",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Disables performance threads on the ASA server. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-onethread",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Disables multithreading on the ASA server. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-nosound",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Disables sound processing on the dedicated server to improve performance. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NoAI",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Prevents AI controllers from being attached to creatures (creatures spawn but do not act). Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-ip",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Binds the server to a specific IPv4 address; seen in Nitrado setups, expected to work in a -MULTIHOME context. Undocumented by Wildcard.",
      "enumValues": null,
      "example": "-ip=192.168.1.10",
      "complex": false
    },
    {
      "key": "-ServerIP",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Alternative IPv4 bind option found in Nitrado server setups, expected to work in a -MULTIHOME context. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NitradoQueryPort",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Network & Performance",
      "games": [
        "ASA"
      ],
      "description": "Nitrado-specific query port option; exact behavior unknown and undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NoDinosExceptForcedSpawn",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Prevents wild creature spawning except forced spawns. Mutually exclusive with -NoDinos and the other -NoDinosExcept* options. Existing wild creatures are not auto-destroyed.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NoDinosExceptStreamingSpawn",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Prevents wild creature spawning except streaming spawns. Mutually exclusive with -NoDinos and the other -NoDinosExcept* options.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NoDinosExceptManualSpawn",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Prevents wild creature spawning except manual spawns. Mutually exclusive with -NoDinos and the other -NoDinosExcept* options.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NoDinosExceptWaterSpawn",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Prevents wild creature spawning except water spawns. Mutually exclusive with -NoDinos and the other -NoDinosExcept* options.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-ForceIgnoreSingleplayerSpawnRangeCheck",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Makes single-player/non-dedicated sessions spawn creatures across the whole map like a dedicated server, even when no player is nearby. May cost performance and memory.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ActiveMapMod",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "string",
      "default": "",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "CurseForge mod ID of the custom mod map the ASA server should load.",
      "enumValues": null,
      "example": "ActiveMapMod=123456",
      "complex": false
    },
    {
      "key": "AllowCryoFridgeOnSaddle",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, cryofridges can be built on platform saddles and rafts.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ArmadoggoDeathCooldown",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "3600",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Seconds before an Armadoggo reappears after taking fatal damage (default 1 hour). Must be greater than 0.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "YoungIceFoxDeathCooldown",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "3600",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Seconds before a Veilwyn (young ice fox) reappears after taking fatal damage (default 1 hour). Must be greater than 0.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CosmeticWhitelistOverride",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "string",
      "default": "",
      "category": "Engrams & Items",
      "games": [
        "ASA"
      ],
      "description": "URL to a comma-separated whitelist of allowed Custom Cosmetic mods (mod ID, dynamic-download flag, allow non-dataonly blueprints flag).",
      "enumValues": null,
      "example": "CosmeticWhitelistOverride=\"https://example.com/cosmetics.txt\"",
      "complex": false
    },
    {
      "key": "CosmoWeaponAmmoReloadAmount",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "How much ammo the Cosmo's webslinger regains per reload tick.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxCosmoWeaponAmmo",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "-1",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Fixed maximum ammo for the Cosmo's webslinger. -1 keeps the default behavior of scaling with the Cosmo's level.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DestroyTamesOverTheSoftTameLimit",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Enables the soft tame limit: tames above the limit are flagged 'For Cryo' with a countdown and are auto-destroyed (and tribe-logged) if not cryopodded in time, while still letting new players tame past the server cap.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxTamedDinos_SoftTameLimit",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "5000",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Server-wide soft tame limit used by DestroyTamesOverTheSoftTameLimit.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxTamedDinos_SoftTameLimit_CountdownForDeletionDuration",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "604800",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Seconds a tame flagged over the soft tame limit has before it is automatically destroyed (default 7 days).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DisableBurrowDecayTimers",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Decay",
      "games": [
        "ASA"
      ],
      "description": "If true, completely disables decay timers on Burrowbuck burrows.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DisableCryopodEnemyCheck",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "If true, cryopods can be thrown/used even while enemies are nearby.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DisableCryopodFridgeRequirement",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "If true, cryopods work without being in range of a powered cryofridge.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ForceGachaUnhappyInCaves",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "True",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "If true, Gachas become unhappy (unproductive) while inside caves.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ImplantSuicideCD",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "28800",
      "category": "Players",
      "games": [
        "ASA"
      ],
      "description": "Cooldown in seconds between uses of the implant's respawn (suicide) feature (default 8 hours).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxTrainCars",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "8",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Maximum number of carts a train can pull.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "UseCharacterTracker",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Players",
      "games": [
        "ASA"
      ],
      "description": "Enables ASA's character tracker. The -disableCharacterTracker command-line flag overrides this. Undocumented by Wildcard.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AdminListURL",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASA"
      ],
      "description": "Web-hosted alternative to AllowedCheaterAccountIDs.txt for admin whitelisting; polled at the interval set by UpdateAllowedCheatersInterval. Replaces ASE's AllowedCheatersURL. Undocumented by Wildcard.",
      "enumValues": null,
      "example": "AdminListURL=\"https://example.com/admins.txt\"",
      "complex": false
    },
    {
      "key": "AutoRestartIntervalSeconds",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASA"
      ],
      "description": "Time in seconds after which the server automatically restarts. Undocumented by Wildcard; reportedly shuts the server down rather than restarting it properly.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "LimitBunkersPerTribe",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "True",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, limits how many Lost Colony bunkers a tribe can own (count set by LimitBunkersPerTribeNum). Added in ASA 70.0; not yet documented in detail.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "LimitBunkersPerTribeNum",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "3",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Maximum number of Lost Colony bunkers per tribe when LimitBunkersPerTribe is enabled.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowBunkersInPreventionZones",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, Lost Colony bunkers may be claimed/placed inside building prevention zones.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowRidingDinosInsideBunkers",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "True",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, players may ride creatures while inside Lost Colony bunkers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowBunkerModulesAboveGround",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, bunker modules can be placed above ground instead of only underground.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowDinoAIInsideBunkers",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "True",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, creature AI remains active inside Lost Colony bunkers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowBunkerModulesInPreventionZones",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, bunker modules can be placed inside building prevention zones.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MinDistanceBetweenBunkers",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "3000.0",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Minimum distance (Unreal units) required between two Lost Colony bunkers.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "EnemyAccessBunkerHPThreshold",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "0.25",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Bunker HP fraction (0-1) below which enemies gain access to the bunker.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BunkerUnderHPThresholdDmgMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "0.05",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Damage multiplier applied to a bunker once it is below the enemy-access HP threshold.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CryoHospitalHoursToRegenHP",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Hours for a creature stored in the Cryo Hospital to fully regenerate health (Lost Colony structure).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CryoHospitalHoursToRegenFood",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "24.0",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Hours for a creature stored in the Cryo Hospital to fully regenerate food.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CryoHospitalHoursToDrainTorpor",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Creatures",
      "games": [
        "ASA"
      ],
      "description": "Hours for the Cryo Hospital to fully drain a stored creature's torpor.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CryoHospitalMatingCooldownReduction",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "2.0",
      "category": "Breeding",
      "games": [
        "ASA"
      ],
      "description": "Factor by which the Cryo Hospital accelerates/reduces mating cooldown of stored creatures.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BloodforgeReinforceExtraDurability",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "0.3",
      "category": "Engrams & Items",
      "games": [
        "ASA"
      ],
      "description": "Extra durability fraction granted when reinforcing an item at the Bloodforge (Lost Colony structure).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BloodforgeReinforceResourceCostMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "3.0",
      "category": "Engrams & Items",
      "games": [
        "ASA"
      ],
      "description": "Multiplier on the resource cost of Bloodforge item reinforcement.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "BloodforgeReinforceSpeedMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "0.1",
      "category": "Engrams & Items",
      "games": [
        "ASA"
      ],
      "description": "Multiplier on the speed of Bloodforge item reinforcement.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxActiveOutposts",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "Maximum number of simultaneously active outposts (Lost Colony). Default not documented.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxActiveResourceCaches",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "Maximum number of simultaneously active resource caches (Lost Colony). Default not documented.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "MaxActiveCityOutposts",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "int",
      "default": "",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "Maximum number of simultaneously active city outposts (Lost Colony). Default not documented.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "OutpostSigilRewardMultiplier",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "float",
      "default": "1.0",
      "category": "Rates",
      "games": [
        "ASA"
      ],
      "description": "Scales the number of sigils rewarded by outpost missions; higher values grant more sigils.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bAllowSpeedLeveling",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "Players",
      "games": [
        "ASA"
      ],
      "description": "Allows players and non-flyer creatures to level up Movement Speed, which ASA disables by default (in ASE this was always possible).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisablePhotoMode",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "If true, disables ASA's photo mode on the server.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PhotoModeRangeLimit",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "3000",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "Maximum distance the photo mode camera may move away from the player.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableWirelessCrafting",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, disables wireless crafting from Tek Dedicated Storage entirely.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableWirelessCraftingForDinos",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, blocks wireless crafting from Tek Dedicated Storage when crafting in creature inventories.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableWirelessCraftingForPlayers",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, blocks wireless crafting from Tek Dedicated Storage when crafting in the player inventory.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "bDisableWirelessCraftingForStructures",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "false",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "If true, blocks wireless crafting from Tek Dedicated Storage when crafting in structure inventories.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "WirelessCraftingRangeOverride",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "3000",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Wireless crafting range (Unreal units) of the Tek Dedicated Storage.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CheatTeleportLocations",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASA"
      ],
      "description": "Defines named teleport targets usable with the cheat TP command; coordinates are in Unreal units. One line per location, repeatable.",
      "enumValues": null,
      "example": "CheatTeleportLocations=(TeleportName=\"Hightower\",TeleportLocation=(X=467967.0,Y=-359082.0,Z=6879.0))",
      "complex": true
    },
    {
      "key": "LimitGeneratorsNum",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "3",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Maximum number of (Tek) generators allowed within the radius set by LimitGeneratorsRange. Official ASA servers use 3.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "LimitGeneratorsRange",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "int",
      "default": "15000",
      "category": "Structures",
      "games": [
        "ASA"
      ],
      "description": "Radius in Unreal units within which LimitGeneratorsNum applies. Official ASA servers use 15000.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "IgnorePVPMountedWeaponryRestrictions",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "bool",
      "default": "False",
      "category": "PvP",
      "games": [
        "ASA"
      ],
      "description": "If true, lifts the PvP restrictions on using weaponry while mounted.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "TribeTowerBonusMultiplier",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "float",
      "default": "2.0",
      "category": "Tribes",
      "games": [
        "ASA"
      ],
      "description": "Multiplier applied to the Tribe Tower bonus.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ValgueroMemorialEntries",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "string",
      "default": "",
      "category": "General",
      "games": [
        "ASA"
      ],
      "description": "Semicolon-separated list of player names shown on the interactable Valguero Memorial honoring ascended players.",
      "enumValues": null,
      "example": "ValgueroMemorialEntries=Name1;Name2;Name3;",
      "complex": false
    },
    {
      "key": "BanListURL",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "string",
      "default": "https://cdn2.arkdedicated.com/asa/BanList.txt",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "URL of a global ban list fetched every 10 minutes; must be quoted. ASA default/official list is https://cdn2.arkdedicated.com/asa/BanList.txt and supports HTTPS; ASE used http://arkdedicated.com/banlist.txt (HTTP only).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "CustomLiveTuningUrl",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Direct link to a live-tuning overrides file (Wildcard remote balance tweaks). Leave it unset to keep ARK built-in source. The ASE built-in file is http://arkdedicated.com/DefaultOverloads.json (HTTP only, verified reachable). The https://cdn2.arkdedicated.com/asa/ URL previously listed here answers with an EMPTY document, so no verified ASA URL is known - set one only if you host your own overrides file.",
      "enumValues": null,
      "example": "http://arkdedicated.com/DefaultOverloads.json",
      "complex": false
    },
    {
      "key": "?AltSaveDirectoryName",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Custom directory name for the world save, typically used for clusters. In ASA the save still nests as <savedir_name>/<map_name> instead of a flat directory.",
      "enumValues": null,
      "example": "?AltSaveDirectoryName=Cluster1",
      "complex": false
    },
    {
      "key": "-ActiveEvent",
      "file": "CommandLine",
      "section": "",
      "type": "enum",
      "default": "",
      "category": "General",
      "games": [
        "ASE",
        "ASA"
      ],
      "description": "Activates a seasonal event or its color palette on wild creatures (ASE). In ASA this option is obsolete - events are enabled via the -mods argument instead.",
      "enumValues": [
        "Easter",
        "FearEvolved",
        "PAX",
        "Summer",
        "TurkeyTrial",
        "vday",
        "WinterWonderland",
        "birthday",
        "ark7th",
        "ARKaeology",
        "ExtinctionChronicles",
        "None"
      ],
      "example": null,
      "complex": false
    },
    {
      "key": "-crossplay",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "ASE: enables Epic+Steam crossplay (requires PublicIPForEpic). Removed in ASA - use -ServerPlatform=ALL instead.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-epiconly",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "ASE: restricts the server to Epic Games Store players. Obsolete in ASA, which is not available on Epic.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-PublicIPForEpic",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "ASE: public IP that Epic Games Store clients connect to when using crossplay/multihome. Not used in ASA (no Epic support).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-UseVivox",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "ASE: enables Vivox voice chat on Steam-only servers. Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-automanagedmods",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE Steam-only automatic mod download/install/update using [ModInstaller] IDs from Game.ini. Replaced in ASA by the CurseForge -mods system.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "?GameModIds",
      "file": "CommandLine",
      "section": "",
      "type": "array",
      "default": "",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE: comma-separated Steam Workshop mod IDs in load order (left = highest priority). Replaced in ASA by -mods with CurseForge IDs.",
      "enumValues": null,
      "example": "?GameModIds=731604991,916417001",
      "complex": false
    },
    {
      "key": "-MapModID",
      "file": "CommandLine",
      "section": "",
      "type": "string",
      "default": "",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE: loads a custom map directly via its Steam Workshop mod ID. Replaced in ASA by -mods/ActiveMapMod.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "ActiveTotalConversion",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "string",
      "default": "",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE: activates a total-conversion mod (e.g. Primitive Plus) by mod ID; equivalent to -TotalConversionMod. Not present in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-insecure",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE: disables the Valve Anti-Cheat (VAC) system. Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-newsaveformat",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE: switches to save format version 11 (faster, smaller cryopod saving); required together with -usestore to load the official 2023 server save backups. ASA uses the new format natively.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-AllowFlyerSpeedLeveling",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Creatures",
      "games": [
        "ASE"
      ],
      "description": "ASE: enables Movement Speed level-ups for flyers (same as bAllowFlyerSpeedLeveling=True in Game.ini). Removed in ASA - use the Game.ini settings instead.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-NewYearEvent",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE: enables the Happy New Year! event, triggering at midnight/noon EST on January 1st. Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "?NewYear1UTC",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE: epoch timestamp overriding the first New Year event trigger time (must be after Jan 1 midnight EST). Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "?NewYear2UTC",
      "file": "CommandLine",
      "section": "",
      "type": "int",
      "default": "",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE: epoch timestamp overriding the second New Year event trigger time. Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-ServerAllowAnsel",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE: allows connected clients to use NVIDIA Ansel photography. Not available in ASA (which has a native photo mode).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-allowansel",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "General",
      "games": [
        "ASE"
      ],
      "description": "ASE client option activating NVIDIA Ansel support in single player. Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "?bRawSockets",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "ASE (deprecated since 311.78): direct UDP socket connections instead of Steam P2P. Never existed in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-forcenetthreading",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "ASE (deprecated since 311.78): forced threaded networking for ?bRawSockets servers. Never existed in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-nonetthreading",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Network & Performance",
      "games": [
        "ASE"
      ],
      "description": "ASE (deprecated since 271.17): limited ?bRawSockets servers to a single network thread. Never existed in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-d3d10",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE client: forces DirectX 10 / shader model 4 rendering (aliases -dx10, -sm4) for weaker hardware. Not applicable to ASA (UE5).",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-d3d12",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE client (deprecated): forced the DirectX 12 back-end (alias -dx12); now crashes the ASE client and does not exist for ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-noaafonts",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE (deprecated): removed font anti-aliasing. Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-nosteamclient",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE: used internally when the client launches a non-dedicated host; no effect on dedicated servers. Not available in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "-USEALLAVAILABLECORES",
      "file": "CommandLine",
      "section": "",
      "type": "bool",
      "default": "false",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "Unreal Engine cooking/DevKit parameter, useless for ARK servers and clients; listed for ASE only. Not applicable to ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "DestroyUnconnectedWaterPipes",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "bool",
      "default": "False",
      "category": "Structures",
      "games": [
        "ASE"
      ],
      "description": "ASE: auto-destroys water pipes that stay unconnected for two real-time days with no allied player nearby. Not present in ASA.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "AllowedCheatersURL",
      "file": "GameUserSettings",
      "section": "ServerSettings",
      "type": "string",
      "default": "",
      "category": "Advanced",
      "games": [
        "ASE"
      ],
      "description": "ASE: web-hosted admin (cheater) whitelist URL. Replaced in ASA by AdminListURL.",
      "enumValues": null,
      "example": null,
      "complex": false
    },
    {
      "key": "PreventTransferForClassNames",
      "file": "Game",
      "section": "/Script/ShooterGame.ShooterGameMode",
      "type": "array",
      "default": "",
      "category": "Cross-ARK/Transfers",
      "games": [
        "ASE"
      ],
      "description": "ASE: blocks cross-server transfer of specific creatures by class name; one repeatable line per class. Marked unavailable in ASA on the wiki.",
      "enumValues": null,
      "example": "PreventTransferForClassNames=\"Argent_Character_BP_C\"",
      "complex": true
    }
  ]
}
;
