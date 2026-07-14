USE acore_world;

-- Binds the MOBA recall SpellScript to Hearthstone (spell 8690). Inside a
-- BattlegroundMOBA the script redirects the teleport to the team base; outside
-- it, Hearthstone is unchanged. Script: src/server/scripts/Custom/moba_recall.cpp.
DELETE FROM `spell_script_names` WHERE `spell_id` = 8690 AND `ScriptName` = 'spell_moba_hearthstone_recall';
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(8690, 'spell_moba_hearthstone_recall');
