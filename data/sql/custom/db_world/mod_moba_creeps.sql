-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_creep_roster.py from apps/moba/maps/*/creep_config.yaml.
-- Stats are full copies of real source creatures (see the config's
-- "source" fields) with a fixed override list enforced in code --
-- see the generator's docstring for the list and rationale.
-- Waypoint paths live in mod_moba_creep_paths.sql (gen_creep_paths.py).
-- Every DELETE clears this generator's whole ID block, not just the rows
-- about to be inserted, so a creep removed from config loses its DB rows
-- too. Blocks are declared in apps/moba/id_blocks.json.
-- ============================================================

USE acore_world;

DELETE FROM `creature_template` WHERE `entry` BETWEEN 900010 AND 900099;
INSERT INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
-- alliance_melee_right (from creature_template_2279.txt)
(900010,0,0,0,0,0,'Alliance Footman','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900010,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_melee_left (from creature_template_2279.txt)
(900016,0,0,0,0,0,'Alliance Footman','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900016,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_melee_right (from creature_template_2279.txt)
(900011,0,0,0,0,0,'Horde Grunt','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900011,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_melee_left (from creature_template_2279.txt)
(900017,0,0,0,0,0,'Horde Grunt','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900017,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_caster (from creature_template_1914.txt)
(900012,0,0,0,0,0,'Alliance Mage','MOBA Minion',NULL,0,80,80,0,84,0,1,0.98571,1,1,18,0,0,1,2000,2000,1,1,8,8,2048,0,0,7,0,0,0,0,0,0,0,0,'',1,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_caster (from creature_template_11683.txt)
(900013,0,0,0,0,0,'Horde Shaman','MOBA Minion',NULL,0,80,80,0,83,0,1,0.98571,1,1,20,0,0,1,2000,2000,1,1,8,32776,2048,0,0,7,0,0,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_siege (from creature_template_34775.txt)
(900014,0,0,0,0,0,'Alliance Demolisher','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.3,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_siege (from creature_template_34775.txt)
(900015,0,0,0,0,0,'Horde Demolisher','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.3,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_super (from creature_template_34775.txt)
(900018,0,0,0,0,0,'Alliance Super Minion','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.6,1,0.5,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_super (from creature_template_34775.txt)
(900019,0,0,0,0,0,'Horde Super Minion','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.6,1,0.5,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_top_melee_right (from creature_template_2279.txt)
(900020,0,0,0,0,0,'Alliance Footman','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900020,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_top_melee_left (from creature_template_2279.txt)
(900021,0,0,0,0,0,'Alliance Footman','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900021,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_top_melee_right (from creature_template_2279.txt)
(900022,0,0,0,0,0,'Horde Grunt','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900022,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_top_melee_left (from creature_template_2279.txt)
(900023,0,0,0,0,0,'Horde Grunt','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900023,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_top_caster (from creature_template_1914.txt)
(900024,0,0,0,0,0,'Alliance Mage','MOBA Minion',NULL,0,80,80,0,84,0,1,0.98571,1,1,18,0,0,1,2000,2000,1,1,8,8,2048,0,0,7,0,0,0,0,0,0,0,0,'',1,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_top_caster (from creature_template_11683.txt)
(900025,0,0,0,0,0,'Horde Shaman','MOBA Minion',NULL,0,80,80,0,83,0,1,0.98571,1,1,20,0,0,1,2000,2000,1,1,8,32776,2048,0,0,7,0,0,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_top_siege (from creature_template_34775.txt)
(900026,0,0,0,0,0,'Alliance Demolisher','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.3,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_top_siege (from creature_template_34775.txt)
(900027,0,0,0,0,0,'Horde Demolisher','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.3,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_top_super (from creature_template_34775.txt)
(900028,0,0,0,0,0,'Alliance Super Minion','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.6,1,0.5,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_top_super (from creature_template_34775.txt)
(900029,0,0,0,0,0,'Horde Super Minion','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.6,1,0.5,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_bot_melee_right (from creature_template_2279.txt)
(900030,0,0,0,0,0,'Alliance Footman','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900030,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_bot_melee_left (from creature_template_2279.txt)
(900031,0,0,0,0,0,'Alliance Footman','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900031,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_bot_melee_right (from creature_template_2279.txt)
(900032,0,0,0,0,0,'Horde Grunt','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900032,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_bot_melee_left (from creature_template_2279.txt)
(900033,0,0,0,0,0,'Horde Grunt','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,0,0,1,2000,2000,1,1,1,8,2048,0,0,7,0,900033,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_bot_caster (from creature_template_1914.txt)
(900034,0,0,0,0,0,'Alliance Mage','MOBA Minion',NULL,0,80,80,0,84,0,1,0.98571,1,1,18,0,0,1,2000,2000,1,1,8,8,2048,0,0,7,0,0,0,0,0,0,0,0,'',1,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_bot_caster (from creature_template_11683.txt)
(900035,0,0,0,0,0,'Horde Shaman','MOBA Minion',NULL,0,80,80,0,83,0,1,0.98571,1,1,20,0,0,1,2000,2000,1,1,8,32776,2048,0,0,7,0,0,0,0,0,0,0,0,'',0,1,0.15,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_bot_siege (from creature_template_34775.txt)
(900036,0,0,0,0,0,'Alliance Demolisher','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.3,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_bot_siege (from creature_template_34775.txt)
(900037,0,0,0,0,0,'Horde Demolisher','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.3,1,0.25,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- alliance_bot_super (from creature_template_34775.txt)
(900038,0,0,0,0,0,'Alliance Super Minion','MOBA Minion',NULL,0,80,80,0,84,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.6,1,0.5,1,0,0,0,0,2097152,'npc_moba_creep',0),
-- horde_bot_super (from creature_template_34775.txt)
(900039,0,0,0,0,0,'Horde Super Minion','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.6,1,0.5,1,0,0,0,0,2097152,'npc_moba_creep',0);

-- Creeps spawn from C++, never from `creature` rows, so this normally deletes
-- nothing. It sweeps GM `.npc add` test spawns, which would otherwise sit in
-- the world forever. The spawn table's entry column is `id`, not `id1`:
-- upstream 2026_06_16_00.sql renamed it and moved id2/id3 to creature_multispawn.
DELETE FROM `creature` WHERE `id` BETWEEN 900010 AND 900099;

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 900010 AND 900099;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900010, 0, 164, 1.0, 1, 0),
(900016, 0, 164, 1.0, 1, 0),
(900011, 0, 496, 1.0, 1, 0),
(900017, 0, 496, 1.0, 1, 0),
(900012, 0, 3559, 1.0, 1, 0),
(900013, 0, 11865, 1.0, 1, 0),
(900014, 0, 27658, 1.0, 1, 0),
(900015, 0, 27658, 1.0, 1, 0),
(900018, 0, 8395, 1.5, 1, 0),
(900019, 0, 12818, 1.5, 1, 0),
(900020, 0, 164, 1.0, 1, 0),
(900021, 0, 164, 1.0, 1, 0),
(900022, 0, 496, 1.0, 1, 0),
(900023, 0, 496, 1.0, 1, 0),
(900024, 0, 3559, 1.0, 1, 0),
(900025, 0, 11865, 1.0, 1, 0),
(900026, 0, 27658, 1.0, 1, 0),
(900027, 0, 27658, 1.0, 1, 0),
(900028, 0, 8395, 1.5, 1, 0),
(900029, 0, 12818, 1.5, 1, 0),
(900030, 0, 164, 1.0, 1, 0),
(900031, 0, 164, 1.0, 1, 0),
(900032, 0, 496, 1.0, 1, 0),
(900033, 0, 496, 1.0, 1, 0),
(900034, 0, 3559, 1.0, 1, 0),
(900035, 0, 11865, 1.0, 1, 0),
(900036, 0, 27658, 1.0, 1, 0),
(900037, 0, 27658, 1.0, 1, 0),
(900038, 0, 8395, 1.5, 1, 0),
(900039, 0, 12818, 1.5, 1, 0);

DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN 900010 AND 900099;
INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
VALUES
(900010, 1, 1899, 143, 0, 0),
(900016, 1, 1899, 143, 0, 0),
(900011, 1, 2183, 2051, 0, 0),
(900017, 1, 2183, 2051, 0, 0),
(900012, 1, 2177, 0, 0, 0),
(900013, 1, 5303, 0, 0, 0),
(900020, 1, 1899, 143, 0, 0),
(900021, 1, 1899, 143, 0, 0),
(900022, 1, 2183, 2051, 0, 0),
(900023, 1, 2183, 2051, 0, 0),
(900024, 1, 2177, 0, 0, 0),
(900025, 1, 5303, 0, 0, 0),
(900030, 1, 1899, 143, 0, 0),
(900031, 1, 1899, 143, 0, 0),
(900032, 1, 2183, 2051, 0, 0),
(900033, 1, 2183, 2051, 0, 0),
(900034, 1, 2177, 0, 0, 0),
(900035, 1, 5303, 0, 0, 0);

-- Role: 0=melee, 1=caster, 2=siege, 3=super. AttackRange/AttackIntervalMs/
-- AttackSpellId apply to casters only (melee/siege use default engine
-- auto-attack). WaypointPathId comes from the lane generator lockfile.
DROP TABLE IF EXISTS `mod_moba_creep_data`;
CREATE TABLE `mod_moba_creep_data` (
    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,
    `Map`              INT UNSIGNED NOT NULL,
    `Team`             TINYINT UNSIGNED NOT NULL,
    `Role`             TINYINT UNSIGNED NOT NULL,
    `AttackRange`      FLOAT NOT NULL DEFAULT 20,
    `AttackIntervalMs` INT UNSIGNED NOT NULL DEFAULT 2000,
    `AttackSpellId`    INT UNSIGNED NOT NULL DEFAULT 0,
    `WaypointPathId`   INT UNSIGNED NOT NULL,
    `ReferencePathId`  INT UNSIGNED NOT NULL,  -- lane centreline; creep speed
                                               -- compensation divides by its legs
    `DespawnMs`        INT UNSIGNED NOT NULL DEFAULT 60000,
    `Lane`             TINYINT UNSIGNED NOT NULL DEFAULT 0  -- 0 none, 1 top, 2 mid, 3 bot;
                                                           -- a super creep spawns only while the
                                                           -- ENEMY inhibitor on THIS lane is down
);

INSERT INTO `mod_moba_creep_data`
(`CreatureEntry`, `Map`, `Team`, `Role`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`, `WaypointPathId`, `ReferencePathId`, `DespawnMs`, `Lane`)
VALUES
-- alliance_melee_right (mid/melee_right)
(900010, 566, 0, 0, 20, 2000, 0, 900110, 900130, 60000, 2),
-- alliance_melee_left (mid/melee_left)
(900016, 566, 0, 0, 20, 2000, 0, 900111, 900130, 60000, 2),
-- horde_melee_right (mid/melee_right)
(900011, 566, 1, 0, 20, 2000, 0, 900120, 900131, 60000, 2),
-- horde_melee_left (mid/melee_left)
(900017, 566, 1, 0, 20, 2000, 0, 900121, 900131, 60000, 2),
-- alliance_caster (mid/caster)
(900012, 566, 0, 1, 20, 2000, 20793, 900100, 900130, 60000, 2),
-- horde_caster (mid/caster)
(900013, 566, 1, 1, 20, 2000, 20805, 900101, 900131, 60000, 2),
-- alliance_siege (mid/siege)
(900014, 566, 0, 2, 20, 2000, 0, 900112, 900130, 60000, 2),
-- horde_siege (mid/siege)
(900015, 566, 1, 2, 20, 2000, 0, 900122, 900131, 60000, 2),
-- alliance_super (mid/super)
(900018, 566, 0, 3, 20, 2000, 0, 900102, 900130, 60000, 2),
-- horde_super (mid/super)
(900019, 566, 1, 3, 20, 2000, 0, 900103, 900131, 60000, 2),
-- alliance_top_melee_right (top/melee_right)
(900020, 900, 0, 0, 20, 2000, 0, 900106, 900132, 60000, 1),
-- alliance_top_melee_left (top/melee_left)
(900021, 900, 0, 0, 20, 2000, 0, 900104, 900132, 60000, 1),
-- horde_top_melee_right (top/melee_right)
(900022, 900, 1, 0, 20, 2000, 0, 900107, 900133, 60000, 1),
-- horde_top_melee_left (top/melee_left)
(900023, 900, 1, 0, 20, 2000, 0, 900105, 900133, 60000, 1),
-- alliance_top_caster (top/caster)
(900024, 900, 0, 1, 20, 2000, 20793, 900108, 900132, 60000, 1),
-- horde_top_caster (top/caster)
(900025, 900, 1, 1, 20, 2000, 20805, 900109, 900133, 60000, 1),
-- alliance_top_siege (top/siege)
(900026, 900, 0, 2, 20, 2000, 0, 900113, 900132, 60000, 1),
-- horde_top_siege (top/siege)
(900027, 900, 1, 2, 20, 2000, 0, 900114, 900133, 60000, 1),
-- alliance_top_super (top/super)
(900028, 900, 0, 3, 20, 2000, 0, 900115, 900132, 60000, 1),
-- horde_top_super (top/super)
(900029, 900, 1, 3, 20, 2000, 0, 900116, 900133, 60000, 1),
-- alliance_bot_melee_right (bot/melee_right)
(900030, 900, 0, 0, 20, 2000, 0, 900119, 900134, 60000, 3),
-- alliance_bot_melee_left (bot/melee_left)
(900031, 900, 0, 0, 20, 2000, 0, 900117, 900134, 60000, 3),
-- horde_bot_melee_right (bot/melee_right)
(900032, 900, 1, 0, 20, 2000, 0, 900123, 900135, 60000, 3),
-- horde_bot_melee_left (bot/melee_left)
(900033, 900, 1, 0, 20, 2000, 0, 900118, 900135, 60000, 3),
-- alliance_bot_caster (bot/caster)
(900034, 900, 0, 1, 20, 2000, 20793, 900124, 900134, 60000, 3),
-- horde_bot_caster (bot/caster)
(900035, 900, 1, 1, 20, 2000, 20805, 900125, 900135, 60000, 3),
-- alliance_bot_siege (bot/siege)
(900036, 900, 0, 2, 20, 2000, 0, 900126, 900134, 60000, 3),
-- horde_bot_siege (bot/siege)
(900037, 900, 1, 2, 20, 2000, 0, 900127, 900135, 60000, 3),
-- alliance_bot_super (bot/super)
(900038, 900, 0, 3, 20, 2000, 0, 900128, 900134, 60000, 3),
-- horde_bot_super (bot/super)
(900039, 900, 1, 3, 20, 2000, 0, 900129, 900135, 60000, 3);

DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 900010 AND 900099;
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
VALUES
(900010, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_melee_right (moba drop)'),
(900016, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_melee_left (moba drop)'),
(900011, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_melee_right (moba drop)'),
(900017, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_melee_left (moba drop)'),
(900020, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_top_melee_right (moba drop)'),
(900021, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_top_melee_left (moba drop)'),
(900022, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_top_melee_right (moba drop)'),
(900023, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_top_melee_left (moba drop)'),
(900030, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_bot_melee_right (moba drop)'),
(900031, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_bot_melee_left (moba drop)'),
(900032, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_bot_melee_right (moba drop)'),
(900033, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_bot_melee_left (moba drop)');

-- Drops, rolled and delivered by BattlegroundMOBA::GrantDeathDrops at
-- the killing blow. Type 0 = buff (aura on the killer), 1 = gold
-- (Copper injected into the corpse loot), 3 = team gold, 4 = team buff.
-- The team types pay the whole killing team with no corpse; team buff
-- reaches living players only. DurationMs 0 = the spell's own duration.
-- Type 2 = item is the ODD ONE: the item itself comes from the
-- creature_loot_template rows above, so a type-2 row grants nothing and
-- carries only Item + Sell, the per-unit price npc_moba_store refunds. An
-- item drop with no `sell` gets no row here and cannot be sold back.
-- Chance is a percent (config x 100).
DROP TABLE IF EXISTS `mod_moba_creep_drops`;
CREATE TABLE `mod_moba_creep_drops` (
    `CreatureEntry` INT UNSIGNED NOT NULL,
    `Idx`           TINYINT UNSIGNED NOT NULL,
    `Type`          TINYINT UNSIGNED NOT NULL,
    `Spell`         INT UNSIGNED NOT NULL DEFAULT 0,
    `DurationMs`    INT UNSIGNED NOT NULL DEFAULT 0,
    `Copper`        INT UNSIGNED NOT NULL DEFAULT 0,
    `Chance`        FLOAT NOT NULL DEFAULT 100,
    `Item`          INT UNSIGNED NOT NULL DEFAULT 0,   -- type 2 only
    `Sell`          INT UNSIGNED NOT NULL DEFAULT 0,   -- type 2 only, PER UNIT
    PRIMARY KEY (`CreatureEntry`, `Idx`)
);

INSERT INTO `mod_moba_creep_drops`
(`CreatureEntry`, `Idx`, `Type`, `Spell`, `DurationMs`, `Copper`, `Chance`, `Item`, `Sell`)
VALUES
-- alliance_melee_right
(900010, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_melee_right
(900010, 1, 2, 0, 0, 0, 100, 33470, 100),
-- alliance_melee_left
(900016, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_melee_left
(900016, 1, 2, 0, 0, 0, 100, 33470, 100),
-- horde_melee_right
(900011, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_melee_right
(900011, 1, 2, 0, 0, 0, 100, 33470, 100),
-- horde_melee_left
(900017, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_melee_left
(900017, 1, 2, 0, 0, 0, 100, 33470, 100),
-- alliance_caster
(900012, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_caster
(900013, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_siege
(900014, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_siege
(900015, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_super
(900018, 0, 1, 0, 0, 8000, 100, 0, 0),
-- horde_super
(900019, 0, 1, 0, 0, 8000, 100, 0, 0),
-- alliance_top_melee_right
(900020, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_top_melee_right
(900020, 1, 2, 0, 0, 0, 100, 33470, 100),
-- alliance_top_melee_left
(900021, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_top_melee_left
(900021, 1, 2, 0, 0, 0, 100, 33470, 100),
-- horde_top_melee_right
(900022, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_top_melee_right
(900022, 1, 2, 0, 0, 0, 100, 33470, 100),
-- horde_top_melee_left
(900023, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_top_melee_left
(900023, 1, 2, 0, 0, 0, 100, 33470, 100),
-- alliance_top_caster
(900024, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_top_caster
(900025, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_top_siege
(900026, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_top_siege
(900027, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_top_super
(900028, 0, 1, 0, 0, 8000, 100, 0, 0),
-- horde_top_super
(900029, 0, 1, 0, 0, 8000, 100, 0, 0),
-- alliance_bot_melee_right
(900030, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_bot_melee_right
(900030, 1, 2, 0, 0, 0, 100, 33470, 100),
-- alliance_bot_melee_left
(900031, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_bot_melee_left
(900031, 1, 2, 0, 0, 0, 100, 33470, 100),
-- horde_bot_melee_right
(900032, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_bot_melee_right
(900032, 1, 2, 0, 0, 0, 100, 33470, 100),
-- horde_bot_melee_left
(900033, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_bot_melee_left
(900033, 1, 2, 0, 0, 0, 100, 33470, 100),
-- alliance_bot_caster
(900034, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_bot_caster
(900035, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_bot_siege
(900036, 0, 1, 0, 0, 5000, 100, 0, 0),
-- horde_bot_siege
(900037, 0, 1, 0, 0, 5000, 100, 0, 0),
-- alliance_bot_super
(900038, 0, 1, 0, 0, 8000, 100, 0, 0),
-- horde_bot_super
(900039, 0, 1, 0, 0, 8000, 100, 0, 0);
