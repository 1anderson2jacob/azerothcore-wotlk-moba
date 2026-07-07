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