USE acore_world;

-- Twisted Treeline's own battleground slot: BattlemasterList.dbc row 12, which is
-- also BattlegroundTypeId 12 and battleground_template.ID 12 -- one number in three
-- places by construction.
--
-- Unlike mod_moba_map.sql's rows, this one has a CLIENT half: apps/moba/wmo/dbc_tool.py
-- writes the same row into the MPQ patch. The client cannot list a battleground it
-- has no row for, and it echoes that row's id back in CMSG_BATTLEMASTER_JOIN, so the
-- two copies must agree on the ID. They need not agree on anything else -- the map
-- reaches the client from bg->GetMapId(), in SMSG_BATTLEFIELD_STATUS and again in
-- SMSG_NEW_WORLD, never from its own DBC.
--
-- See mod_moba_map.sql for why these *_dbc tables are the server's only source and
-- why their column COUNT aborts the worldserver if it changes.

-- Row 7 is Eye of the Storm again. The DELETE alone restores it: the loader merges DB
-- rows over the file, so dropping the override lets the file's stock row through.
-- Restoring it by UPDATE is not possible here -- this table ships schema-only and has
-- no row 7 of its own. battleground_template 7 and game_graveyard 1103/1104 were
-- edited too; those are undone in mod_moba_restore_eots.sql.
DELETE FROM `battlemasterlist_dbc` WHERE `ID` = 7;

-- 12 is the first id stock BattlemasterList.dbc leaves free -- it holds 1-11, 30 and
-- 32 -- and the id must stay under 32: BattlegroundMgr::SetHolidayWeekends walks every
-- type id below MAX_BATTLEGROUND_TYPE_ID doing `mask & (1 << bgtype)`, and a shift
-- that reaches an int's width is undefined.
--
-- Every column is spelled out because DBCDatabaseLoader::Load walks the whole format
-- string filling each field from the SQL columns, so an omitted one takes the TABLE
-- default. The expensive one to lose is MapID_2: LoadBattlegroundTemplates registers
-- _battlegroundMapTemplates only when mapid[1] == -1, and GetRandomBG resolves through
-- that map on EVERY queue, not just the random one.
--
-- HolidayWorldState is 0 where EotS carries 2851. Sharing the id would light Twisted
-- Treeline up as Call to Arms whenever Eye of the Storm was.
--
-- MaxGroupSize 3 is the team size, not a spare number. Group::CanJoinBattlegroundQueue
-- rejects a bigger group with ERR_BATTLEGROUND_NONE, which the client renders as
-- nothing at all -- the queue button just does nothing, with no message anywhere.
--
-- Minlevel/Maxlevel are `x` in the format string -- the server's gate is
-- battleground_template.MinLvl/MaxLvl. They are kept in step with it anyway, because
-- the CLIENT copy of this row is what decides whether the PvP frame offers the
-- battleground at all.
DELETE FROM `battlemasterlist_dbc` WHERE `ID` BETWEEN 12 AND 19;
INSERT INTO `battlemasterlist_dbc` (`ID`, `MapID_1`, `MapID_2`, `MapID_3`, `MapID_4`,
`MapID_5`, `MapID_6`, `MapID_7`, `MapID_8`, `InstanceType`, `GroupsAllowed`,
`Name_Lang_enUS`, `Name_Lang_enGB`, `Name_Lang_koKR`, `Name_Lang_frFR`,
`Name_Lang_deDE`, `Name_Lang_enCN`, `Name_Lang_zhCN`, `Name_Lang_enTW`,
`Name_Lang_zhTW`, `Name_Lang_esES`, `Name_Lang_esMX`, `Name_Lang_ruRU`,
`Name_Lang_ptPT`, `Name_Lang_ptBR`, `Name_Lang_itIT`, `Name_Lang_Unk`,
`Name_Lang_Mask`, `MaxGroupSize`, `HolidayWorldState`, `Minlevel`, `Maxlevel`) VALUES
(12,900,-1,-1,-1,-1,-1,-1,-1,3,1,
'Twisted Treeline','','','','','','','','','','','','','','','',
16712190,3,0,61,80);

-- Without a bracket for the map GetBattlegroundBracketByLevel returns null and
-- HandleBattleFieldPortOpcode returns early -- "Enter Battle" does nothing, with no
-- error anywhere.
--
-- One wide bracket instead of stock 566's three (61-69/70-79/80-85), so testers at any
-- level share a queue.
--
-- MinLevel may not go below battleground_template.MinLvl for the same slot. Every
-- queue -- not just Random Battleground -- runs CreateNewBattleground -> GetRandomBG,
-- which keeps a candidate template only while bg->MinLevel <= bracketEntry->minLevel.
-- Set it lower and the candidate list is empty, SelectRandomWeightedContainerElement
-- returns 0, and the console reports "bg template not found for 0" -- naming neither
-- this table nor the cause.
DELETE FROM `pvpdifficulty_dbc` WHERE `ID` BETWEEN 200 AND 209;
INSERT INTO `pvpdifficulty_dbc` (`ID`, `MapID`, `RangeIndex`, `MinLevel`, `MaxLevel`, `Difficulty`) VALUES
(200,900,0,61,85,0);
