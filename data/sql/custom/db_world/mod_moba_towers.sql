-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_tower_data.py from apps/moba/maps/*/tower_config.yaml.
-- Owns the structure creatures (creature_template + creature_template_model)
-- AND their per-map placement (mod_moba_tower_data) -- there is no separate
-- hand-written defs file. Fixed creature invariants (level 80, unit flags,
-- npc_moba_tower script) are enforced in the generator; name/faction/health/
-- armor/display/scale are config fields.
-- Kind: 0 = tower (attacks), 1 = inhibitor (passive; grants super minions
-- and respawns after RespawnMs on death), 2 = core/base (passive; its
-- destruction wins the match). RespawnMs applies to inhibitors; 0 = never.
-- Lane: 0 = none (cores), 1 = top, 2 = mid, 3 = bot. Kill-feed wording only.
-- ============================================================

USE acore_world;

DELETE FROM `creature_template` WHERE `entry` BETWEEN 900000 AND 900009;
INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`,
 `type`, `type_flags`, `MovementType`, `HealthModifier`, `ArmorModifier`,
 `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
(900000, 'Alliance Tower', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900002, 'Alliance Inhibitor', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 10, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900004, 'Alliance Base', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 15, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900001, 'Horde Tower', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900003, 'Horde Inhibitor', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 10, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900005, 'Horde Base', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 15, 5, 0, 0, 0, 'npc_moba_tower', 0);

-- Structures spawn from C++, never from `creature` rows, so this normally
-- deletes nothing. It sweeps GM `.npc add` test spawns. The spawn table's
-- entry column is `id`, not `id1`: upstream 2026_06_16_00.sql renamed it.
DELETE FROM `creature` WHERE `id` BETWEEN 900000 AND 900009;

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 900000 AND 900009;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900000, 0, 27101, 3.0, 1, 0),
(900002, 0, 1460, 0.75, 1, 0),
(900004, 0, 11659, 2.0, 1, 0),
(900001, 0, 18505, 2.0, 1, 0),
(900003, 0, 1461, 0.75, 1, 0),
(900005, 0, 11659, 2.0, 1, 0);

DROP TABLE IF EXISTS `mod_moba_tower_data`;
CREATE TABLE `mod_moba_tower_data` (
    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,
    `Map`              INT UNSIGNED NOT NULL,               -- BG map id
    `Team`             TINYINT UNSIGNED NOT NULL,           -- 0 = Alliance, 1 = Horde
    `Tier`             TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `Lane`             TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- 0 none, 1 top, 2 mid, 3 bot
    `GuardedByEntry`   INT UNSIGNED NOT NULL DEFAULT 0,      -- 0 = none / always vulnerable
    `Kind`             TINYINT UNSIGNED NOT NULL DEFAULT 0,  -- 0 tower, 1 inhibitor, 2 core
    `RespawnMs`        INT UNSIGNED NOT NULL DEFAULT 0,      -- inhibitor respawn delay; 0 = never
    `PosX`             FLOAT NOT NULL,
    `PosY`             FLOAT NOT NULL,
    `PosZ`             FLOAT NOT NULL,
    `Orientation`      FLOAT NOT NULL,
    `AttackRange`      FLOAT NOT NULL DEFAULT 40,
    `AttackIntervalMs` INT UNSIGNED NOT NULL DEFAULT 1500,
    `AttackSpellId`    INT UNSIGNED NOT NULL DEFAULT 9053,
    `TeamGold`         INT UNSIGNED NOT NULL DEFAULT 0,      -- copper to EVERY player on the destroying team
    `LastHitGold`      INT UNSIGNED NOT NULL DEFAULT 0       -- copper to the killing-blow player only
);

INSERT INTO `mod_moba_tower_data`
(`CreatureEntry`, `Map`, `Team`, `Tier`, `Lane`, `GuardedByEntry`, `Kind`, `RespawnMs`, `PosX`, `PosY`, `PosZ`, `Orientation`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`, `TeamGold`, `LastHitGold`)
VALUES
(900000, 566, 0, 0, 2, 0, 0, 0, 2285.5596, 1587.9965, 1165.4397, 3.2774656, 40, 1500, 9053, 3000, 1500),
(900002, 566, 0, 1, 2, 900000, 1, 120000, 2320.745, 1584.3153, 1169.2806, 4.106839, 0, 1500, 0, 0, 1500),
(900004, 566, 0, 2, 0, 900002, 2, 0, 2354.7302, 1587.6167, 1171.2659, 0.89613223, 0, 1500, 0, 0, 0),
(900001, 566, 1, 0, 2, 0, 0, 0, 2056.0195, 1547.1702, 1162.6882, 0.21284086, 40, 1500, 9053, 3000, 1500),
(900003, 566, 1, 1, 2, 900001, 1, 120000, 2018.5479, 1549.687, 1168.0171, 0.04476848, 0, 1500, 0, 0, 1500),
(900005, 566, 1, 2, 0, 900003, 2, 0, 1983.9092, 1547.2158, 1170.3706, 5.8048787, 0, 1500, 0, 0, 0);
