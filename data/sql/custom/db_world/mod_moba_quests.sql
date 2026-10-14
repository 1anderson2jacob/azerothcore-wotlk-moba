-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_quests.py from apps/moba/maps/*/quest_config.yaml.
-- Owns the quest givers and their spawns, the invisible credit entries, every
-- quest row and its text, the one-quest-per-animal conditions, and the baked
-- reward items. Every DELETE clears this generator's whole block
-- (apps/moba/id_blocks.json), so anything dropped from config goes too.
-- ============================================================

USE acore_world;

DELETE FROM `creature_template` WHERE `entry` BETWEEN 900500 AND 900599;
INSERT INTO `creature_template`
(`entry`, `name`, `subname`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `speed_walk`, `speed_run`, `rank`, `unit_class`, `unit_flags`, `type`, `type_flags`, `MovementType`, `HealthModifier`, `ArmorModifier`, `RegenHealth`, `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
-- plate/0
(900500, 'Quartermaster', 'Plate', 80, 80, 84, 2, 1.0, 1.14286, 0, 1, 770, 7, 0, 0, 1, 1, 1, 0, '', 0),
-- plate/1
(900501, 'Quartermaster', 'Plate', 80, 80, 83, 2, 1.0, 1.14286, 0, 1, 770, 7, 0, 0, 1, 1, 1, 0, '', 0),
-- jewelry/0
(900502, 'Jeweler', 'Jewelry', 80, 80, 84, 2, 1.0, 1.14286, 0, 1, 770, 7, 0, 0, 1, 1, 1, 0, '', 0),
-- jewelry/1
(900503, 'Jeweler', 'Jewelry', 80, 80, 83, 2, 1.0, 1.14286, 0, 1, 770, 7, 0, 0, 1, 1, 1, 0, '', 0),
-- tiger credit
(900504, 'tiger quest credit', '', 1, 1, 35, 0, 1.0, 1.14286, 0, 1, 0, 10, 0, 0, 1, 1, 1, 128, '', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 900500 AND 900599;
INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900500, 0, 3275, 1, 1, 0),
(900501, 0, 1879, 1, 1, 0),
(900502, 0, 12669, 1, 1, 0),
(900503, 0, 12681, 1, 1, 0),
(900504, 0, 11686, 1, 1, 0);

DELETE FROM `creature` WHERE `guid` BETWEEN 900500 AND 900599 OR `id` BETWEEN 900500 AND 900599;
INSERT INTO `creature`
(`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `wander_distance`, `MovementType`)
VALUES
(900500, 900500, 900, 1, 1, 0, -183.32, 7.83, 3.0, 3.6486, 300, 0, 0),
(900501, 900501, 900, 1, 1, 0, 183.16, 6.92, 3.0, 5.8152, 300, 0, 0),
(900502, 900502, 900, 1, 1, 0, -182.41, -7.29, 3.0, 2.6536, 300, 0, 0),
(900503, 900503, 900, 1, 1, 0, 182.84, -7.8, 3.0, 0.465, 300, 0, 0);

DELETE FROM `quest_template` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_template`
(`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`, `RewardXPDifficulty`, `AllowableRaces`, `RewardChoiceItemID1`, `RewardChoiceItemQuantity1`, `RewardChoiceItemID2`, `RewardChoiceItemQuantity2`, `RewardChoiceItemID3`, `RewardChoiceItemQuantity3`, `RewardChoiceItemID4`, `RewardChoiceItemQuantity4`, `RewardChoiceItemID5`, `RewardChoiceItemQuantity5`, `RewardChoiceItemID6`, `RewardChoiceItemQuantity6`, `RequiredNpcOrGo1`, `RequiredNpcOrGoCount1`, `LogTitle`, `LogDescription`, `QuestDescription`, `ObjectiveText1`, `VerifiedBuild`)
VALUES
-- plate/tiger/1
(900000, 2, 40, 0, 5000, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 10, 'Way of the Tiger (1)', 'Land the killing blow on 10 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 10 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- plate/tiger/2
(900001, 2, 40, 0, 5000, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 15, 'Way of the Tiger (2)', 'Land the killing blow on 15 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 15 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- plate/tiger/3
(900002, 2, 40, 0, 5000, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 20, 'Way of the Tiger (3)', 'Land the killing blow on 20 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 20 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- plate/tiger/4
(900003, 2, 40, 0, 5000, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 25, 'Way of the Tiger (4)', 'Land the killing blow on 25 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 25 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/tiger/1
(900004, 2, 40, 0, 5000, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 10, 'Way of the Tiger (1)', 'Land the killing blow on 10 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 10 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/tiger/2
(900005, 2, 40, 0, 5000, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 15, 'Way of the Tiger (2)', 'Land the killing blow on 15 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 15 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/tiger/3
(900006, 2, 40, 0, 5000, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 20, 'Way of the Tiger (3)', 'Land the killing blow on 20 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 20 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/tiger/4
(900007, 2, 40, 0, 5000, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 25, 'Way of the Tiger (4)', 'Land the killing blow on 25 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 25 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0);

DELETE FROM `quest_template_addon` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_template_addon`
(`ID`, `PrevQuestID`, `NextQuestID`)
VALUES
(900000, 0, 900001),
(900001, 900000, 900002),
(900002, 900001, 900003),
(900003, 900002, 0),
(900004, 0, 900005),
(900005, 900004, 900006),
(900006, 900005, 900007),
(900007, 900006, 0);

DELETE FROM `quest_details` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_details`
(`ID`, `Emote1`)
VALUES
(900000, 1),
(900001, 1),
(900002, 1),
(900003, 1),
(900004, 1),
(900005, 1),
(900006, 1),
(900007, 1);

DELETE FROM `quest_request_items` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_request_items`
(`ID`, `EmoteOnIncomplete`, `CompletionText`)
VALUES
(900000, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900001, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900002, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900003, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900004, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900005, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900006, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900007, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.');

DELETE FROM `quest_offer_reward` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_offer_reward`
(`ID`, `Emote1`, `RewardText`)
VALUES
(900000, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900001, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900002, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900003, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900004, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900005, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900006, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900007, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.');

DELETE FROM `creature_queststarter` WHERE `quest` BETWEEN 900000 AND 900099 OR `id` BETWEEN 900500 AND 900599;
INSERT INTO `creature_queststarter`
(`id`, `quest`)
VALUES
(900500, 900000),
(900500, 900001),
(900500, 900002),
(900500, 900003),
(900501, 900000),
(900501, 900001),
(900501, 900002),
(900501, 900003),
(900502, 900004),
(900502, 900005),
(900502, 900006),
(900502, 900007),
(900503, 900004),
(900503, 900005),
(900503, 900006),
(900503, 900007);

DELETE FROM `creature_questender` WHERE `quest` BETWEEN 900000 AND 900099 OR `id` BETWEEN 900500 AND 900599;
INSERT INTO `creature_questender`
(`id`, `quest`)
VALUES
(900500, 900000),
(900500, 900001),
(900500, 900002),
(900500, 900003),
(900501, 900000),
(900501, 900001),
(900501, 900002),
(900501, 900003),
(900502, 900004),
(900502, 900005),
(900502, 900006),
(900502, 900007),
(900503, 900004),
(900503, 900005),
(900503, 900006),
(900503, 900007);

DELETE FROM `conditions` WHERE `SourceTypeOrReferenceId` = 19 AND `SourceEntry` BETWEEN 900000 AND 900099;
INSERT INTO `conditions`
(`SourceTypeOrReferenceId`, `SourceGroup`, `SourceEntry`, `SourceId`, `ElseGroup`, `ConditionTypeOrReference`, `ConditionTarget`, `ConditionValue1`, `ConditionValue2`, `ConditionValue3`, `NegativeCondition`, `Comment`)
VALUES
(19, 0, 900000, 0, 0, 47, 0, 900004, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900000, 0, 0, 47, 0, 900005, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900000, 0, 0, 47, 0, 900006, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900000, 0, 0, 47, 0, 900007, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900001, 0, 0, 47, 0, 900004, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900001, 0, 0, 47, 0, 900005, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900001, 0, 0, 47, 0, 900006, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900001, 0, 0, 47, 0, 900007, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900002, 0, 0, 47, 0, 900004, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900002, 0, 0, 47, 0, 900005, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900002, 0, 0, 47, 0, 900006, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900002, 0, 0, 47, 0, 900007, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900003, 0, 0, 47, 0, 900004, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900003, 0, 0, 47, 0, 900005, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900003, 0, 0, 47, 0, 900006, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900003, 0, 0, 47, 0, 900007, 10, 0, 1, 'plate/tiger: not while jewelry/tiger is held'),
(19, 0, 900004, 0, 0, 47, 0, 900000, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900004, 0, 0, 47, 0, 900001, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900004, 0, 0, 47, 0, 900002, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900004, 0, 0, 47, 0, 900003, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900005, 0, 0, 47, 0, 900000, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900005, 0, 0, 47, 0, 900001, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900005, 0, 0, 47, 0, 900002, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900005, 0, 0, 47, 0, 900003, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900006, 0, 0, 47, 0, 900000, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900006, 0, 0, 47, 0, 900001, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900006, 0, 0, 47, 0, 900002, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900006, 0, 0, 47, 0, 900003, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900007, 0, 0, 47, 0, 900000, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900007, 0, 0, 47, 0, 900001, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900007, 0, 0, 47, 0, 900002, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900007, 0, 0, 47, 0, 900003, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held');

-- ============================================================
-- Baked item copies: a source cloned once per suffix, each under an entry
-- allocated from this generator's item_template block, with the suffix's
-- stats written into the row. Reclaimed through mod_moba_item_copy like the
-- +900000 copies.
-- ============================================================
CREATE TABLE IF NOT EXISTS `mod_moba_item_copy` (
    `Entry` INT UNSIGNED NOT NULL,
    `Owner` VARCHAR(32) NOT NULL,
    PRIMARY KEY (`Entry`, `Owner`)
);

-- Reclaim this generator's previous copies, sparing any a second owner
-- still claims.
DELETE it FROM `item_template` it
    JOIN `mod_moba_item_copy` mine ON mine.`Entry` = it.`entry` AND mine.`Owner` = 'quests'
    LEFT JOIN `mod_moba_item_copy` other ON other.`Entry` = it.`entry` AND other.`Owner` <> 'quests'
WHERE other.`Entry` IS NULL;

DELETE FROM `mod_moba_item_copy` WHERE `Owner` = 'quests';

-- Each clone is renumbered before the next clone of the same source lands, so
-- one source can back several copies.
DROP TEMPORARY TABLE IF EXISTS `_moba_item_bake`;
CREATE TEMPORARY TABLE `_moba_item_bake` LIKE `item_template`;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956900, `name` = 'Embossed Plate Bracers of the Tiger', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 4, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956901, `name` = 'Embossed Plate Gauntlets of the Tiger', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 4, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956902, `name` = 'Embossed Plate Girdle of the Tiger', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 4, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956903, `name` = 'Embossed Plate Boots of the Tiger', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 4, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9973;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956904, `name` = 'Tellurium Necklace of the Tiger', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 4, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956905, `name` = 'Marsh Ring of the Tiger', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 4, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

-- A copy the tracker lost would otherwise fail the INSERT on its key.
DELETE FROM `item_template` WHERE `entry` IN (956900, 956901, 956902, 956903, 956904, 956905);
INSERT INTO `item_template` SELECT * FROM `_moba_item_bake`;
DROP TEMPORARY TABLE `_moba_item_bake`;

INSERT INTO `mod_moba_item_copy` (`Entry`, `Owner`) VALUES
(956900, 'quests'),
(956901, 'quests'),
(956902, 'quests'),
(956903, 'quests'),
(956904, 'quests'),
(956905, 'quests');
