USE acore_world;

-- Points the hijacked Eye of the Storm battleground slot at map 900.
-- BattlegroundMgr.cpp does not change: battleground_template has no MapID
-- column, so the map comes from BattlemasterList.dbc via
-- bg->SetMapId(bgTemplate->BattlemasterEntry->mapid[0]). BATTLEGROUND_EY = 7 is
-- both the enum and the DBC row id.
--
-- Deliberately server-only: the client is never asked where to go. The map id
-- reaches it from bg->GetMapId() twice -- in SMSG_BATTLEFIELD_STATUS and again
-- in SMSG_NEW_WORLD. Its own BattlemasterList.dbc drives only the PvP frame's
-- name, level gate and description, so leaving row 7 at 566 there is what keeps
-- the queue window reading "Eye of the Storm" while server text says Twisted
-- Treeline. That split is the hijack working. Do NOT resolve it by mirroring
-- this row into apps/moba/wmo/dbc_tool.py.
--
-- See mod_moba_map.sql for why these *_dbc tables are the server's only source
-- and why their column COUNT aborts the worldserver if it changes.

-- Row 7 already exists in the DBC file, and that changes the rules: unlike
-- mod_moba_map.sql's new ids, DBCDatabaseLoader::Load REPLACES a record rather
-- than merging it -- it allocates a fresh buffer and walks the whole format
-- string filling every field from the SQL columns, so an omitted column takes
-- the TABLE DEFAULT, not the file's value. Hence all 32. The expensive one to
-- lose is MapID_2: LoadBattlegroundTemplates registers _battlegroundMapTemplates
-- only when mapid[1] == -1, and the column would default to 0.
--
-- Every field but MapID_1 and Name_Lang_enUS is stock row 7 verbatim, so
-- diffing this against env/dist/bin/dbc/BattlemasterList.dbc is the check that
-- it is right. Minlevel/Maxlevel are among the fields the core never reads --
-- they are `x` in the format string -- and are kept at stock for that diff.
--
-- Row 32 (Random Battleground) resolves its map list through
-- GetBattlegroundTemplateByMapId and still names 566, so this slot is no longer
-- reachable from the random rotation. Queue Eye of the Storm directly.
DELETE FROM `battlemasterlist_dbc` WHERE `ID` = 7;
INSERT INTO `battlemasterlist_dbc` (`ID`, `MapID_1`, `MapID_2`, `MapID_3`, `MapID_4`,
`MapID_5`, `MapID_6`, `MapID_7`, `MapID_8`, `InstanceType`, `GroupsAllowed`,
`Name_Lang_enUS`, `Name_Lang_enGB`, `Name_Lang_koKR`, `Name_Lang_frFR`,
`Name_Lang_deDE`, `Name_Lang_enCN`, `Name_Lang_zhCN`, `Name_Lang_enTW`,
`Name_Lang_zhTW`, `Name_Lang_esES`, `Name_Lang_esMX`, `Name_Lang_ruRU`,
`Name_Lang_ptPT`, `Name_Lang_ptBR`, `Name_Lang_itIT`, `Name_Lang_Unk`,
`Name_Lang_Mask`, `MaxGroupSize`, `HolidayWorldState`, `Minlevel`, `Maxlevel`) VALUES
(7,900,-1,-1,-1,-1,-1,-1,-1,3,1,
'Twisted Treeline','','','','','','','','','','','','','','','',
16712190,15,2851,61,80);

-- Without a bracket for the new map GetBattlegroundBracketByLevel returns null
-- and HandleBattleFieldPortOpcode returns early -- "Enter Battle" does nothing,
-- with no error anywhere.
--
-- One wide bracket instead of stock 566's three (61-69/70-79/80-85), so testers
-- at any level share a queue.
--
-- MinLevel may not go below battleground_template.MinLvl (61). Every queue --
-- not just Random Battleground -- runs CreateNewBattleground -> GetRandomBG,
-- which keeps a candidate template only while bg->MinLevel <= bracketEntry
-- ->minLevel. Set it lower and the candidate list is empty,
-- SelectRandomWeightedContainerElement returns 0, and the console reports
-- "bg template not found for 0" -- naming neither this table nor the cause.
-- Lowering the floor for low-level testing means lowering
-- battleground_template.MinLvl first or in the same change -- and gen_base.py
-- does not emit that field today, so it is a base_config.yaml plus generator
-- change, not just a new value.
DELETE FROM `pvpdifficulty_dbc` WHERE `ID` BETWEEN 200 AND 209;
INSERT INTO `pvpdifficulty_dbc` (`ID`, `MapID`, `RangeIndex`, `MinLevel`, `MaxLevel`, `Difficulty`) VALUES
(200,900,0,61,85,0);
