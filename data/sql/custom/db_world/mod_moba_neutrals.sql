-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_neutral_camps.py from apps/moba/maps/*/neutral_config.json.
-- Stats are full copies of real source creatures with a fixed override
-- list enforced in code -- see the generator's docstring.
-- ============================================================

USE acore_world;

DELETE FROM `creature_template` WHERE `entry` IN (900200, 900201, 900202, 900203, 900204, 900205, 900206, 900207);
INSERT INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
-- worg_large (from creature_template_2279.txt)
(900200,0,0,0,0,0,'Ravenous Worg','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.6,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- worg_small (from creature_template_2279.txt)
(900201,0,0,0,0,0,'Worg Pup','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.2,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- wyrm_large (from creature_template_2279.txt)
(900202,0,0,0,0,0,'Greater Mana Wyrm','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.6,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- wyrm_small (from creature_template_2279.txt)
(900203,0,0,0,0,0,'Mana Wyrmling','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.2,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- ray_large (from creature_template_2279.txt)
(900204,0,0,0,0,0,'Nether Ray Matriarch','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.6,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- ray_small (from creature_template_2279.txt)
(900205,0,0,0,0,0,'Nether Ray','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.2,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- boar_large (from creature_template_2279.txt)
(900206,0,0,0,0,0,'Thornfang Boar','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.6,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- boar_small (from creature_template_2279.txt)
(900207,0,0,0,0,0,'Young Boar','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.2,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (900200, 900201, 900202, 900203, 900204, 900205, 900206, 900207);
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900200, 0, 9563, 1.8, 1, 0),
(900201, 0, 9572, 1.0, 1, 0),
(900202, 0, 16217, 1.5, 1, 0),
(900203, 0, 16217, 1.0, 1, 0),
(900204, 0, 19407, 1.5, 1, 0),
(900205, 0, 19407, 1.0, 1, 0),
(900206, 0, 193, 1.5, 1, 0),
(900207, 0, 193, 1.0, 1, 0);

DELETE FROM `creature_equip_template` WHERE `CreatureID` IN (900200, 900201, 900202, 900203, 900204, 900205, 900206, 900207);

-- CampId is positional (config order) and scoped to Map; nothing
-- outside this file references it.
DROP TABLE IF EXISTS `mod_moba_neutral_camps`;
CREATE TABLE `mod_moba_neutral_camps` (
    `Map`            INT UNSIGNED NOT NULL,
    `CampId`         INT UNSIGNED NOT NULL,
    `InitialSpawnMs` INT UNSIGNED NOT NULL DEFAULT 90000,
    `RespawnMs`      INT UNSIGNED NOT NULL DEFAULT 120000,
    PRIMARY KEY (`Map`, `CampId`)
);

INSERT INTO `mod_moba_neutral_camps` (`Map`, `CampId`, `InitialSpawnMs`, `RespawnMs`)
VALUES
-- fel_reaver
(566, 1, 30000, 30000),
-- mage_tower
(566, 2, 30000, 30000),
-- draenei_ruins
(566, 3, 30000, 30000),
-- blood_elf
(566, 4, 30000, 30000);

DROP TABLE IF EXISTS `mod_moba_neutral_members`;
CREATE TABLE `mod_moba_neutral_members` (
    `Map`           INT UNSIGNED NOT NULL,
    `CampId`        INT UNSIGNED NOT NULL,
    `Idx`           TINYINT UNSIGNED NOT NULL,
    `CreatureEntry` INT UNSIGNED NOT NULL,
    `X` FLOAT NOT NULL,
    `Y` FLOAT NOT NULL,
    `Z` FLOAT NOT NULL,
    `O` FLOAT NOT NULL,
    PRIMARY KEY (`Map`, `CampId`, `Idx`)
);

INSERT INTO `mod_moba_neutral_members` (`Map`, `CampId`, `Idx`, `CreatureEntry`, `X`, `Y`, `Z`, `O`)
VALUES
-- fel_reaver/worg_large
(566, 1, 0, 900200, 2019.2444, 1754.3961, 1197.6388, 5.222894),
-- fel_reaver/worg_small
(566, 1, 1, 900201, 2030.1154, 1745.2213, 1193.3864, 5.5291996),
-- fel_reaver/worg_small
(566, 1, 2, 900201, 2018.3785, 1736.9833, 1196.4192, 5.7805276),
-- mage_tower/wyrm_large
(566, 2, 0, 900202, 2280.5337, 1769.0176, 1189.7069, 4.801135),
-- mage_tower/wyrm_small
(566, 2, 1, 900203, 2286.0017, 1758.9531, 1189.7069, 4.827841),
-- mage_tower/wyrm_small
(566, 2, 2, 900203, 2277.8892, 1758.643, 1189.7069, 4.7257395),
-- draenei_ruins/ray_large
(566, 3, 0, 900204, 2256.3613, 1354.0717, 1195.7981, 0.7586918),
-- draenei_ruins/ray_small
(566, 3, 1, 900205, 2271.488, 1361.0706, 1195.8077, 0.96211),
-- draenei_ruins/ray_small
(566, 3, 2, 900205, 2265.256, 1366.9736, 1195.808, 0.8081719),
-- blood_elf/boar_large
(566, 4, 0, 900206, 2051.1995, 1366.716, 1194.5392, 1.6556144),
-- blood_elf/boar_small
(566, 4, 1, 900207, 2046.6493, 1371.3636, 1194.5441, 1.7294422),
-- blood_elf/boar_small
(566, 4, 2, 900207, 2053.9358, 1372.2905, 1194.548, 1.7467208);

-- Per-entry behavior, looked up by npc_moba_neutral. Ranges are
-- authored per CAMP in the config and denormalized here per entry.
-- AggroRange 0 = pull-on-hit (AI goes REACT_DEFENSIVE); >0 is also
-- stamped into creature_template.detection_range, giving exact-radius
-- proximity aggro at equal levels. LeashRange is the hard evade cap
-- measured from the camp anchor (0 = engine leash only).
DROP TABLE IF EXISTS `mod_moba_neutral_data`;
CREATE TABLE `mod_moba_neutral_data` (
    `CreatureEntry`      INT UNSIGNED NOT NULL PRIMARY KEY,
    `Map`                INT UNSIGNED NOT NULL,
    `AggroRange`         FLOAT NOT NULL DEFAULT 0,
    `LeashRange`         FLOAT NOT NULL DEFAULT 20
);

INSERT INTO `mod_moba_neutral_data`
(`CreatureEntry`, `Map`, `AggroRange`, `LeashRange`)
VALUES
-- worg_large
(900200, 566, 8, 20),
-- worg_small
(900201, 566, 8, 20),
-- wyrm_large
(900202, 566, 8, 20),
-- wyrm_small
(900203, 566, 8, 20),
-- ray_large
(900204, 566, 8, 20),
-- ray_small
(900205, 566, 8, 20),
-- boar_large
(900206, 566, 8, 20),
-- boar_small
(900207, 566, 8, 20);

DELETE FROM `creature_loot_template` WHERE `Entry` IN (900200, 900201, 900202, 900203, 900204, 900205, 900206, 900207);

-- Buff/gold drops, rolled and delivered by BattlegroundMOBA::
-- GrantDeathDrops at the killing blow: Type 0 = buff (aura on the
-- killer; DurationMs 0 = the spell's default), 1 = gold (Copper
-- injected into the corpse loot). "item" drops are NOT here -- they
-- are the native creature_loot_template rows above. Chance is a
-- percent (config coefficient x 100).
DROP TABLE IF EXISTS `mod_moba_neutral_drops`;
CREATE TABLE `mod_moba_neutral_drops` (
    `CreatureEntry` INT UNSIGNED NOT NULL,
    `Idx`           TINYINT UNSIGNED NOT NULL,
    `Type`          TINYINT UNSIGNED NOT NULL,
    `Spell`         INT UNSIGNED NOT NULL DEFAULT 0,
    `DurationMs`    INT UNSIGNED NOT NULL DEFAULT 0,
    `Copper`        INT UNSIGNED NOT NULL DEFAULT 0,
    `Chance`        FLOAT NOT NULL DEFAULT 100,
    PRIMARY KEY (`CreatureEntry`, `Idx`)
);

INSERT INTO `mod_moba_neutral_drops`
(`CreatureEntry`, `Idx`, `Type`, `Spell`, `DurationMs`, `Copper`, `Chance`)
VALUES
-- worg_large
(900200, 0, 0, 23505, 0, 0, 100),
-- worg_large
(900200, 1, 1, 0, 0, 5000, 100),
-- worg_small
(900201, 0, 1, 0, 0, 5000, 100),
-- wyrm_large
(900202, 0, 0, 23493, 0, 0, 100),
-- wyrm_large
(900202, 1, 1, 0, 0, 5000, 100),
-- wyrm_small
(900203, 0, 1, 0, 0, 5000, 100),
-- ray_large
(900204, 0, 0, 23451, 0, 0, 100),
-- ray_large
(900204, 1, 1, 0, 0, 5000, 100),
-- ray_small
(900205, 0, 1, 0, 0, 5000, 100),
-- boar_large
(900206, 0, 1, 0, 0, 5000, 100),
-- boar_small
(900207, 0, 1, 0, 0, 5000, 100);
