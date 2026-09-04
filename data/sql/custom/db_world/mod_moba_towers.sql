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

DELETE FROM `creature_template` WHERE (`entry` BETWEEN 900000 AND 900009 OR `entry` BETWEEN 900400 AND 900499);
INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`,
 `type`, `type_flags`, `MovementType`, `HealthModifier`, `ArmorModifier`,
 `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
(900006, 'Alliance Outer Tower', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900406, 'Alliance Inner Tower', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900007, 'Alliance Outer Tower', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900407, 'Alliance Inner Tower', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900008, 'Alliance Inhibitor', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 10, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900009, 'Alliance Inhibitor', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 10, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900400, 'Alliance Base', 'MOBA Objective', 80, 80, 84, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 15, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900401, 'Horde Outer Tower', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900408, 'Horde Inner Tower', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900402, 'Horde Outer Tower', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900409, 'Horde Inner Tower', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 8, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900403, 'Horde Inhibitor', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 10, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900404, 'Horde Inhibitor', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 10, 5, 0, 0, 0, 'npc_moba_tower', 0),
(900405, 'Horde Base', 'MOBA Objective', 80, 80, 83, 0, 1.0, 1.14286, 1, 1, 32768, 2048, 9, 0, 0, 15, 5, 0, 0, 0, 'npc_moba_tower', 0);

-- Structures spawn from C++, never from `creature` rows, so this normally
-- deletes nothing. It sweeps GM `.npc add` test spawns. The spawn table's
-- entry column is `id`, not `id1`: upstream 2026_06_16_00.sql renamed it.
DELETE FROM `creature` WHERE (`id` BETWEEN 900000 AND 900009 OR `id` BETWEEN 900400 AND 900499);

DELETE FROM `creature_template_model` WHERE (`CreatureID` BETWEEN 900000 AND 900009 OR `CreatureID` BETWEEN 900400 AND 900499);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900006, 0, 27101, 3.0, 1, 0),
(900406, 0, 27101, 3.0, 1, 0),
(900007, 0, 27101, 3.0, 1, 0),
(900407, 0, 27101, 3.0, 1, 0),
(900008, 0, 1460, 0.75, 1, 0),
(900009, 0, 1460, 0.75, 1, 0),
(900400, 0, 11659, 2.0, 1, 0),
(900401, 0, 18505, 2.0, 1, 0),
(900408, 0, 18505, 2.0, 1, 0),
(900402, 0, 18505, 2.0, 1, 0),
(900409, 0, 18505, 2.0, 1, 0),
(900403, 0, 1461, 0.75, 1, 0),
(900404, 0, 1461, 0.75, 1, 0),
(900405, 0, 11659, 2.0, 1, 0);

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
(900006, 900, 0, 0, 1, 0, 0, 0, -50.83, 50.63, 0.0, 5.8423, 40, 1500, 9053, 3000, 1500),
(900406, 900, 0, 0, 1, 900006, 0, 0, -112.57, 76.65, 0.0, 6.0756, 40, 1500, 9053, 3000, 1500),
(900007, 900, 0, 0, 3, 0, 0, 0, -55.6, -73.5, 0.0, 0.2628, 40, 1500, 9053, 3000, 1500),
(900407, 900, 0, 0, 3, 900007, 0, 0, -107.07, -79.51, 0.0, 0.0368, 40, 1500, 9053, 3000, 1500),
(900008, 900, 0, 1, 1, 900406, 1, 120000, -160.2, 60.19, 3.0, 0.6884, 0, 1500, 0, 0, 1500),
(900009, 900, 0, 1, 3, 900407, 1, 120000, -161.53, -61.42, 3.0, 5.536, 0, 1500, 0, 0, 1500),
(900400, 900, 0, 2, 0, 0, 2, 0, -132.84, -0.26, 3.0, 0.0, 0, 1500, 0, 0, 0),
(900401, 900, 1, 0, 1, 0, 0, 0, 50.83, 50.63, 0.0, 3.5825, 40, 1500, 9053, 3000, 1500),
(900408, 900, 1, 0, 1, 900401, 0, 0, 112.57, 76.65, 0.0, 3.3492, 40, 1500, 9053, 3000, 1500),
(900402, 900, 1, 0, 3, 0, 0, 0, 55.6, -73.5, 0.0, 2.8788, 40, 1500, 9053, 3000, 1500),
(900409, 900, 1, 0, 3, 900402, 0, 0, 107.07, -79.51, 0.0, 3.1048, 40, 1500, 9053, 3000, 1500),
(900403, 900, 1, 1, 1, 900408, 1, 120000, 160.2, 60.19, 3.0, 2.4532, 0, 1500, 0, 0, 1500),
(900404, 900, 1, 1, 3, 900409, 1, 120000, 161.53, -61.42, 3.0, 3.8888, 0, 1500, 0, 0, 1500),
(900405, 900, 1, 2, 0, 0, 2, 0, 132.84, -0.26, 3.0, 3.1416, 0, 1500, 0, 0, 0);
