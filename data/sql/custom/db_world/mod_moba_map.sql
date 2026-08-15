USE acore_world;

-- Registers Twisted Treeline server-side. These tables ship schema-only and
-- exist purely to override or extend the extracted DBCs: DBCDatabaseLoader
-- merges them additively over the file, so one row per table adds a map without
-- touching data/dbc. The CLIENT still needs the same rows inside the MPQ patch
-- (apps/moba/wmo/dbc_tool.py) -- it cannot load a map otherwise, and
-- vmap4_extractor finds the WDT by walking Map.dbc.
--
-- The extracted DBCs under env/dist/bin/dbc/ are stock client files and
-- deliberately carry none of these rows: finding no map 900 in Map.dbc means
-- nothing is broken. These tables are the only server-side source, and a row
-- here always wins -- the loader writes it over the file's index entry.
--
-- Column COUNT is load-bearing in a way column values are not: the loader runs
-- `SELECT *` and asserts the result's field count equals the DBC format string's
-- length, so adding or dropping a column here aborts the worldserver at startup.

DELETE FROM `map_dbc` WHERE `ID` BETWEEN 900 AND 909;
INSERT INTO `map_dbc` (`ID`, `Directory`, `InstanceType`, `Flags`, `PVP`,
`MapName_Lang_enUS`, `MapName_Lang_enGB`, `MapName_Lang_Mask`, `AreaTableID`,
`LoadingScreenID`, `MinimapIconScale`, `TimeOfDayOverride`, `ExpansionID`, `MaxPlayers`) VALUES
-- `Directory` is documentation here: MapEntryfmt marks field 1 `x`, so the core
-- never reads it. The client and both extractors do, and it must match the WDT
-- folder exactly. LoadingScreenID 210 is Eye of the Storm's, borrowed until
-- there is one of our own.
(900,'TwistedTreeline',3,1,1,'Twisted Treeline','Twisted Treeline',16712190,5000,210,1,-1,2,0);

DELETE FROM `areatable_dbc` WHERE `ID` BETWEEN 5000 AND 5009;
INSERT INTO `areatable_dbc` (`ID`, `ContinentID`, `ParentAreaID`, `AreaBit`, `Flags`,
`AmbienceID`, `ZoneMusic`, `ExplorationLevel`, `AreaName_Lang_enUS`, `AreaName_Lang_enGB`,
`AreaName_Lang_Mask`, `FactionGroupMask`, `MinElevation`, `Ambient_Multiplier`) VALUES
-- Flags is AREA_FLAG_OUTSIDE, deliberately NOT Eye of the Storm's 0x4000: that
-- is AREA_FLAG_OUTLAND2, which no line of the core reads. 0x04000000 is read,
-- by Map.cpp's indoor/outdoor fallback.
-- AreaBit indexes the client's exploration bitmask; reusing a stock one would
-- mark another zone explored.
(5000,900,0,3000,67108864,401,243,0,'Twisted Treeline','Twisted Treeline',16712190,0,-500,0.6);

DELETE FROM `wmoareatable_dbc` WHERE `ID` BETWEEN 51200 AND 51209;
INSERT INTO `wmoareatable_dbc` (`ID`, `WMOID`, `NameSetID`, `WMOGroupID`, `Flags`,
`AreaTableID`, `AreaName_Lang_enUS`, `AreaName_Lang_enGB`, `AreaName_Lang_Mask`) VALUES
-- The lookup key is (rootId, adtId, groupId) = (MOHD.id, MODF.NameSet, MOGP.groupID)
-- = (9000, 0, 0) for every one of our 47 groups, so the second row is the one the
-- core resolves; its 0x4 forces outdoors. The -1 row is the client's whole-WMO
-- fallback and matches the shape Blizzard ships.
-- Leaving MOHD.id at 0 would have collided with stock row 47479 (WMOID 0, group 0).
(51200,9000,0,-1,16,5000,'Twisted Treeline','Twisted Treeline',16712190),
(51201,9000,0,0,4,5000,'Twisted Treeline','Twisted Treeline',16712190);

-- Without a MapDifficulty row, ObjectMgr::LoadCreatures computes spawnMasks[900] = 0
-- (it ORs a bit per difficulty that HAS a row), so every static spawn on the map logs
-- "wrong spawn mask 1 ... not supported difficulty modes". Log-only -- nothing at spawn
-- time reads spawnMask, and the shopkeepers were spawning fine -- but it is an ERROR on
-- every boot, and a real failure hiding in that noise is how this map has bitten before.
--
-- Only the fields stock row 69 (map 566) sets are given; the rest take table defaults,
-- as the areatable/wmoareatable rows above already do. Row 69 carries no message text,
-- no RaidDuration and no MaxPlayers -- a battleground has nothing to say there.
--
-- Id 800: stock MapDifficulty.dbc runs to 753. Small for the same reason as map_dbc and
-- pvpdifficulty_dbc -- DBCStorage sizes its index table to max(id)+1.
DELETE FROM `mapdifficulty_dbc` WHERE `ID` BETWEEN 800 AND 809;
INSERT INTO `mapdifficulty_dbc` (`ID`, `MapID`, `Difficulty`, `Message_Lang_Mask`) VALUES
(800,900,0,16712188);
