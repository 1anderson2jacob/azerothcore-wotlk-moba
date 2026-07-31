-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_creep_roster.py from apps/moba/maps/*/creep_config.yaml.
-- Stats are full copies of real source creatures (see the config's
-- "source" fields) with a fixed override list enforced in code --
-- see the generator's docstring for the list and rationale.
-- Waypoint paths live in mod_moba_creep_paths.sql (gen_creep_paths.py).
-- ============================================================

USE acore_world;

DELETE FROM `creature_template` WHERE `entry` IN (900010, 900016, 900011, 900017, 900012, 900013, 900014, 900015, 900018, 900019);
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
(900019,0,0,0,0,0,'Horde Super Minion','MOBA Minion',NULL,0,80,80,0,83,0,1.2,0.98571,1,1,20,1,0,1,2000,2000,1,1,1,16392,2048,0,0,7,131080,0,0,0,0,0,0,0,'',0,1,0.6,1,0.5,1,0,0,0,0,2097152,'npc_moba_creep',0);

DELETE FROM `creature_template_model` WHERE `CreatureID` IN (900010, 900016, 900011, 900017, 900012, 900013, 900014, 900015, 900018, 900019);
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
(900019, 0, 12818, 1.5, 1, 0);

DELETE FROM `creature_equip_template` WHERE `CreatureID` IN (900010, 900016, 900011, 900017, 900012, 900013, 900014, 900015, 900018, 900019);
INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`, `VerifiedBuild`)
VALUES
(900010, 1, 1899, 143, 0, 0),
(900016, 1, 1899, 143, 0, 0),
(900011, 1, 2183, 2051, 0, 0),
(900017, 1, 2183, 2051, 0, 0),
(900012, 1, 2177, 0, 0, 0),
(900013, 1, 5303, 0, 0, 0);

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
    `DespawnMs`        INT UNSIGNED NOT NULL DEFAULT 60000
);

INSERT INTO `mod_moba_creep_data`
(`CreatureEntry`, `Map`, `Team`, `Role`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`, `WaypointPathId`, `DespawnMs`)
VALUES
-- alliance_melee_right (mid/melee_right)
(900010, 566, 0, 0, 20, 2000, 0, 900110, 60000),
-- alliance_melee_left (mid/melee_left)
(900016, 566, 0, 0, 20, 2000, 0, 900111, 60000),
-- horde_melee_right (mid/melee_right)
(900011, 566, 1, 0, 20, 2000, 0, 900120, 60000),
-- horde_melee_left (mid/melee_left)
(900017, 566, 1, 0, 20, 2000, 0, 900121, 60000),
-- alliance_caster (mid/caster)
(900012, 566, 0, 1, 20, 2000, 20793, 900100, 60000),
-- horde_caster (mid/caster)
(900013, 566, 1, 1, 20, 2000, 20805, 900101, 60000),
-- alliance_siege (mid/siege)
(900014, 566, 0, 2, 20, 2000, 0, 900112, 60000),
-- horde_siege (mid/siege)
(900015, 566, 1, 2, 20, 2000, 0, 900122, 60000),
-- alliance_super (mid/super)
(900018, 566, 0, 3, 20, 2000, 0, 900102, 60000),
-- horde_super (mid/super)
(900019, 566, 1, 3, 20, 2000, 0, 900103, 60000);

DELETE FROM `creature_loot_template` WHERE `Entry` IN (900010, 900016, 900011, 900017, 900012, 900013, 900014, 900015, 900018, 900019);
INSERT INTO `creature_loot_template`
(`Entry`, `Item`, `Reference`, `Chance`, `QuestRequired`, `LootMode`, `GroupId`, `MinCount`, `MaxCount`, `Comment`)
VALUES
(900010, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_melee_right (moba drop)'),
(900016, 33470, 0, 100, 0, 1, 0, 1, 1, 'alliance_melee_left (moba drop)'),
(900011, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_melee_right (moba drop)'),
(900017, 33470, 0, 100, 0, 1, 0, 1, 1, 'horde_melee_left (moba drop)');

-- Buff/gold drops, rolled and delivered by BattlegroundMOBA::
-- GrantDeathDrops at the killing blow: Type 0 = buff (aura on the
-- killer; DurationMs 0 = the spell's default), 1 = gold (Copper
-- injected into the corpse loot). Type 2 = item is the ODD ONE: the
-- item itself comes from the creature_loot_template rows above, so a
-- type-2 row grants nothing and carries only Item + Sell, the per-unit
-- price npc_moba_store refunds. An item drop with no `sell` gets no row
-- here and cannot be sold back. Chance is a percent (config x 100).
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
(900019, 0, 1, 0, 0, 8000, 100, 0, 0);
