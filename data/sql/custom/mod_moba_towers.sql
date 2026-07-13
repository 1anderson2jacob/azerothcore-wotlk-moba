USE acore_world;

DELETE FROM `creature_template` WHERE `entry` IN (900000, 900001);

INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`,
 `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `unit_flags2`,
 `type`, `type_flags`, `MovementType`, `HealthModifier`, `ArmorModifier`,
 `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
(900000, 'Alliance Tower', 'MOBA Objective', 80, 80, 84, 0,
 1.0, 1.14286, 1, 1, 32768, 2048,
 9, 0, 0, 100, 5,
 0, 0, 0, 'npc_moba_tower', 0),
(900001, 'Horde Tower', 'MOBA Objective', 80, 80, 83, 0,
 1.0, 1.14286, 1, 1, 32768, 2048,
 9, 0, 0, 100, 5,
 0, 0, 0, 'npc_moba_tower', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (900000, 900001);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900000, 0, 27101, 3.0, 1, 0),
(900001, 0, 18505, 2.0, 1, 0);

UPDATE battleground_template SET AllianceStartLoc = 1103, HordeStartLoc = 1104 WHERE ID = 7;
UPDATE game_graveyard SET x = 2387.529, y = 1587.426, z = 1174.763 WHERE ID = 1103;
UPDATE game_graveyard SET x = 1942.9327, y = 1547.6229, z = 1176.458 WHERE ID = 1104;

-- Tower registry: spawn position, tier/guard dependency, and per-tower AI config.
-- GuardedByEntry = 0 means always vulnerable; otherwise this tower is inert
-- (UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE) until the referenced
-- tower entry is destroyed. GuardedByEntry is a single FK (one guard per
-- guarded tower) -- sufficient for a linear lane, not multi-guard AND-gating.
DROP TABLE IF EXISTS `mod_moba_tower_data`;
CREATE TABLE `mod_moba_tower_data` (
    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,
    `Map`              INT UNSIGNED NOT NULL,               -- BG map id (566 = hijacked EotS)
    `Team`             TINYINT UNSIGNED NOT NULL,           -- TeamId: 0 = Alliance, 1 = Horde
    `Tier`             TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `GuardedByEntry`   INT UNSIGNED NOT NULL DEFAULT 0,      -- 0 = none / always vulnerable
    `PosX`             FLOAT NOT NULL,
    `PosY`             FLOAT NOT NULL,
    `PosZ`             FLOAT NOT NULL,
    `Orientation`      FLOAT NOT NULL,
    `AttackRange`      FLOAT NOT NULL DEFAULT 40,
    `AttackIntervalMs` INT UNSIGNED NOT NULL DEFAULT 1500,
    `AttackSpellId`    INT UNSIGNED NOT NULL DEFAULT 9053
);

DELETE FROM `mod_moba_tower_data` WHERE `CreatureEntry` IN (900000, 900001);
INSERT INTO `mod_moba_tower_data`
(`CreatureEntry`, `Map`, `Team`, `Tier`, `GuardedByEntry`, `PosX`, `PosY`, `PosZ`, `Orientation`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`)
VALUES
(900000, 566, 0, 0, 0, 2285.5596, 1587.9965, 1165.4397, 3.2774656, 40, 1500, 9053),
(900001, 566, 1, 0, 0, 2056.0195, 1547.1702, 1162.6882, 0.21284086, 40, 1500, 9053);


-- Tower health: was 100x level-80 baseline (absurd). First-pass tuning to 8x --
-- still a real objective, not a 2-second delete. Adjust after testing.
UPDATE creature_template SET HealthModifier = 8 WHERE entry IN (900000, 900001);
