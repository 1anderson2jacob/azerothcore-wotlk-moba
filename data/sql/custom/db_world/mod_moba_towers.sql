-- ============================================================
-- GENERATED FILE -- do not hand-edit.
-- Produced by apps/moba/gen_tower_data.py from apps/moba/maps/*/tower_config.json.
-- Tower CREATURES (creature_template/model/health) are shared and
-- hand-written in mod_moba_tower_defs.sql; this is only the per-map rows.
-- GuardedByEntry = 0 means always vulnerable; otherwise the tower spawns
-- inert until the referenced tower entry is destroyed (single FK).
-- ============================================================

USE acore_world;

DROP TABLE IF EXISTS `mod_moba_tower_data`;
CREATE TABLE `mod_moba_tower_data` (
    `CreatureEntry`    INT UNSIGNED NOT NULL PRIMARY KEY,
    `Map`              INT UNSIGNED NOT NULL,               -- BG map id
    `Team`             TINYINT UNSIGNED NOT NULL,           -- 0 = Alliance, 1 = Horde
    `Tier`             TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `GuardedByEntry`   INT UNSIGNED NOT NULL DEFAULT 0,      -- 0 = none / always vulnerable
    `PosX`             FLOAT NOT NULL,
    `PosY`             FLOAT NOT NULL,
    `PosZ`             FLOAT NOT NULL,
    `Orientation`      FLOAT NOT NULL,
    `AttackRange`      FLOAT NOT NULL DEFAULT 40,
    `AttackIntervalMs` INT UNSIGNED NOT NULL DEFAULT 1500,
    `AttackSpellId`    INT UNSIGNED NOT NULL DEFAULT 9053
);

INSERT INTO `mod_moba_tower_data`
(`CreatureEntry`, `Map`, `Team`, `Tier`, `GuardedByEntry`, `PosX`, `PosY`, `PosZ`, `Orientation`, `AttackRange`, `AttackIntervalMs`, `AttackSpellId`)
VALUES
(900000, 566, 0, 0, 0, 2285.5596, 1587.9965, 1165.4397, 3.2774656, 40, 1500, 9053),
(900001, 566, 1, 0, 0, 2056.0195, 1547.1702, 1162.6882, 0.21284086, 40, 1500, 9053);
