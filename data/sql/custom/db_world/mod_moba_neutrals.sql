-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_neutral_camps.py from apps/moba/maps/*/neutral_config.yaml.
-- Stats are full copies of real source creatures with a fixed override
-- list enforced in code -- see the generator's docstring.
-- Every DELETE clears this generator's whole ID block, not just the rows
-- about to be inserted, so a mob removed from config loses its DB rows too.
-- Blocks are declared in apps/moba/id_blocks.json.
-- ============================================================

USE acore_world;

DELETE FROM `creature_template` WHERE `entry` BETWEEN 900200 AND 900249;
INSERT INTO `creature_template`
(`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`, `KillCredit1`, `KillCredit2`, `name`, `subname`, `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `speed_swim`, `speed_flight`, `detection_range`, `rank`, `dmgschool`, `DamageModifier`, `BaseAttackTime`, `RangeAttackTime`, `BaseVariance`, `RangeVariance`, `unit_class`, `unit_flags`, `unit_flags2`, `dynamicflags`, `family`, `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`, `PetSpellDataId`, `VehicleId`, `mingold`, `maxgold`, `AIName`, `MovementType`, `HoverHeight`, `HealthModifier`, `ManaModifier`, `ArmorModifier`, `ExperienceModifier`, `RacialLeader`, `movementId`, `RegenHealth`, `CreatureImmunitiesId`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
-- wolf_large (from Alliance Battleguard #2279)
(900209,0,0,0,0,0,'Ravenous Worg','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.6,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- wolf_small (from Alliance Battleguard #2279)
(900210,0,0,0,0,0,'Worg Pup','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.2,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- wraith_large (from Alliance Battleguard #2279)
(900211,0,0,0,0,0,'Greater Wraith','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.6,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- wraith_small (from Alliance Battleguard #2279)
(900212,0,0,0,0,0,'Lesser Wraith','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.2,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- golem_large (from Alliance Battleguard #2279)
(900213,0,0,0,0,0,'Ancient Golem','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.6,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- golem_small (from Alliance Battleguard #2279)
(900214,0,0,0,0,0,'Golem Shard','MOBA Jungle',NULL,0,80,80,0,14,0,1.2,1.14286,1,1,8,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,0.2,1,0.25,1,0,0,1,0,2097152,'npc_moba_neutral',0),
-- rosham_bo (from Alliance Battleguard #2279)
(900215,0,0,0,0,0,'Rosham Bo','The Terror from Below',NULL,0,85,85,0,14,0,1.2,1.14286,1,1,12,0,0,1,2000,2000,1,1,1,0,2048,0,0,1,0,0,0,0,0,0,0,0,'',0,1,4,1,2,1,0,0,1,0,0,'npc_moba_neutral',0);

-- Neutrals spawn from C++, never from `creature` rows, so this normally deletes
-- nothing. It sweeps GM `.npc add` test spawns. The spawn table's entry column
-- is `id`, not `id1`: upstream 2026_06_16_00.sql renamed it.
DELETE FROM `creature` WHERE `id` BETWEEN 900200 AND 900249;

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 900200 AND 900249;
INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900209, 0, 9563, 1.8, 1, 0),
(900210, 0, 9572, 1.0, 1, 0),
(900211, 0, 19407, 1.5, 1, 0),
(900212, 0, 19407, 1.0, 1, 0),
(900213, 0, 16217, 1.5, 1, 0),
(900214, 0, 16217, 1.0, 1, 0),
(900215, 0, 20746, 1.0, 1, 0);

DELETE FROM `creature_equip_template` WHERE `CreatureID` BETWEEN 900200 AND 900249;

-- CampId is positional (config order) and scoped to Map; nothing
-- outside this file references it. Tier 0 is an ordinary camp and the
-- kill feed announces nothing for it; nonzero marks a boss. SpawnWarnMs
-- is the lead time on the "spawning soon" line (0 = none) and must stay
-- under RespawnMs -- the warning fires at RespawnMs - SpawnWarnMs.
DROP TABLE IF EXISTS `mod_moba_neutral_camps`;
CREATE TABLE `mod_moba_neutral_camps` (
    `Map`            INT UNSIGNED NOT NULL,
    `CampId`         INT UNSIGNED NOT NULL,
    `Tier`           TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `InitialSpawnMs` INT UNSIGNED NOT NULL DEFAULT 90000,
    `RespawnMs`      INT UNSIGNED NOT NULL DEFAULT 120000,
    `SpawnWarnMs`  INT UNSIGNED NOT NULL DEFAULT 0,
    PRIMARY KEY (`Map`, `CampId`)
);

INSERT INTO `mod_moba_neutral_camps`
(`Map`, `CampId`, `Tier`, `InitialSpawnMs`, `RespawnMs`, `SpawnWarnMs`)
VALUES
-- wolves_west
(900, 1, 0, 30000, 30000, 0),
-- wolves_east
(900, 2, 0, 30000, 30000, 0),
-- wraiths_west
(900, 3, 0, 30000, 30000, 0),
-- wraiths_east
(900, 4, 0, 30000, 30000, 0),
-- golems_west
(900, 5, 0, 30000, 30000, 0),
-- golems_east
(900, 6, 0, 30000, 30000, 0),
-- boss_den
(900, 7, 1, 60000, 60000, 15000);

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
-- wolves_west/wolf_large
(900, 1, 0, 900209, -89.83, -23.12, 0.0, 0.2519),
-- wolves_west/wolf_small
(900, 1, 1, 900210, -92.23, -26.63, 0.0, 0.2519),
-- wolves_west/wolf_small
(900, 1, 2, 900210, -91.63, -21.21, 0.0, 0.2519),
-- wolves_east/wolf_large
(900, 2, 0, 900209, 89.83, -23.12, 0.0, 2.8897),
-- wolves_east/wolf_small
(900, 2, 1, 900210, 92.23, -26.63, 0.0, 2.8897),
-- wolves_east/wolf_small
(900, 2, 2, 900210, 91.63, -21.21, 0.0, 2.8897),
-- wraiths_west/wraith_large
(900, 3, 0, 900211, -35.36, 63.13, 0.0, 5.223),
-- wraiths_west/wraith_small
(900, 3, 1, 900212, -39.37, 64.55, 0.0, 5.223),
-- wraiths_west/wraith_small
(900, 3, 2, 900212, -34.88, 66.6, 0.0, 5.223),
-- wraiths_east/wraith_large
(900, 4, 0, 900211, 35.36, 63.13, 0.0, 4.2018),
-- wraiths_east/wraith_small
(900, 4, 1, 900212, 39.37, 64.55, 0.0, 4.2018),
-- wraiths_east/wraith_small
(900, 4, 2, 900212, 34.88, 66.6, 0.0, 4.2018),
-- golems_west/golem_large
(900, 5, 0, 900213, -47.93, -34.57, 0.0, 0.6249),
-- golems_west/golem_small
(900, 5, 1, 900214, -50.73, -31.37, 0.0, 0.6249),
-- golems_west/golem_small
(900, 5, 2, 900214, -45.37, -31.05, 0.0, 0.6249),
-- golems_east/golem_large
(900, 6, 0, 900213, 47.93, -34.57, 0.0, 2.5167),
-- golems_east/golem_small
(900, 6, 1, 900214, 50.73, -31.37, 0.0, 2.5167),
-- golems_east/golem_small
(900, 6, 2, 900214, 45.37, -31.05, 0.0, 2.5167),
-- boss_den/rosham_bo
(900, 7, 0, 900215, 0.0, 83.5, 0.0, 4.7124);

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
-- wolf_large
(900209, 900, 8, 20),
-- wolf_small
(900210, 900, 8, 20),
-- wraith_large
(900211, 900, 8, 20),
-- wraith_small
(900212, 900, 8, 20),
-- golem_large
(900213, 900, 8, 20),
-- golem_small
(900214, 900, 8, 20),
-- rosham_bo
(900215, 900, 12, 20);

DELETE FROM `creature_loot_template` WHERE `Entry` BETWEEN 900200 AND 900249;

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
DROP TABLE IF EXISTS `mod_moba_neutral_drops`;
CREATE TABLE `mod_moba_neutral_drops` (
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

INSERT INTO `mod_moba_neutral_drops`
(`CreatureEntry`, `Idx`, `Type`, `Spell`, `DurationMs`, `Copper`, `Chance`, `Item`, `Sell`)
VALUES
-- wolf_large
(900209, 0, 0, 23505, 0, 0, 100, 0, 0),
-- wolf_large
(900209, 1, 1, 0, 0, 5000, 100, 0, 0),
-- wolf_small
(900210, 0, 1, 0, 0, 5000, 100, 0, 0),
-- wraith_large
(900211, 0, 0, 23493, 0, 0, 100, 0, 0),
-- wraith_large
(900211, 1, 1, 0, 0, 5000, 100, 0, 0),
-- wraith_small
(900212, 0, 1, 0, 0, 5000, 100, 0, 0),
-- golem_large
(900213, 0, 0, 23451, 0, 0, 100, 0, 0),
-- golem_large
(900213, 1, 1, 0, 0, 5000, 100, 0, 0),
-- golem_small
(900214, 0, 1, 0, 0, 5000, 100, 0, 0),
-- rosham_bo
(900215, 0, 3, 0, 0, 40000, 100, 0, 0),
-- rosham_bo
(900215, 1, 4, 48469, 180000, 0, 100, 0, 0);
