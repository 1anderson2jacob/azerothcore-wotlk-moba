-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_quests.py from apps/moba/maps/*/quest_config.yaml.
-- Owns the quest givers and their spawns, the invisible credit entries, every
-- quest row and its text, the one-quest-per-animal conditions, the quest block,
-- kill-credit and volume tables, and the baked reward items. Every DELETE clears this
-- generator's whole block (apps/moba/id_blocks.json), so anything dropped from
-- config goes too.
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
(900504, 'tiger quest credit', '', 1, 1, 35, 0, 1.0, 1.14286, 0, 1, 0, 10, 0, 0, 1, 1, 1, 128, '', 0),
-- monkey credit
(900505, 'monkey quest credit', '', 1, 1, 35, 0, 1.0, 1.14286, 0, 1, 0, 10, 0, 0, 1, 1, 1, 128, '', 0),
-- owl credit
(900506, 'owl quest credit', '', 1, 1, 35, 0, 1.0, 1.14286, 0, 1, 0, 10, 0, 0, 1, 1, 1, 128, '', 0),
-- whale credit
(900507, 'whale quest credit', '', 1, 1, 35, 0, 1.0, 1.14286, 0, 1, 0, 10, 0, 0, 1, 1, 1, 128, '', 0),
-- falcon credit
(900508, 'falcon quest credit', '', 1, 1, 35, 0, 1.0, 1.14286, 0, 1, 0, 10, 0, 0, 1, 1, 1, 128, '', 0);

DELETE FROM `creature_template_model` WHERE `CreatureID` BETWEEN 900500 AND 900599;
INSERT INTO `creature_template_model`
(`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`)
VALUES
(900500, 0, 3275, 1, 1, 0),
(900501, 0, 1879, 1, 1, 0),
(900502, 0, 12669, 1, 1, 0),
(900503, 0, 12681, 1, 1, 0),
(900504, 0, 11686, 1, 1, 0),
(900505, 0, 11686, 1, 1, 0),
(900506, 0, 11686, 1, 1, 0),
(900507, 0, 11686, 1, 1, 0),
(900508, 0, 11686, 1, 1, 0);

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
(`ID`, `QuestType`, `QuestLevel`, `MinLevel`, `QuestSortID`, `QuestInfoID`, `RewardXPDifficulty`, `AllowableRaces`, `RewardChoiceItemID1`, `RewardChoiceItemQuantity1`, `RewardChoiceItemID2`, `RewardChoiceItemQuantity2`, `RewardChoiceItemID3`, `RewardChoiceItemQuantity3`, `RewardChoiceItemID4`, `RewardChoiceItemQuantity4`, `RewardChoiceItemID5`, `RewardChoiceItemQuantity5`, `RewardChoiceItemID6`, `RewardChoiceItemQuantity6`, `RequiredNpcOrGo1`, `RequiredNpcOrGoCount1`, `RequiredItemId1`, `RequiredItemCount1`, `LogTitle`, `LogDescription`, `QuestDescription`, `ObjectiveText1`, `VerifiedBuild`)
VALUES
-- plate/tiger/1
(900000, 2, 40, 0, 5000, 62, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 10, 0, 0, 'Way of the Tiger (1)', 'Land the killing blow on 10 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 10 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- plate/tiger/2
(900001, 2, 40, 0, 5000, 62, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 15, 0, 0, 'Way of the Tiger (2)', 'Land the killing blow on 15 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 15 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- plate/tiger/3
(900002, 2, 40, 0, 5000, 62, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 20, 0, 0, 'Way of the Tiger (3)', 'Land the killing blow on 20 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 20 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- plate/tiger/4
(900003, 2, 40, 0, 5000, 62, 0, 0, 956900, 1, 956901, 1, 956902, 1, 956903, 1, 0, 0, 0, 0, 900504, 25, 0, 0, 'Way of the Tiger (4)', 'Land the killing blow on 25 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 25 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- plate/monkey/1
(900008, 2, 40, 0, 5000, 62, 0, 0, 956906, 1, 956907, 1, 956908, 1, 956909, 1, 0, 0, 0, 0, 900505, 21, 0, 0, 'Way of the Monkey (1)', 'Land the killing blow on 21 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 21 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- plate/monkey/2
(900009, 2, 40, 0, 5000, 62, 0, 0, 956906, 1, 956907, 1, 956908, 1, 956909, 1, 0, 0, 0, 0, 900505, 30, 0, 0, 'Way of the Monkey (2)', 'Land the killing blow on 30 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 30 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- plate/monkey/3
(900010, 2, 40, 0, 5000, 62, 0, 0, 956906, 1, 956907, 1, 956908, 1, 956909, 1, 0, 0, 0, 0, 900505, 42, 0, 0, 'Way of the Monkey (3)', 'Land the killing blow on 42 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 42 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- plate/monkey/4
(900011, 2, 40, 0, 5000, 62, 0, 0, 956906, 1, 956907, 1, 956908, 1, 956909, 1, 0, 0, 0, 0, 900505, 54, 0, 0, 'Way of the Monkey (4)', 'Land the killing blow on 54 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 54 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- plate/bear/1
(900016, 2, 40, 0, 5000, 62, 0, 0, 956912, 1, 956913, 1, 956914, 1, 956915, 1, 0, 0, 0, 0, 0, 0, 944529, 2, 'Way of the Bear (1)', 'Land the killing blow on enemy siege minions and salvage 2 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 2 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- plate/bear/2
(900017, 2, 40, 0, 5000, 62, 0, 0, 956912, 1, 956913, 1, 956914, 1, 956915, 1, 0, 0, 0, 0, 0, 0, 944529, 3, 'Way of the Bear (2)', 'Land the killing blow on enemy siege minions and salvage 3 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 3 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- plate/bear/3
(900018, 2, 40, 0, 5000, 62, 0, 0, 956912, 1, 956913, 1, 956914, 1, 956915, 1, 0, 0, 0, 0, 0, 0, 944529, 3, 'Way of the Bear (3)', 'Land the killing blow on enemy siege minions and salvage 3 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 3 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- plate/bear/4
(900019, 2, 40, 0, 5000, 62, 0, 0, 956912, 1, 956913, 1, 956914, 1, 956915, 1, 0, 0, 0, 0, 0, 0, 944529, 4, 'Way of the Bear (4)', 'Land the killing blow on enemy siege minions and salvage 4 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 4 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- plate/boar/1
(900020, 2, 40, 0, 5000, 62, 0, 0, 956916, 1, 956917, 1, 956918, 1, 956919, 1, 0, 0, 0, 0, 0, 0, 917422, 10, 'Way of the Boar (1)', 'Land the killing blow on enemy melee or caster minions and gather 10 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 10 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- plate/boar/2
(900021, 2, 40, 0, 5000, 62, 0, 0, 956916, 1, 956917, 1, 956918, 1, 956919, 1, 0, 0, 0, 0, 0, 0, 917422, 15, 'Way of the Boar (2)', 'Land the killing blow on enemy melee or caster minions and gather 15 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 15 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- plate/boar/3
(900022, 2, 40, 0, 5000, 62, 0, 0, 956916, 1, 956917, 1, 956918, 1, 956919, 1, 0, 0, 0, 0, 0, 0, 917422, 20, 'Way of the Boar (3)', 'Land the killing blow on enemy melee or caster minions and gather 20 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 20 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- plate/boar/4
(900023, 2, 40, 0, 5000, 62, 0, 0, 956916, 1, 956917, 1, 956918, 1, 956919, 1, 0, 0, 0, 0, 0, 0, 917422, 25, 'Way of the Boar (4)', 'Land the killing blow on enemy melee or caster minions and gather 25 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 25 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- plate/wolf/1
(900024, 2, 40, 0, 5000, 62, 0, 0, 956920, 1, 956921, 1, 956922, 1, 956923, 1, 0, 0, 0, 0, 0, 0, 925812, 6, 'Way of the Wolf (1)', 'Land the killing blow on worgs in the jungle camps and take 6 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 6 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- plate/wolf/2
(900025, 2, 40, 0, 5000, 62, 0, 0, 956920, 1, 956921, 1, 956922, 1, 956923, 1, 0, 0, 0, 0, 0, 0, 925812, 9, 'Way of the Wolf (2)', 'Land the killing blow on worgs in the jungle camps and take 9 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 9 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- plate/wolf/3
(900026, 2, 40, 0, 5000, 62, 0, 0, 956920, 1, 956921, 1, 956922, 1, 956923, 1, 0, 0, 0, 0, 0, 0, 925812, 12, 'Way of the Wolf (3)', 'Land the killing blow on worgs in the jungle camps and take 12 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 12 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- plate/wolf/4
(900027, 2, 40, 0, 5000, 62, 0, 0, 956920, 1, 956921, 1, 956922, 1, 956923, 1, 0, 0, 0, 0, 0, 0, 925812, 18, 'Way of the Wolf (4)', 'Land the killing blow on worgs in the jungle camps and take 18 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 18 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- plate/owl/1
(900040, 2, 40, 0, 5000, 62, 0, 0, 956930, 1, 956931, 1, 956932, 1, 956933, 1, 0, 0, 0, 0, 900506, 10, 0, 0, 'Way of the Owl (1)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 10 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 10 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- plate/owl/2
(900041, 2, 40, 0, 5000, 62, 0, 0, 956930, 1, 956931, 1, 956932, 1, 956933, 1, 0, 0, 0, 0, 900506, 15, 0, 0, 'Way of the Owl (2)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 15 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 15 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- plate/owl/3
(900042, 2, 40, 0, 5000, 62, 0, 0, 956930, 1, 956931, 1, 956932, 1, 956933, 1, 0, 0, 0, 0, 900506, 20, 0, 0, 'Way of the Owl (3)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 20 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 20 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- plate/owl/4
(900043, 2, 40, 0, 5000, 62, 0, 0, 956930, 1, 956931, 1, 956932, 1, 956933, 1, 0, 0, 0, 0, 900506, 25, 0, 0, 'Way of the Owl (4)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 25 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 25 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- plate/whale/1
(900044, 2, 40, 0, 5000, 62, 0, 0, 956934, 1, 956935, 1, 956936, 1, 956937, 1, 0, 0, 0, 0, 900507, 10, 0, 0, 'Way of the Whale (1)', 'Endure 10 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 10 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- plate/whale/2
(900045, 2, 40, 0, 5000, 62, 0, 0, 956934, 1, 956935, 1, 956936, 1, 956937, 1, 0, 0, 0, 0, 900507, 15, 0, 0, 'Way of the Whale (2)', 'Endure 15 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 15 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- plate/whale/3
(900046, 2, 40, 0, 5000, 62, 0, 0, 956934, 1, 956935, 1, 956936, 1, 956937, 1, 0, 0, 0, 0, 900507, 20, 0, 0, 'Way of the Whale (3)', 'Endure 20 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 20 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- plate/whale/4
(900047, 2, 40, 0, 5000, 62, 0, 0, 956934, 1, 956935, 1, 956936, 1, 956937, 1, 0, 0, 0, 0, 900507, 25, 0, 0, 'Way of the Whale (4)', 'Endure 25 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 25 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- plate/falcon/1
(900048, 2, 40, 0, 5000, 62, 0, 0, 956938, 1, 956939, 1, 956940, 1, 956941, 1, 0, 0, 0, 0, 900508, 10, 0, 0, 'Way of the Falcon (1)', 'Land 10 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 10 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0),
-- plate/falcon/2
(900049, 2, 40, 0, 5000, 62, 0, 0, 956938, 1, 956939, 1, 956940, 1, 956941, 1, 0, 0, 0, 0, 900508, 15, 0, 0, 'Way of the Falcon (2)', 'Land 15 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 15 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0),
-- plate/falcon/3
(900050, 2, 40, 0, 5000, 62, 0, 0, 956938, 1, 956939, 1, 956940, 1, 956941, 1, 0, 0, 0, 0, 900508, 20, 0, 0, 'Way of the Falcon (3)', 'Land 20 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 20 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0),
-- plate/falcon/4
(900051, 2, 40, 0, 5000, 62, 0, 0, 956938, 1, 956939, 1, 956940, 1, 956941, 1, 0, 0, 0, 0, 900508, 25, 0, 0, 'Way of the Falcon (4)', 'Land 25 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 25 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0),
-- jewelry/tiger/1
(900004, 2, 40, 0, 5000, 62, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 10, 0, 0, 'Way of the Tiger (1)', 'Land the killing blow on 10 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 10 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/tiger/2
(900005, 2, 40, 0, 5000, 62, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 15, 0, 0, 'Way of the Tiger (2)', 'Land the killing blow on 15 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 15 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/tiger/3
(900006, 2, 40, 0, 5000, 62, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 20, 0, 0, 'Way of the Tiger (3)', 'Land the killing blow on 20 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 20 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/tiger/4
(900007, 2, 40, 0, 5000, 62, 0, 0, 956904, 1, 956905, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900504, 25, 0, 0, 'Way of the Tiger (4)', 'Land the killing blow on 25 enemy melee or caster minions.', 'The tiger does not waste its strength on the strong, $N. It takes the stragglers: the slow, the wounded, the ones already falling.$B$BThe enemy''s minions march down these lanes by the dozen. Let others wear them down; you strike the final blow. Claim 25 of them, and the tiger''s strength will be yours to wear.', 'Minions slain', 0),
-- jewelry/monkey/1
(900012, 2, 40, 0, 5000, 62, 0, 0, 956910, 1, 956911, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900505, 21, 0, 0, 'Way of the Monkey (1)', 'Land the killing blow on 21 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 21 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- jewelry/monkey/2
(900013, 2, 40, 0, 5000, 62, 0, 0, 956910, 1, 956911, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900505, 30, 0, 0, 'Way of the Monkey (2)', 'Land the killing blow on 30 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 30 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- jewelry/monkey/3
(900014, 2, 40, 0, 5000, 62, 0, 0, 956910, 1, 956911, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900505, 42, 0, 0, 'Way of the Monkey (3)', 'Land the killing blow on 42 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 42 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- jewelry/monkey/4
(900015, 2, 40, 0, 5000, 62, 0, 0, 956910, 1, 956911, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900505, 54, 0, 0, 'Way of the Monkey (4)', 'Land the killing blow on 54 worgs, wraiths or golems in the jungle camps.', 'While the lanes grind against each other, the ground between them goes to whoever is quick enough to take it, $N. The monkey does not wait on the road for its meal. It goes into the trees, takes what it finds, and is gone before anyone thinks to look.$B$BWorgs, wraiths and golems fill the camps between the lanes, and they return as fast as they are cleared. Strike the final blow on 54 of them, and the monkey''s quickness will be yours to wear.', 'Jungle monsters slain', 0),
-- jewelry/bear/1
(900028, 2, 40, 0, 5000, 62, 0, 0, 956924, 1, 956925, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 944529, 2, 'Way of the Bear (1)', 'Land the killing blow on enemy siege minions and salvage 2 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 2 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- jewelry/bear/2
(900029, 2, 40, 0, 5000, 62, 0, 0, 956924, 1, 956925, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 944529, 3, 'Way of the Bear (2)', 'Land the killing blow on enemy siege minions and salvage 3 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 3 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- jewelry/bear/3
(900030, 2, 40, 0, 5000, 62, 0, 0, 956924, 1, 956925, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 944529, 3, 'Way of the Bear (3)', 'Land the killing blow on enemy siege minions and salvage 3 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 3 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- jewelry/bear/4
(900031, 2, 40, 0, 5000, 62, 0, 0, 956924, 1, 956925, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 944529, 4, 'Way of the Bear (4)', 'Land the killing blow on enemy siege minions and salvage 4 Demolisher Parts from the wreckage.', 'The bear does not chase every rabbit, $N. It waits for the prey worth the effort, and once its claws are in, it does not let go.$B$BEvery so often the enemy sends a demolisher down each lane: slow, armored, and built to bring down our towers. Break one, and pull what you can from the wreck. Bring me 4 Demolisher Parts, and the bear''s endurance will be yours to wear.', '', 0),
-- jewelry/boar/1
(900032, 2, 40, 0, 5000, 62, 0, 0, 956926, 1, 956927, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 917422, 10, 'Way of the Boar (1)', 'Land the killing blow on enemy melee or caster minions and gather 10 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 10 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- jewelry/boar/2
(900033, 2, 40, 0, 5000, 62, 0, 0, 956926, 1, 956927, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 917422, 15, 'Way of the Boar (2)', 'Land the killing blow on enemy melee or caster minions and gather 15 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 15 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- jewelry/boar/3
(900034, 2, 40, 0, 5000, 62, 0, 0, 956926, 1, 956927, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 917422, 20, 'Way of the Boar (3)', 'Land the killing blow on enemy melee or caster minions and gather 20 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 20 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- jewelry/boar/4
(900035, 2, 40, 0, 5000, 62, 0, 0, 956926, 1, 956927, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 917422, 25, 'Way of the Boar (4)', 'Land the killing blow on enemy melee or caster minions and gather 25 Armor Scraps from them.', 'The boar is not clever, $N, and it has never needed to be. It roots through the same ground day after day and comes away fed, while cleverer beasts go hungry.$B$BThe enemy sends its minions down these lanes in armor that will not save them, and each one you finish leaves a little of it behind. Gather 25 Armor Scraps from the fallen, and the boar''s stubbornness will be yours to wear.', '', 0),
-- jewelry/wolf/1
(900036, 2, 40, 0, 5000, 62, 0, 0, 956928, 1, 956929, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 925812, 6, 'Way of the Wolf (1)', 'Land the killing blow on worgs in the jungle camps and take 6 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 6 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- jewelry/wolf/2
(900037, 2, 40, 0, 5000, 62, 0, 0, 956928, 1, 956929, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 925812, 9, 'Way of the Wolf (2)', 'Land the killing blow on worgs in the jungle camps and take 9 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 9 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- jewelry/wolf/3
(900038, 2, 40, 0, 5000, 62, 0, 0, 956928, 1, 956929, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 925812, 12, 'Way of the Wolf (3)', 'Land the killing blow on worgs in the jungle camps and take 12 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 12 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- jewelry/wolf/4
(900039, 2, 40, 0, 5000, 62, 0, 0, 956928, 1, 956929, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 925812, 18, 'Way of the Wolf (4)', 'Land the killing blow on worgs in the jungle camps and take 18 Timber Worg Pelts from them.', 'A lone wolf is a meal for something larger, $N. A wolf that knows its ground is what everything else learns to fear.$B$BWorgs den in the jungle between the lanes, and they return to their dens as fast as they are cleared. Learn where they lie, hunt them each time they come back, and bring me 18 of their pelts. Do that, and the wolf''s cunning will be yours to wear.', '', 0),
-- jewelry/owl/1
(900052, 2, 40, 0, 5000, 62, 0, 0, 956942, 1, 956943, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900506, 10, 0, 0, 'Way of the Owl (1)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 10 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 10 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- jewelry/owl/2
(900053, 2, 40, 0, 5000, 62, 0, 0, 956942, 1, 956943, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900506, 15, 0, 0, 'Way of the Owl (2)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 15 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 15 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- jewelry/owl/3
(900054, 2, 40, 0, 5000, 62, 0, 0, 956942, 1, 956943, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900506, 20, 0, 0, 'Way of the Owl (3)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 20 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 20 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- jewelry/owl/4
(900055, 2, 40, 0, 5000, 62, 0, 0, 956942, 1, 956943, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900506, 25, 0, 0, 'Way of the Owl (4)', 'Damage enemy players, minions and jungle monsters, or heal your allies, until you have unleashed 25 measures of power. Damage to towers, inhibitors and the base does not count.', 'The owl sees the whole field before it moves, $N. It knows where its strength is needed, and it spends none of it in the wrong place.$B$BPower works the same way. Turn yours on the enemy''s champions and minions, or pour it into the allies fighting at your side; the owl counts both as wisdom put to use. Unleash 25 measures of it, and the owl''s wisdom will be yours to wear.', 'Power unleashed', 0),
-- jewelry/whale/1
(900056, 2, 40, 0, 5000, 62, 0, 0, 956944, 1, 956945, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900507, 10, 0, 0, 'Way of the Whale (1)', 'Endure 10 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 10 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- jewelry/whale/2
(900057, 2, 40, 0, 5000, 62, 0, 0, 956944, 1, 956945, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900507, 15, 0, 0, 'Way of the Whale (2)', 'Endure 15 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 15 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- jewelry/whale/3
(900058, 2, 40, 0, 5000, 62, 0, 0, 956944, 1, 956945, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900507, 20, 0, 0, 'Way of the Whale (3)', 'Endure 20 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 20 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- jewelry/whale/4
(900059, 2, 40, 0, 5000, 62, 0, 0, 956944, 1, 956945, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900507, 25, 0, 0, 'Way of the Whale (4)', 'Endure 25 measures of punishment from enemy players, minions, jungle monsters and towers. Harm you do to yourself does not count.', 'The whale is neither quick nor fierce, $N. It dives where nothing else can follow, bears the weight of the deep, and always rises again.$B$BIn the lanes, every minion, every champion and every tower will try to break you. Let them try. Stand where the blows fall, take them, and live to take more. Endure 25 measures of their punishment, and the whale''s resolve will be yours to wear.', 'Punishment endured', 0),
-- jewelry/falcon/1
(900060, 2, 40, 0, 5000, 62, 0, 0, 956946, 1, 956947, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900508, 10, 0, 0, 'Way of the Falcon (1)', 'Land 10 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 10 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0),
-- jewelry/falcon/2
(900061, 2, 40, 0, 5000, 62, 0, 0, 956946, 1, 956947, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900508, 15, 0, 0, 'Way of the Falcon (2)', 'Land 15 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 15 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0),
-- jewelry/falcon/3
(900062, 2, 40, 0, 5000, 62, 0, 0, 956946, 1, 956947, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900508, 20, 0, 0, 'Way of the Falcon (3)', 'Land 20 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 20 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0),
-- jewelry/falcon/4
(900063, 2, 40, 0, 5000, 62, 0, 0, 956946, 1, 956947, 1, 0, 0, 0, 0, 0, 0, 0, 0, 900508, 25, 0, 0, 'Way of the Falcon (4)', 'Land 25 measures of damage on enemy players and their pets.', 'The falcon is no scavenger, $N. It does not pick at what others have killed. It chooses living prey, the swiftest in the sky, and takes it on the wing.$B$BThe minions are fodder; let others squabble over them. The champions the enemy sends against us are the prey worth hunting. Find them in the lanes, strike them, and keep striking. Land 25 measures of damage on the enemy''s champions, and the falcon''s precision will be yours to wear.', 'Enemy players harried', 0);

DELETE FROM `quest_template_addon` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_template_addon`
(`ID`, `PrevQuestID`, `NextQuestID`)
VALUES
(900000, 0, 900001),
(900001, 900000, 900002),
(900002, 900001, 900003),
(900003, 900002, 0),
(900008, 0, 900009),
(900009, 900008, 900010),
(900010, 900009, 900011),
(900011, 900010, 0),
(900016, 0, 900017),
(900017, 900016, 900018),
(900018, 900017, 900019),
(900019, 900018, 0),
(900020, 0, 900021),
(900021, 900020, 900022),
(900022, 900021, 900023),
(900023, 900022, 0),
(900024, 0, 900025),
(900025, 900024, 900026),
(900026, 900025, 900027),
(900027, 900026, 0),
(900040, 0, 900041),
(900041, 900040, 900042),
(900042, 900041, 900043),
(900043, 900042, 0),
(900044, 0, 900045),
(900045, 900044, 900046),
(900046, 900045, 900047),
(900047, 900046, 0),
(900048, 0, 900049),
(900049, 900048, 900050),
(900050, 900049, 900051),
(900051, 900050, 0),
(900004, 0, 900005),
(900005, 900004, 900006),
(900006, 900005, 900007),
(900007, 900006, 0),
(900012, 0, 900013),
(900013, 900012, 900014),
(900014, 900013, 900015),
(900015, 900014, 0),
(900028, 0, 900029),
(900029, 900028, 900030),
(900030, 900029, 900031),
(900031, 900030, 0),
(900032, 0, 900033),
(900033, 900032, 900034),
(900034, 900033, 900035),
(900035, 900034, 0),
(900036, 0, 900037),
(900037, 900036, 900038),
(900038, 900037, 900039),
(900039, 900038, 0),
(900052, 0, 900053),
(900053, 900052, 900054),
(900054, 900053, 900055),
(900055, 900054, 0),
(900056, 0, 900057),
(900057, 900056, 900058),
(900058, 900057, 900059),
(900059, 900058, 0),
(900060, 0, 900061),
(900061, 900060, 900062),
(900062, 900061, 900063),
(900063, 900062, 0);

DELETE FROM `quest_details` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_details`
(`ID`, `Emote1`)
VALUES
(900000, 1),
(900001, 1),
(900002, 1),
(900003, 1),
(900008, 1),
(900009, 1),
(900010, 1),
(900011, 1),
(900016, 1),
(900017, 1),
(900018, 1),
(900019, 1),
(900020, 1),
(900021, 1),
(900022, 1),
(900023, 1),
(900024, 1),
(900025, 1),
(900026, 1),
(900027, 1),
(900040, 1),
(900041, 1),
(900042, 1),
(900043, 1),
(900044, 1),
(900045, 1),
(900046, 1),
(900047, 1),
(900048, 1),
(900049, 1),
(900050, 1),
(900051, 1),
(900004, 1),
(900005, 1),
(900006, 1),
(900007, 1),
(900012, 1),
(900013, 1),
(900014, 1),
(900015, 1),
(900028, 1),
(900029, 1),
(900030, 1),
(900031, 1),
(900032, 1),
(900033, 1),
(900034, 1),
(900035, 1),
(900036, 1),
(900037, 1),
(900038, 1),
(900039, 1),
(900052, 1),
(900053, 1),
(900054, 1),
(900055, 1),
(900056, 1),
(900057, 1),
(900058, 1),
(900059, 1),
(900060, 1),
(900061, 1),
(900062, 1),
(900063, 1);

DELETE FROM `quest_request_items` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_request_items`
(`ID`, `EmoteOnIncomplete`, `CompletionText`)
VALUES
(900000, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900001, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900002, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900003, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900008, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900009, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900010, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900011, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900016, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900017, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900018, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900019, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900020, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900021, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900022, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900023, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900024, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900025, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900026, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900027, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900040, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900041, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900042, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900043, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900044, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900045, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900046, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900047, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900048, 274, 'Your prey is still out there, $N. Hunt it.'),
(900049, 274, 'Your prey is still out there, $N. Hunt it.'),
(900050, 274, 'Your prey is still out there, $N. Hunt it.'),
(900051, 274, 'Your prey is still out there, $N. Hunt it.'),
(900004, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900005, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900006, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900007, 274, 'The tiger is patient, but it is never idle. Your hunt is not finished.'),
(900012, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900013, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900014, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900015, 274, 'The camps fill again while you stand here, $N. Back into the trees.'),
(900028, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900029, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900030, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900031, 274, 'Siege engines are few and far between, $N. Be waiting when the next one comes.'),
(900032, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900033, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900034, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900035, 274, 'Not enough yet, $N. The boar keeps digging.'),
(900036, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900037, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900038, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900039, 274, 'The worgs will be back in their dens by now, $N. So should you.'),
(900052, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900053, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900054, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900055, 274, 'Power held back is power wasted, $N. Spend more of it.'),
(900056, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900057, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900058, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900059, 274, 'You are still standing, $N. Then you can stand a little longer.'),
(900060, 274, 'Your prey is still out there, $N. Hunt it.'),
(900061, 274, 'Your prey is still out there, $N. Hunt it.'),
(900062, 274, 'Your prey is still out there, $N. Hunt it.'),
(900063, 274, 'Your prey is still out there, $N. Hunt it.');

DELETE FROM `quest_offer_reward` WHERE `ID` BETWEEN 900000 AND 900099;
INSERT INTO `quest_offer_reward`
(`ID`, `Emote1`, `RewardText`)
VALUES
(900000, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900001, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900002, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900003, 66, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900008, 66, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900009, 66, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900010, 66, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900011, 66, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900016, 66, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900017, 66, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900018, 66, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900019, 66, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900020, 66, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900021, 66, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900022, 66, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900023, 66, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900024, 66, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900025, 66, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900026, 66, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900027, 66, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900040, 66, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900041, 66, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900042, 66, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900043, 66, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900044, 66, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900045, 66, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900046, 66, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900047, 66, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900048, 66, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900049, 66, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900050, 66, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900051, 66, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900004, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900005, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900006, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900007, 2, 'Clean kills, every one. The tiger has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900012, 2, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900013, 2, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900014, 2, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900015, 2, 'Quick hands, and never a wasted step. The monkey has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900028, 2, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900029, 2, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900030, 2, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900031, 2, 'Every one of these is a tower the enemy will not take. The bear has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900032, 2, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900033, 2, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900034, 2, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900035, 2, 'A heavy haul, and every scrap of it earned. The boar has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900036, 2, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900037, 2, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900038, 2, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900039, 2, 'Thick pelts, cleanly taken. The wolf has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900052, 2, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900053, 2, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900054, 2, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900055, 2, 'Nothing spent where it was not needed. The owl has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900056, 2, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900057, 2, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900058, 2, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900059, 2, 'Battered, and still breathing. The whale has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900060, 2, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900061, 2, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900062, 2, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.'),
(900063, 2, 'Swift prey, cleanly struck. The falcon has marked you, $N.$B$BChoose what you will carry from this hunt.');

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
(900500, 900008),
(900500, 900009),
(900500, 900010),
(900500, 900011),
(900501, 900008),
(900501, 900009),
(900501, 900010),
(900501, 900011),
(900500, 900016),
(900500, 900017),
(900500, 900018),
(900500, 900019),
(900501, 900016),
(900501, 900017),
(900501, 900018),
(900501, 900019),
(900500, 900020),
(900500, 900021),
(900500, 900022),
(900500, 900023),
(900501, 900020),
(900501, 900021),
(900501, 900022),
(900501, 900023),
(900500, 900024),
(900500, 900025),
(900500, 900026),
(900500, 900027),
(900501, 900024),
(900501, 900025),
(900501, 900026),
(900501, 900027),
(900500, 900040),
(900500, 900041),
(900500, 900042),
(900500, 900043),
(900501, 900040),
(900501, 900041),
(900501, 900042),
(900501, 900043),
(900500, 900044),
(900500, 900045),
(900500, 900046),
(900500, 900047),
(900501, 900044),
(900501, 900045),
(900501, 900046),
(900501, 900047),
(900500, 900048),
(900500, 900049),
(900500, 900050),
(900500, 900051),
(900501, 900048),
(900501, 900049),
(900501, 900050),
(900501, 900051),
(900502, 900004),
(900502, 900005),
(900502, 900006),
(900502, 900007),
(900503, 900004),
(900503, 900005),
(900503, 900006),
(900503, 900007),
(900502, 900012),
(900502, 900013),
(900502, 900014),
(900502, 900015),
(900503, 900012),
(900503, 900013),
(900503, 900014),
(900503, 900015),
(900502, 900028),
(900502, 900029),
(900502, 900030),
(900502, 900031),
(900503, 900028),
(900503, 900029),
(900503, 900030),
(900503, 900031),
(900502, 900032),
(900502, 900033),
(900502, 900034),
(900502, 900035),
(900503, 900032),
(900503, 900033),
(900503, 900034),
(900503, 900035),
(900502, 900036),
(900502, 900037),
(900502, 900038),
(900502, 900039),
(900503, 900036),
(900503, 900037),
(900503, 900038),
(900503, 900039),
(900502, 900052),
(900502, 900053),
(900502, 900054),
(900502, 900055),
(900503, 900052),
(900503, 900053),
(900503, 900054),
(900503, 900055),
(900502, 900056),
(900502, 900057),
(900502, 900058),
(900502, 900059),
(900503, 900056),
(900503, 900057),
(900503, 900058),
(900503, 900059),
(900502, 900060),
(900502, 900061),
(900502, 900062),
(900502, 900063),
(900503, 900060),
(900503, 900061),
(900503, 900062),
(900503, 900063);

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
(900500, 900008),
(900500, 900009),
(900500, 900010),
(900500, 900011),
(900501, 900008),
(900501, 900009),
(900501, 900010),
(900501, 900011),
(900500, 900016),
(900500, 900017),
(900500, 900018),
(900500, 900019),
(900501, 900016),
(900501, 900017),
(900501, 900018),
(900501, 900019),
(900500, 900020),
(900500, 900021),
(900500, 900022),
(900500, 900023),
(900501, 900020),
(900501, 900021),
(900501, 900022),
(900501, 900023),
(900500, 900024),
(900500, 900025),
(900500, 900026),
(900500, 900027),
(900501, 900024),
(900501, 900025),
(900501, 900026),
(900501, 900027),
(900500, 900040),
(900500, 900041),
(900500, 900042),
(900500, 900043),
(900501, 900040),
(900501, 900041),
(900501, 900042),
(900501, 900043),
(900500, 900044),
(900500, 900045),
(900500, 900046),
(900500, 900047),
(900501, 900044),
(900501, 900045),
(900501, 900046),
(900501, 900047),
(900500, 900048),
(900500, 900049),
(900500, 900050),
(900500, 900051),
(900501, 900048),
(900501, 900049),
(900501, 900050),
(900501, 900051),
(900502, 900004),
(900502, 900005),
(900502, 900006),
(900502, 900007),
(900503, 900004),
(900503, 900005),
(900503, 900006),
(900503, 900007),
(900502, 900012),
(900502, 900013),
(900502, 900014),
(900502, 900015),
(900503, 900012),
(900503, 900013),
(900503, 900014),
(900503, 900015),
(900502, 900028),
(900502, 900029),
(900502, 900030),
(900502, 900031),
(900503, 900028),
(900503, 900029),
(900503, 900030),
(900503, 900031),
(900502, 900032),
(900502, 900033),
(900502, 900034),
(900502, 900035),
(900503, 900032),
(900503, 900033),
(900503, 900034),
(900503, 900035),
(900502, 900036),
(900502, 900037),
(900502, 900038),
(900502, 900039),
(900503, 900036),
(900503, 900037),
(900503, 900038),
(900503, 900039),
(900502, 900052),
(900502, 900053),
(900502, 900054),
(900502, 900055),
(900503, 900052),
(900503, 900053),
(900503, 900054),
(900503, 900055),
(900502, 900056),
(900502, 900057),
(900502, 900058),
(900502, 900059),
(900503, 900056),
(900503, 900057),
(900503, 900058),
(900503, 900059),
(900502, 900060),
(900502, 900061),
(900502, 900062),
(900502, 900063),
(900503, 900060),
(900503, 900061),
(900503, 900062),
(900503, 900063);

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
(19, 0, 900008, 0, 0, 47, 0, 900012, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900008, 0, 0, 47, 0, 900013, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900008, 0, 0, 47, 0, 900014, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900008, 0, 0, 47, 0, 900015, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900009, 0, 0, 47, 0, 900012, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900009, 0, 0, 47, 0, 900013, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900009, 0, 0, 47, 0, 900014, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900009, 0, 0, 47, 0, 900015, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900010, 0, 0, 47, 0, 900012, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900010, 0, 0, 47, 0, 900013, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900010, 0, 0, 47, 0, 900014, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900010, 0, 0, 47, 0, 900015, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900011, 0, 0, 47, 0, 900012, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900011, 0, 0, 47, 0, 900013, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900011, 0, 0, 47, 0, 900014, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900011, 0, 0, 47, 0, 900015, 10, 0, 1, 'plate/monkey: not while jewelry/monkey is held'),
(19, 0, 900016, 0, 0, 47, 0, 900028, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900016, 0, 0, 47, 0, 900029, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900016, 0, 0, 47, 0, 900030, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900016, 0, 0, 47, 0, 900031, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900017, 0, 0, 47, 0, 900028, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900017, 0, 0, 47, 0, 900029, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900017, 0, 0, 47, 0, 900030, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900017, 0, 0, 47, 0, 900031, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900018, 0, 0, 47, 0, 900028, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900018, 0, 0, 47, 0, 900029, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900018, 0, 0, 47, 0, 900030, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900018, 0, 0, 47, 0, 900031, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900019, 0, 0, 47, 0, 900028, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900019, 0, 0, 47, 0, 900029, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900019, 0, 0, 47, 0, 900030, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900019, 0, 0, 47, 0, 900031, 10, 0, 1, 'plate/bear: not while jewelry/bear is held'),
(19, 0, 900020, 0, 0, 47, 0, 900032, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900020, 0, 0, 47, 0, 900033, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900020, 0, 0, 47, 0, 900034, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900020, 0, 0, 47, 0, 900035, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900021, 0, 0, 47, 0, 900032, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900021, 0, 0, 47, 0, 900033, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900021, 0, 0, 47, 0, 900034, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900021, 0, 0, 47, 0, 900035, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900022, 0, 0, 47, 0, 900032, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900022, 0, 0, 47, 0, 900033, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900022, 0, 0, 47, 0, 900034, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900022, 0, 0, 47, 0, 900035, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900023, 0, 0, 47, 0, 900032, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900023, 0, 0, 47, 0, 900033, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900023, 0, 0, 47, 0, 900034, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900023, 0, 0, 47, 0, 900035, 10, 0, 1, 'plate/boar: not while jewelry/boar is held'),
(19, 0, 900024, 0, 0, 47, 0, 900036, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900024, 0, 0, 47, 0, 900037, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900024, 0, 0, 47, 0, 900038, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900024, 0, 0, 47, 0, 900039, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900025, 0, 0, 47, 0, 900036, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900025, 0, 0, 47, 0, 900037, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900025, 0, 0, 47, 0, 900038, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900025, 0, 0, 47, 0, 900039, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900026, 0, 0, 47, 0, 900036, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900026, 0, 0, 47, 0, 900037, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900026, 0, 0, 47, 0, 900038, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900026, 0, 0, 47, 0, 900039, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900027, 0, 0, 47, 0, 900036, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900027, 0, 0, 47, 0, 900037, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900027, 0, 0, 47, 0, 900038, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900027, 0, 0, 47, 0, 900039, 10, 0, 1, 'plate/wolf: not while jewelry/wolf is held'),
(19, 0, 900040, 0, 0, 47, 0, 900052, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900040, 0, 0, 47, 0, 900053, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900040, 0, 0, 47, 0, 900054, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900040, 0, 0, 47, 0, 900055, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900041, 0, 0, 47, 0, 900052, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900041, 0, 0, 47, 0, 900053, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900041, 0, 0, 47, 0, 900054, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900041, 0, 0, 47, 0, 900055, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900042, 0, 0, 47, 0, 900052, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900042, 0, 0, 47, 0, 900053, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900042, 0, 0, 47, 0, 900054, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900042, 0, 0, 47, 0, 900055, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900043, 0, 0, 47, 0, 900052, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900043, 0, 0, 47, 0, 900053, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900043, 0, 0, 47, 0, 900054, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900043, 0, 0, 47, 0, 900055, 10, 0, 1, 'plate/owl: not while jewelry/owl is held'),
(19, 0, 900044, 0, 0, 47, 0, 900056, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900044, 0, 0, 47, 0, 900057, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900044, 0, 0, 47, 0, 900058, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900044, 0, 0, 47, 0, 900059, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900045, 0, 0, 47, 0, 900056, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900045, 0, 0, 47, 0, 900057, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900045, 0, 0, 47, 0, 900058, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900045, 0, 0, 47, 0, 900059, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900046, 0, 0, 47, 0, 900056, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900046, 0, 0, 47, 0, 900057, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900046, 0, 0, 47, 0, 900058, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900046, 0, 0, 47, 0, 900059, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900047, 0, 0, 47, 0, 900056, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900047, 0, 0, 47, 0, 900057, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900047, 0, 0, 47, 0, 900058, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900047, 0, 0, 47, 0, 900059, 10, 0, 1, 'plate/whale: not while jewelry/whale is held'),
(19, 0, 900048, 0, 0, 47, 0, 900060, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900048, 0, 0, 47, 0, 900061, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900048, 0, 0, 47, 0, 900062, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900048, 0, 0, 47, 0, 900063, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900049, 0, 0, 47, 0, 900060, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900049, 0, 0, 47, 0, 900061, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900049, 0, 0, 47, 0, 900062, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900049, 0, 0, 47, 0, 900063, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900050, 0, 0, 47, 0, 900060, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900050, 0, 0, 47, 0, 900061, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900050, 0, 0, 47, 0, 900062, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900050, 0, 0, 47, 0, 900063, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900051, 0, 0, 47, 0, 900060, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900051, 0, 0, 47, 0, 900061, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900051, 0, 0, 47, 0, 900062, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
(19, 0, 900051, 0, 0, 47, 0, 900063, 10, 0, 1, 'plate/falcon: not while jewelry/falcon is held'),
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
(19, 0, 900007, 0, 0, 47, 0, 900003, 10, 0, 1, 'jewelry/tiger: not while plate/tiger is held'),
(19, 0, 900012, 0, 0, 47, 0, 900008, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900012, 0, 0, 47, 0, 900009, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900012, 0, 0, 47, 0, 900010, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900012, 0, 0, 47, 0, 900011, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900013, 0, 0, 47, 0, 900008, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900013, 0, 0, 47, 0, 900009, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900013, 0, 0, 47, 0, 900010, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900013, 0, 0, 47, 0, 900011, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900014, 0, 0, 47, 0, 900008, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900014, 0, 0, 47, 0, 900009, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900014, 0, 0, 47, 0, 900010, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900014, 0, 0, 47, 0, 900011, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900015, 0, 0, 47, 0, 900008, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900015, 0, 0, 47, 0, 900009, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900015, 0, 0, 47, 0, 900010, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900015, 0, 0, 47, 0, 900011, 10, 0, 1, 'jewelry/monkey: not while plate/monkey is held'),
(19, 0, 900028, 0, 0, 47, 0, 900016, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900028, 0, 0, 47, 0, 900017, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900028, 0, 0, 47, 0, 900018, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900028, 0, 0, 47, 0, 900019, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900029, 0, 0, 47, 0, 900016, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900029, 0, 0, 47, 0, 900017, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900029, 0, 0, 47, 0, 900018, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900029, 0, 0, 47, 0, 900019, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900030, 0, 0, 47, 0, 900016, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900030, 0, 0, 47, 0, 900017, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900030, 0, 0, 47, 0, 900018, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900030, 0, 0, 47, 0, 900019, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900031, 0, 0, 47, 0, 900016, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900031, 0, 0, 47, 0, 900017, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900031, 0, 0, 47, 0, 900018, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900031, 0, 0, 47, 0, 900019, 10, 0, 1, 'jewelry/bear: not while plate/bear is held'),
(19, 0, 900032, 0, 0, 47, 0, 900020, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900032, 0, 0, 47, 0, 900021, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900032, 0, 0, 47, 0, 900022, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900032, 0, 0, 47, 0, 900023, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900033, 0, 0, 47, 0, 900020, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900033, 0, 0, 47, 0, 900021, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900033, 0, 0, 47, 0, 900022, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900033, 0, 0, 47, 0, 900023, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900034, 0, 0, 47, 0, 900020, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900034, 0, 0, 47, 0, 900021, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900034, 0, 0, 47, 0, 900022, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900034, 0, 0, 47, 0, 900023, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900035, 0, 0, 47, 0, 900020, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900035, 0, 0, 47, 0, 900021, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900035, 0, 0, 47, 0, 900022, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900035, 0, 0, 47, 0, 900023, 10, 0, 1, 'jewelry/boar: not while plate/boar is held'),
(19, 0, 900036, 0, 0, 47, 0, 900024, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900036, 0, 0, 47, 0, 900025, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900036, 0, 0, 47, 0, 900026, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900036, 0, 0, 47, 0, 900027, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900037, 0, 0, 47, 0, 900024, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900037, 0, 0, 47, 0, 900025, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900037, 0, 0, 47, 0, 900026, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900037, 0, 0, 47, 0, 900027, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900038, 0, 0, 47, 0, 900024, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900038, 0, 0, 47, 0, 900025, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900038, 0, 0, 47, 0, 900026, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900038, 0, 0, 47, 0, 900027, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900039, 0, 0, 47, 0, 900024, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900039, 0, 0, 47, 0, 900025, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900039, 0, 0, 47, 0, 900026, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900039, 0, 0, 47, 0, 900027, 10, 0, 1, 'jewelry/wolf: not while plate/wolf is held'),
(19, 0, 900052, 0, 0, 47, 0, 900040, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900052, 0, 0, 47, 0, 900041, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900052, 0, 0, 47, 0, 900042, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900052, 0, 0, 47, 0, 900043, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900053, 0, 0, 47, 0, 900040, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900053, 0, 0, 47, 0, 900041, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900053, 0, 0, 47, 0, 900042, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900053, 0, 0, 47, 0, 900043, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900054, 0, 0, 47, 0, 900040, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900054, 0, 0, 47, 0, 900041, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900054, 0, 0, 47, 0, 900042, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900054, 0, 0, 47, 0, 900043, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900055, 0, 0, 47, 0, 900040, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900055, 0, 0, 47, 0, 900041, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900055, 0, 0, 47, 0, 900042, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900055, 0, 0, 47, 0, 900043, 10, 0, 1, 'jewelry/owl: not while plate/owl is held'),
(19, 0, 900056, 0, 0, 47, 0, 900044, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900056, 0, 0, 47, 0, 900045, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900056, 0, 0, 47, 0, 900046, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900056, 0, 0, 47, 0, 900047, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900057, 0, 0, 47, 0, 900044, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900057, 0, 0, 47, 0, 900045, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900057, 0, 0, 47, 0, 900046, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900057, 0, 0, 47, 0, 900047, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900058, 0, 0, 47, 0, 900044, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900058, 0, 0, 47, 0, 900045, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900058, 0, 0, 47, 0, 900046, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900058, 0, 0, 47, 0, 900047, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900059, 0, 0, 47, 0, 900044, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900059, 0, 0, 47, 0, 900045, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900059, 0, 0, 47, 0, 900046, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900059, 0, 0, 47, 0, 900047, 10, 0, 1, 'jewelry/whale: not while plate/whale is held'),
(19, 0, 900060, 0, 0, 47, 0, 900048, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900060, 0, 0, 47, 0, 900049, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900060, 0, 0, 47, 0, 900050, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900060, 0, 0, 47, 0, 900051, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900061, 0, 0, 47, 0, 900048, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900061, 0, 0, 47, 0, 900049, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900061, 0, 0, 47, 0, 900050, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900061, 0, 0, 47, 0, 900051, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900062, 0, 0, 47, 0, 900048, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900062, 0, 0, 47, 0, 900049, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900062, 0, 0, 47, 0, 900050, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900062, 0, 0, 47, 0, 900051, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900063, 0, 0, 47, 0, 900048, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900063, 0, 0, 47, 0, 900049, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900063, 0, 0, 47, 0, 900050, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held'),
(19, 0, 900063, 0, 0, 47, 0, 900051, 10, 0, 1, 'jewelry/falcon: not while plate/falcon is held');

DROP TABLE IF EXISTS `mod_moba_quest_block`;
CREATE TABLE `mod_moba_quest_block` (
    `QuestId`     INT UNSIGNED NOT NULL,
    `Map`         INT UNSIGNED NOT NULL,
    `ResetsChain` TINYINT UNSIGNED NOT NULL DEFAULT 0,   -- 1 = restarts its chain
    PRIMARY KEY (`QuestId`)
);

INSERT INTO `mod_moba_quest_block`
(`QuestId`, `Map`, `ResetsChain`)
VALUES
-- plate/tiger/1
(900000, 900, 0),
-- plate/tiger/2
(900001, 900, 0),
-- plate/tiger/3
(900002, 900, 0),
-- plate/tiger/4
(900003, 900, 0),
-- plate/monkey/1
(900008, 900, 0),
-- plate/monkey/2
(900009, 900, 0),
-- plate/monkey/3
(900010, 900, 0),
-- plate/monkey/4
(900011, 900, 0),
-- plate/bear/1
(900016, 900, 0),
-- plate/bear/2
(900017, 900, 0),
-- plate/bear/3
(900018, 900, 0),
-- plate/bear/4
(900019, 900, 0),
-- plate/boar/1
(900020, 900, 0),
-- plate/boar/2
(900021, 900, 0),
-- plate/boar/3
(900022, 900, 0),
-- plate/boar/4
(900023, 900, 0),
-- plate/wolf/1
(900024, 900, 0),
-- plate/wolf/2
(900025, 900, 0),
-- plate/wolf/3
(900026, 900, 0),
-- plate/wolf/4
(900027, 900, 0),
-- plate/owl/1
(900040, 900, 0),
-- plate/owl/2
(900041, 900, 0),
-- plate/owl/3
(900042, 900, 0),
-- plate/owl/4
(900043, 900, 0),
-- plate/whale/1
(900044, 900, 0),
-- plate/whale/2
(900045, 900, 0),
-- plate/whale/3
(900046, 900, 0),
-- plate/whale/4
(900047, 900, 0),
-- plate/falcon/1
(900048, 900, 0),
-- plate/falcon/2
(900049, 900, 0),
-- plate/falcon/3
(900050, 900, 0),
-- plate/falcon/4
(900051, 900, 0),
-- jewelry/tiger/1
(900004, 900, 0),
-- jewelry/tiger/2
(900005, 900, 0),
-- jewelry/tiger/3
(900006, 900, 0),
-- jewelry/tiger/4
(900007, 900, 0),
-- jewelry/monkey/1
(900012, 900, 0),
-- jewelry/monkey/2
(900013, 900, 0),
-- jewelry/monkey/3
(900014, 900, 0),
-- jewelry/monkey/4
(900015, 900, 0),
-- jewelry/bear/1
(900028, 900, 0),
-- jewelry/bear/2
(900029, 900, 0),
-- jewelry/bear/3
(900030, 900, 0),
-- jewelry/bear/4
(900031, 900, 0),
-- jewelry/boar/1
(900032, 900, 0),
-- jewelry/boar/2
(900033, 900, 0),
-- jewelry/boar/3
(900034, 900, 0),
-- jewelry/boar/4
(900035, 900, 0),
-- jewelry/wolf/1
(900036, 900, 0),
-- jewelry/wolf/2
(900037, 900, 0),
-- jewelry/wolf/3
(900038, 900, 0),
-- jewelry/wolf/4
(900039, 900, 0),
-- jewelry/owl/1
(900052, 900, 0),
-- jewelry/owl/2
(900053, 900, 0),
-- jewelry/owl/3
(900054, 900, 0),
-- jewelry/owl/4
(900055, 900, 0),
-- jewelry/whale/1
(900056, 900, 0),
-- jewelry/whale/2
(900057, 900, 0),
-- jewelry/whale/3
(900058, 900, 0),
-- jewelry/whale/4
(900059, 900, 0),
-- jewelry/falcon/1
(900060, 900, 0),
-- jewelry/falcon/2
(900061, 900, 0),
-- jewelry/falcon/3
(900062, 900, 0),
-- jewelry/falcon/4
(900063, 900, 0);

DROP TABLE IF EXISTS `mod_moba_quest_credit`;
CREATE TABLE `mod_moba_quest_credit` (
    `CreatureEntry` INT UNSIGNED NOT NULL,
    `Map`           INT UNSIGNED NOT NULL,
    `CreditEntry`   INT UNSIGNED NOT NULL,
    PRIMARY KEY (`CreatureEntry`)
);

INSERT INTO `mod_moba_quest_credit`
(`CreatureEntry`, `Map`, `CreditEntry`)
VALUES
-- alliance_top_melee_right
(900020, 900, 900504),
-- alliance_top_melee_left
(900021, 900, 900504),
-- alliance_bot_melee_right
(900030, 900, 900504),
-- alliance_bot_melee_left
(900031, 900, 900504),
-- horde_top_melee_right
(900022, 900, 900504),
-- horde_top_melee_left
(900023, 900, 900504),
-- horde_bot_melee_right
(900032, 900, 900504),
-- horde_bot_melee_left
(900033, 900, 900504),
-- alliance_top_caster
(900024, 900, 900504),
-- alliance_bot_caster
(900034, 900, 900504),
-- horde_top_caster
(900025, 900, 900504),
-- horde_bot_caster
(900035, 900, 900504),
-- wolf_large
(900209, 900, 900505),
-- wolf_small
(900210, 900, 900505),
-- wraith_large
(900211, 900, 900505),
-- wraith_small
(900212, 900, 900505),
-- golem_large
(900213, 900, 900505),
-- golem_small
(900214, 900, 900505);

DROP TABLE IF EXISTS `mod_moba_quest_volume`;
CREATE TABLE `mod_moba_quest_volume` (
    `CreditEntry` INT UNSIGNED NOT NULL,
    `Map`         INT UNSIGNED NOT NULL,
    `Feeds`       TINYINT UNSIGNED NOT NULL,   -- 1 = damage_dealt, 2 = healing_done, 4 = damage_taken, 8 = player_damage
    `Chunk`       INT UNSIGNED NOT NULL,       -- points per credit
    PRIMARY KEY (`CreditEntry`)
);

INSERT INTO `mod_moba_quest_volume`
(`CreditEntry`, `Map`, `Feeds`, `Chunk`)
VALUES
-- owl
(900506, 900, 3, 1000),
-- whale
(900507, 900, 4, 600),
-- falcon
(900508, 900, 8, 500);

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

-- Reclaim this generator's previous copies and their item_dbc rows, sparing any a
-- second owner still claims.
DELETE it FROM `item_template` it
    JOIN `mod_moba_item_copy` mine ON mine.`Entry` = it.`entry` AND mine.`Owner` = 'quests'
    LEFT JOIN `mod_moba_item_copy` other ON other.`Entry` = it.`entry` AND other.`Owner` <> 'quests'
WHERE other.`Entry` IS NULL;

DELETE it FROM `item_dbc` it
    JOIN `mod_moba_item_copy` mine ON mine.`Entry` = it.`ID` AND mine.`Owner` = 'quests'
    LEFT JOIN `mod_moba_item_copy` other ON other.`Entry` = it.`ID` AND other.`Owner` <> 'quests'
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

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956906, `name` = 'Embossed Plate Bracers of the Monkey', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 7, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956907, `name` = 'Embossed Plate Gauntlets of the Monkey', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 7, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956908, `name` = 'Embossed Plate Girdle of the Monkey', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 7, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956909, `name` = 'Embossed Plate Boots of the Monkey', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 7, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9973;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956912, `name` = 'Embossed Plate Bracers of the Bear', `ItemLevel` = 45,
       `stat_type1` = 4, `stat_value1` = 6, `stat_type2` = 7, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956913, `name` = 'Embossed Plate Gauntlets of the Bear', `ItemLevel` = 45,
       `stat_type1` = 4, `stat_value1` = 9, `stat_type2` = 7, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956914, `name` = 'Embossed Plate Girdle of the Bear', `ItemLevel` = 45,
       `stat_type1` = 4, `stat_value1` = 9, `stat_type2` = 7, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956915, `name` = 'Embossed Plate Boots of the Bear', `ItemLevel` = 45,
       `stat_type1` = 4, `stat_value1` = 9, `stat_type2` = 7, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9973;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956916, `name` = 'Embossed Plate Bracers of the Boar', `ItemLevel` = 45,
       `stat_type1` = 6, `stat_value1` = 6, `stat_type2` = 4, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956917, `name` = 'Embossed Plate Gauntlets of the Boar', `ItemLevel` = 45,
       `stat_type1` = 6, `stat_value1` = 9, `stat_type2` = 4, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956918, `name` = 'Embossed Plate Girdle of the Boar', `ItemLevel` = 45,
       `stat_type1` = 6, `stat_value1` = 9, `stat_type2` = 4, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956919, `name` = 'Embossed Plate Boots of the Boar', `ItemLevel` = 45,
       `stat_type1` = 6, `stat_value1` = 9, `stat_type2` = 4, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9973;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956920, `name` = 'Embossed Plate Bracers of the Wolf', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956921, `name` = 'Embossed Plate Gauntlets of the Wolf', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956922, `name` = 'Embossed Plate Girdle of the Wolf', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956923, `name` = 'Embossed Plate Boots of the Wolf', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9973;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956930, `name` = 'Embossed Plate Bracers of the Owl', `ItemLevel` = 45,
       `stat_type1` = 5, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956931, `name` = 'Embossed Plate Gauntlets of the Owl', `ItemLevel` = 45,
       `stat_type1` = 5, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956932, `name` = 'Embossed Plate Girdle of the Owl', `ItemLevel` = 45,
       `stat_type1` = 5, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956933, `name` = 'Embossed Plate Boots of the Owl', `ItemLevel` = 45,
       `stat_type1` = 5, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9973;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956934, `name` = 'Embossed Plate Bracers of the Whale', `ItemLevel` = 45,
       `stat_type1` = 7, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956935, `name` = 'Embossed Plate Gauntlets of the Whale', `ItemLevel` = 45,
       `stat_type1` = 7, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956936, `name` = 'Embossed Plate Girdle of the Whale', `ItemLevel` = 45,
       `stat_type1` = 7, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956937, `name` = 'Embossed Plate Boots of the Whale', `ItemLevel` = 45,
       `stat_type1` = 7, `stat_value1` = 9, `stat_type2` = 6, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9973;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9972;
UPDATE `_moba_item_bake`
   SET `entry` = 956938, `name` = 'Embossed Plate Bracers of the Falcon', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 5, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9972;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9967;
UPDATE `_moba_item_bake`
   SET `entry` = 956939, `name` = 'Embossed Plate Gauntlets of the Falcon', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 5, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9967;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9968;
UPDATE `_moba_item_bake`
   SET `entry` = 956940, `name` = 'Embossed Plate Girdle of the Falcon', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 5, `stat_value2` = 9,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 9968;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 9973;
UPDATE `_moba_item_bake`
   SET `entry` = 956941, `name` = 'Embossed Plate Boots of the Falcon', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 9, `stat_type2` = 5, `stat_value2` = 9,
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

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956910, `name` = 'Tellurium Necklace of the Monkey', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 7, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956911, `name` = 'Marsh Ring of the Monkey', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 7, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956924, `name` = 'Tellurium Necklace of the Bear', `ItemLevel` = 45,
       `stat_type1` = 4, `stat_value1` = 6, `stat_type2` = 7, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956925, `name` = 'Marsh Ring of the Bear', `ItemLevel` = 45,
       `stat_type1` = 4, `stat_value1` = 6, `stat_type2` = 7, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956926, `name` = 'Tellurium Necklace of the Boar', `ItemLevel` = 45,
       `stat_type1` = 6, `stat_value1` = 6, `stat_type2` = 4, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956927, `name` = 'Marsh Ring of the Boar', `ItemLevel` = 45,
       `stat_type1` = 6, `stat_value1` = 6, `stat_type2` = 4, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956928, `name` = 'Tellurium Necklace of the Wolf', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956929, `name` = 'Marsh Ring of the Wolf', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956942, `name` = 'Tellurium Necklace of the Owl', `ItemLevel` = 45,
       `stat_type1` = 5, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956943, `name` = 'Marsh Ring of the Owl', `ItemLevel` = 45,
       `stat_type1` = 5, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956944, `name` = 'Tellurium Necklace of the Whale', `ItemLevel` = 45,
       `stat_type1` = 7, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956945, `name` = 'Marsh Ring of the Whale', `ItemLevel` = 45,
       `stat_type1` = 7, `stat_value1` = 6, `stat_type2` = 6, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12023;
UPDATE `_moba_item_bake`
   SET `entry` = 956946, `name` = 'Tellurium Necklace of the Falcon', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 5, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12023;

INSERT INTO `_moba_item_bake` SELECT * FROM `item_template` WHERE `entry` = 12012;
UPDATE `_moba_item_bake`
   SET `entry` = 956947, `name` = 'Marsh Ring of the Falcon', `ItemLevel` = 45,
       `stat_type1` = 3, `stat_value1` = 6, `stat_type2` = 5, `stat_value2` = 6,
       `RandomProperty` = 0, `RandomSuffix` = 0, `itemset` = 0, `SellPrice` = 0
 WHERE `entry` = 12012;

-- A copy the tracker lost would otherwise fail the INSERT on its key.
DELETE FROM `item_template` WHERE `entry` IN (956900, 956901, 956902, 956903, 956906, 956907, 956908, 956909, 956912, 956913, 956914, 956915, 956916, 956917, 956918, 956919, 956920, 956921, 956922, 956923, 956930, 956931, 956932, 956933, 956934, 956935, 956936, 956937, 956938, 956939, 956940, 956941, 956904, 956905, 956910, 956911, 956924, 956925, 956926, 956927, 956928, 956929, 956942, 956943, 956944, 956945, 956946, 956947);
INSERT INTO `item_template` SELECT * FROM `_moba_item_bake`;
DROP TEMPORARY TABLE `_moba_item_bake`;

-- The server's own Item.dbc rows for these copies (apps/moba/README.md, Item copies).
DELETE FROM `item_dbc` WHERE `ID` IN (956900, 956901, 956902, 956903, 956906, 956907, 956908, 956909, 956912, 956913, 956914, 956915, 956916, 956917, 956918, 956919, 956920, 956921, 956922, 956923, 956930, 956931, 956932, 956933, 956934, 956935, 956936, 956937, 956938, 956939, 956940, 956941, 956904, 956905, 956910, 956911, 956924, 956925, 956926, 956927, 956928, 956929, 956942, 956943, 956944, 956945, 956946, 956947);
INSERT INTO `item_dbc` (`ID`, `ClassID`, `SubclassID`, `Sound_Override_Subclassid`, `Material`, `DisplayInfoID`, `InventoryType`, `SheatheType`)
SELECT `entry`, `class`, `subclass`, `SoundOverrideSubclass`, `Material`, `displayid`, `InventoryType`, `sheath`
  FROM `item_template` WHERE `entry` IN (956900, 956901, 956902, 956903, 956906, 956907, 956908, 956909, 956912, 956913, 956914, 956915, 956916, 956917, 956918, 956919, 956920, 956921, 956922, 956923, 956930, 956931, 956932, 956933, 956934, 956935, 956936, 956937, 956938, 956939, 956940, 956941, 956904, 956905, 956910, 956911, 956924, 956925, 956926, 956927, 956928, 956929, 956942, 956943, 956944, 956945, 956946, 956947);

INSERT INTO `mod_moba_item_copy` (`Entry`, `Owner`) VALUES
(956900, 'quests'),
(956901, 'quests'),
(956902, 'quests'),
(956903, 'quests'),
(956906, 'quests'),
(956907, 'quests'),
(956908, 'quests'),
(956909, 'quests'),
(956912, 'quests'),
(956913, 'quests'),
(956914, 'quests'),
(956915, 'quests'),
(956916, 'quests'),
(956917, 'quests'),
(956918, 'quests'),
(956919, 'quests'),
(956920, 'quests'),
(956921, 'quests'),
(956922, 'quests'),
(956923, 'quests'),
(956930, 'quests'),
(956931, 'quests'),
(956932, 'quests'),
(956933, 'quests'),
(956934, 'quests'),
(956935, 'quests'),
(956936, 'quests'),
(956937, 'quests'),
(956938, 'quests'),
(956939, 'quests'),
(956940, 'quests'),
(956941, 'quests'),
(956904, 'quests'),
(956905, 'quests'),
(956910, 'quests'),
(956911, 'quests'),
(956924, 'quests'),
(956925, 'quests'),
(956926, 'quests'),
(956927, 'quests'),
(956928, 'quests'),
(956929, 'quests'),
(956942, 'quests'),
(956943, 'quests'),
(956944, 'quests'),
(956945, 'quests'),
(956946, 'quests'),
(956947, 'quests');
