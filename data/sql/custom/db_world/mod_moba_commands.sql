USE acore_world;

-- Chat commands registered by src/server/scripts/Custom/moba_surrender.cpp.
-- Without a row the command still works, but every boot warns that help text is
-- missing and `.help` says nothing. `security` must match the SEC_* in the
-- script: when the two disagree the TABLE wins and only warns, so a stale row
-- here silently changes who may surrender.
DELETE FROM `command` WHERE `name` IN ('surrender', 'ff');
INSERT INTO `command` (`name`, `security`, `help`) VALUES
('surrender',0,'Syntax: .surrender [yes|no]\r\n\r\nStarts a surrender vote for your team, or casts your ballot in the one already running. A bare .surrender is a yes.'),
('ff',0,'Syntax: .ff [yes|no]\r\n\r\nAlias for .surrender.');
