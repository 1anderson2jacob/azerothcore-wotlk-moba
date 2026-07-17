-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_base.py from apps/moba/maps/*/base_config.json.
-- Tunables live in mod_moba_base; the base LOCATION and RADIUS are written
-- into game_graveyard / battleground_template (read at runtime via
-- GetTeamStartPosition / GetClosestGraveyard / GetStartMaxDist).
-- ============================================================

USE acore_world;

DROP TABLE IF EXISTS `mod_moba_base`;
CREATE TABLE `mod_moba_base` (
    `Map`             INT UNSIGNED NOT NULL PRIMARY KEY,      -- BG map id
    `RespawnBaseMs`   INT UNSIGNED NOT NULL DEFAULT 10000,    -- base respawn wait
    `RespawnPerMinMs` INT UNSIGNED NOT NULL DEFAULT 1500,     -- added per elapsed match-minute
    `RespawnCapMs`    INT UNSIGNED NOT NULL DEFAULT 60000,    -- maximum respawn wait
    `RecallCastMs`          INT UNSIGNED NOT NULL DEFAULT 0,  -- recall cast time (ms); 0 = spell default
    `RecallEmpoweredCastMs` INT UNSIGNED NOT NULL DEFAULT 0,  -- empowered recall cast time (ms); 0 = fall back to normal
    `FountainTickMs`  INT UNSIGNED NOT NULL DEFAULT 0,        -- fountain heal cadence (ms); 0 = fountain healing off
    `FountainHpPct`   INT UNSIGNED NOT NULL DEFAULT 0,        -- % of max health restored per tick
    `FountainManaPct` INT UNSIGNED NOT NULL DEFAULT 0         -- % of max mana restored per tick (mana users only)
);

INSERT INTO `mod_moba_base` (`Map`, `RespawnBaseMs`, `RespawnPerMinMs`, `RespawnCapMs`, `RecallCastMs`, `RecallEmpoweredCastMs`, `FountainTickMs`, `FountainHpPct`, `FountainManaPct`)
VALUES
(566, 10000, 1500, 60000, 9000, 4500, 1000, 10, 10);

-- Spawn wiring for map 566 (eye_of_the_storm)
-- StartMaxDist is the base bubble: the core's prep-phase leash AND the fountain heal zone.
UPDATE battleground_template SET AllianceStartLoc = 1103, AllianceStartO = 3.0222116, HordeStartLoc = 1104, HordeStartO = 0.32122585, StartMaxDist = 10.0 WHERE ID = 7;
UPDATE game_graveyard SET x = 2387.529, y = 1587.426, z = 1174.763 WHERE ID = 1103;
UPDATE game_graveyard SET x = 1942.9327, y = 1547.6229, z = 1176.458 WHERE ID = 1104;
