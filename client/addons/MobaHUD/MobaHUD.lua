-- MobaHUD: MOBA battleground HUD -- scoreboard bar, revive countdown, kill feed.
-- Fed by the server over LANG_ADDON chat messages, prefix "MobaHUD".
--
-- Loads last, and draws nothing itself: this is the orchestrator. It owns the
-- wire protocol, the event frame and the slash command, and drives the three UI
-- modules through ns.Bar / ns.Feed / ns.Shop.
--
-- Payloads (server -> client):
--   T:<seconds>                        clock start/sync (bar counts up locally), show bar
--   S:<ally>,<enemy>,<k>,<d>,<a>,<cs>,<gold>  scoreboard update (team-relative:
--                                      ally = you). <gold> is the MATCH wallet in
--                                      copper -- never the character's real money.
--   R:<seconds>                        revive countdown start (client ticks down); 0 = hide
--   K:<pov>,<killer>,<kClass>,<kSide>,<victim>,<vClass>,<vSide>,<flag>
--                                      transient kill-feed line, built per recipient:
--                                        pov  0 = you got the kill, 1 = you died, 2 = bystander
--                                        side 0 = your team (blue), 1 = enemy (red)
--                                        class = WoW class id (1-11) -> class emblem
--                                        flag 0 = ordinary, 1 = first blood, 2 = shutdown
--   D:<pov>,<vSide>,<vClass>,<victim>,<cat>
--                                      transient non-player death line (per recipient):
--                                        pov 0 = you died, 1 = bystander
--                                        vSide/vClass as in K:  (side colour, class emblem)
--                                        cat 0=environment 1=tower 2=lane creep 3=neutral
--   O:<event>,<ownerSide>,<kind>,<tier>,<lane>,<actor>
--                                      transient structure line (per recipient):
--                                        event 0 = destroyed, 1 = respawning soon,
--                                                2 = respawned
--                                        ownerSide 0 = your team's structure, 1 = enemy's
--                                        kind 0 tower, 1 inhibitor, 2 core
--                                        tier 0 outer, 1 inner (turrets only)
--                                        lane 0 none, 1 top, 2 mid, 3 bot
--                                        actor = killing-blow player, EMPTY if creeps
--   X:<pov>,<side>,<name>,<type>,<count>
--                                      transient kill-streak line (per recipient):
--                                        pov 0 = it is about you, 1 = about someone else
--                                        side 0 = good news for your team, 1 = the enemy's
--                                        name = the player it is about, EMPTY for an ace
--                                        type 0 multi-kill, 1 spree, 2 ace
--                                        count = multi size / spree length; 0 for an ace
--   B:<event>,<side>,<arg>,<name>      transient boss line (per recipient):
--                                        event 0 = slain, 1 = spawning soon,
--                                                2 = spawned
--                                        side 0 = your team took it, 1 = the enemy
--                                             took it, 2 = nobody -- which every
--                                             spawn is, and a kill whose blow
--                                             resolves to no team
--                                        arg = lead seconds on "spawning soon";
--                                        0 otherwise
--                                        name = the boss's creature_template name
--   N:<code>,<arg>                     transient match-flow notice:
--                                        1 minions incoming, 2 minions have spawned,
--                                        3 victory, 4 defeat
--                                        arg = a number the wording needs (the
--                                        warning's lead time in seconds); 0 unused
--                                      Only 3/4 are per recipient -- everyone is
--                                      told whether THEY won, never which faction did
--   E                                  match over -- freeze the bar; it hides when
--                                      you leave the instance, not on this

local ADDON_NAME, ns = ...

local PREFIX      = ns.PREFIX
local SHOP_PREFIX = ns.SHOP_PREFIX
local END_MSG     = "E"
local Print       = ns.Print

-- ---- handshake -----------------------------------------------------------
-- The server's reply to REQ is the ONLY thing that raises the bar during the prep
-- phase: until the doors open it pushes nothing on its own. So a REQ that lands
-- before the server has run Battleground::AddPlayer for us is not merely late, it
-- is invisible -- the hook finds no battleground, answers nothing, and the bar
-- stays hidden until T:0 at match start. PLAYER_ENTERING_WORLD can win that race
-- on a fast load.
--
-- Ping until an answer arrives rather than once. Any MobaHUD or MobaShop payload
-- counts as the answer; the try cap keeps a NON-MOBA battleground, where nothing
-- will ever reply, from pinging for the whole match.
local HANDSHAKE_INTERVAL = 1.0
local HANDSHAKE_TRIES    = 10

local handshake = CreateFrame("Frame")
local hsElapsed = 0
local hsTries   = 0
handshake:Hide()

local function Ping()
    SendAddonMessage(PREFIX, "REQ", "BATTLEGROUND")
    SendAddonMessage(SHOP_PREFIX, "HELLO", "BATTLEGROUND")
end

handshake:SetScript("OnUpdate", function(self, elapsed)
    hsElapsed = hsElapsed + elapsed
    if hsElapsed < HANDSHAKE_INTERVAL then return end
    hsElapsed = 0
    hsTries = hsTries + 1
    if hsTries > HANDSHAKE_TRIES then self:Hide(); return end
    Ping()
end)

local function StartHandshake()
    hsElapsed, hsTries = 0, 0
    Ping()
    handshake:Show()
end

local function StopHandshake()
    handshake:Hide()
end

local function HideAll()
    StopHandshake()
    ns.Bar.Stop()
    ns.Feed.Clear()
    ns.Shop.Stop()
    ns.Minimap.Hide()
end

-- The server sends E for both "match ended" and "you left", so it cannot mean hide --
-- the bar has to survive the end-of-match scoreboard screen. Freeze it and drop the
-- match-time affordances; leaving the instance is what tears the HUD down.
--
-- The feed keeps running on purpose. The base kill that ended the match is emitted
-- immediately before this E, so clearing here would wipe the one line the player most
-- wants to see -- and the same for the victory/defeat notice. ns.Feed.EndMatch drops
-- only the revive countdown, which the engine's own resurrect has just invalidated.
local function EndMatch()
    ns.Bar.Freeze()
    ns.Feed.EndMatch()
    ns.Shop.Hide()
    ns.Minimap.Hide()
end

local function HandlePayload(payload)
    if payload == END_MSG then EndMatch(); return end
    local sec = tonumber(string.match(payload, "^T:(%d+)$"))
    if sec then ns.Bar.StartClock(sec); return end
    local rsec = tonumber(string.match(payload, "^R:(%d+)$"))
    if rsec ~= nil then ns.Feed.Respawn(rsec); return end
    local scores = string.match(payload, "^S:(.+)$")
    if scores then
        if ns.Bar.Scoreboard(scores) and ns.Shop.IsShown() then ns.Shop.Render() end
        return
    end
    local kill = string.match(payload, "^K:(.+)$")
    if kill then ns.Feed.Kill(kill); return end
    local death = string.match(payload, "^D:(.+)$")
    if death then ns.Feed.Death(death); return end
    local structure = string.match(payload, "^O:(.+)$")
    if structure then ns.Feed.Structure(structure); return end
    local streak = string.match(payload, "^X:(.+)$")
    if streak then ns.Feed.Streak(streak); return end
    local boss = string.match(payload, "^B:(.+)$")
    if boss then ns.Feed.Boss(boss); return end
    local notice = string.match(payload, "^N:(.+)$")
    if notice then ns.Feed.Notice(notice); return end
end

local function Dispatch(prefix, payload)
    StopHandshake()
    if prefix == PREFIX then HandlePayload(payload)
    elseif prefix == SHOP_PREFIX then ns.Shop.Handle(payload) end
end

local function OnAddonMessage(prefix, message)
    if prefix == PREFIX or prefix == SHOP_PREFIX then
        Dispatch(prefix, message)
        return
    end
    local p, rest = string.match(message or "", "^([^\t]+)\t(.*)$")
    if p then Dispatch(p, rest) end
end

-- ---- events --------------------------------------------------------------
local ev = CreateFrame("Frame")
ev:RegisterEvent("ADDON_LOADED")
ev:RegisterEvent("CHAT_MSG_ADDON")
ev:RegisterEvent("PLAYER_ENTERING_WORLD")
ev:RegisterEvent("DISPLAY_SIZE_CHANGED")
ev:RegisterEvent("UI_SCALE_CHANGED")
ev:RegisterEvent("CURSOR_UPDATE")
ev:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        OnAddonMessage(prefix, message)
    elseif event == "DISPLAY_SIZE_CHANGED" or event == "UI_SCALE_CHANGED" then
        ns.Bar.RelayoutForScale()
    elseif event == "CURSOR_UPDATE" then
        ns.Shop.UpdateSellZone()
    elseif event == "PLAYER_ENTERING_WORLD" then
        -- Ready ping; the server answers with current state. Only in a battleground
        -- (covers entering the BG, joining mid-match, and /reload). Anywhere else
        -- means we just left one, which is what hides the HUD -- the E payload only
        -- freezes it.
        local _, instanceType = IsInInstance()
        if instanceType == "pvp" then
            StartHandshake()
        else
            HideAll()
        end
    elseif event == "ADDON_LOADED" then
        local name = ...
        if name == ADDON_NAME then
            ns.InitDB()               -- must precede all three: it owns the sub-tables
            ns.Bar.InitSavedVars()
            ns.Shop.InitSavedVars()
            ns.Minimap.InitSavedVars()
        end
    end
end)

-- ---- slash ---------------------------------------------------------------
SLASH_MOBAHUD1 = "/mobahud"
SLASH_MOBAHUD2 = "/mhud"
SlashCmdList["MOBAHUD"] = function(msg)
    msg = string.lower(msg or "")
    if msg == "test" then
        ns.Bar.SetLocked(false)
        ns.Bar.Scoreboard("12,5,8,2,1,85,143500")
        ns.Bar.StartClock(0)
        Print("test bar shown (drag to position). '/mobahud lock' when done, '/mobahud stop' to hide.")
    elseif msg == "time" or msg:match("^time%s") then
        local arg = msg:match("^time%s+(.+)$")
        local secs
        if arg then
            local m, s = arg:match("^(%d+):(%d+)$")
            if m then secs = tonumber(m) * 60 + tonumber(s)
            else secs = tonumber(arg) end
        end
        if secs then
            ns.Bar.StartClock(secs)
            Print("clock set to " .. ns.Bar.FormatTime(secs) .. ".")
        else
            Print("usage: /mhud time <seconds|m:ss>  e.g. /mhud time 10:00")
        end
    elseif msg == "sb" or msg:match("^sb%s") then
        -- Feeds the payload to the same handler the server's S: packet lands on, so
        -- this tests the real parse-and-render path, not a shortcut around it.
        local payload = msg:match("^sb%s+(.+)$")
        if payload and payload:match("^%d+,%d+,%d+,%d+,%d+,%d+,%d+$") then
            ns.Bar.Scoreboard(payload)
            Print("scoreboard set to " .. payload .. ".")
        else
            Print("usage: /mhud sb <ally,enemy,k,d,a,cs,gold>  e.g. /mhud sb 9,9,9,9,9,99,143500")
        end
    elseif msg == "death" then
        ns.Feed.Respawn(10); Print("revive countdown test (10s).")
    elseif msg == "kill" then
        -- Note the trailing flag on every line: the K: pattern is anchored, so a
        -- payload written before the flag existed matches nothing and shows nothing.
        ns.Feed.Kill("2,Alice,1,0,Bob,4,1,0")   -- ordinary bystander kill
        ns.Feed.Kill("2,Carl,8,1,Dave,5,0,2")   -- enemy Carl shuts down ally Dave
        ns.Feed.Kill("0,Me,7,0,Foe,9,1,1")      -- your kill, and it is First Blood
        ns.Feed.Death("1,1,4,Eve,1")    -- enemy Eve slain by a tower
        ns.Feed.Death("1,0,5,Finn,3")   -- ally Finn slain by a neutral monster
        Print("kill + death feed test (ordinary, shutdown, first blood).")
    elseif msg == "streak" then
        -- Every X: shape and both POVs, including the EMPTY name field an ace
        -- carries and a spree past the last named tier.
        ns.Feed.Streak("1,0,Alice,0,2")   -- ally Alice: Double Kill
        ns.Feed.Streak("0,0,Me,0,5")      -- you: Penta Kill
        ns.Feed.Streak("1,1,Zed,1,3")     -- enemy Zed on a Killing Spree
        ns.Feed.Streak("0,0,Me,1,9")      -- you, past the ladder -> Legendary
        ns.Feed.Streak("1,0,,2,0")        -- your team aced -- note the empty name
        Print("streak test: multi-kill, spree, ladder cap, and an ace (empty name).")
    elseif msg == "feed" then
        -- Overflow on purpose: the two ranked lines go in FIRST, then enough
        -- routine kills to fill the stack twice over. Both must still be on
        -- screen at the end, sitting at the bottom.
        local P = ns.Feed.PRIO
        ns.Feed.Push(ns.C_KILL .. "First Blood!" .. ns.C_END, P.BIG)
        ns.Feed.Push(ns.C_ALLY .. "An enemy turret has fallen!" .. ns.C_END, P.EVENT)
        for i = 1, 6 do
            ns.Feed.Push(ns.C_DIM .. "routine kill " .. i .. ns.C_END, P.KILL)
        end
        Print("feed test: 2 ranked + 6 routine into 5 slots -- both ranked lines survive.")
    elseif msg == "feeddrop" then
        -- The refusal path: every slot outranks a routine kill, so the routine
        -- kill is dropped rather than displacing something bigger.
        local P = ns.Feed.PRIO
        for i = 1, 5 do
            ns.Feed.Push(ns.C_KILL .. "big event " .. i .. ns.C_END, P.BIG)
        end
        ns.Feed.Push(ns.C_ENEMY .. "routine kill (must NOT appear)" .. ns.C_END, P.KILL)
        Print("feed test: 5 big events + 1 routine -- the routine kill is refused.")
    elseif msg == "struct" then
        -- Every structure line and both POVs, including the no-actor wording a
        -- creep-finished structure produces (note the trailing empty field).
        ns.Feed.Structure("0,1,0,0,2,Alice")   -- enemy outer mid turret, player kill
        ns.Feed.Structure("0,0,0,0,2,")        -- your outer mid turret, creep kill
        ns.Feed.Structure("0,1,1,1,2,Bob")     -- enemy mid inhibitor down
        ns.Feed.Structure("1,0,1,1,2,")        -- your mid inhibitor respawning soon
        ns.Feed.Structure("2,0,1,1,2,")        -- your mid inhibitor is back
        ns.Feed.Structure("0,1,2,2,0,Carl")    -- enemy base -- no lane word
        Print("structure test: 6 lines into 5 slots -- the oldest turret evicts, both inhibitor lines and the base survive.")
    elseif msg == "notice" then
        -- Every N: code, in the order a real match produces them. Note the arg
        -- field on all four: the pattern is anchored, so a payload written before
        -- it existed matches nothing and shows nothing.
        ns.Feed.Notice("1,10")   -- minions incoming, 10s lead
        ns.Feed.Notice("2,0")    -- minions have spawned
        ns.Feed.Notice("3,0")    -- victory
        ns.Feed.Notice("4,0")    -- defeat
        Print("notice test: both minion lines, victory and defeat.")
    elseif msg == "boss" then
        -- Every B: event and all three sides, in the order a real match produces
        -- them. The feed renders newest-on-top, so this reads bottom-to-top.
        ns.Feed.Boss("1,2,15,Rosham Bo")   -- spawning soon, 15s lead
        ns.Feed.Boss("2,2,0,Rosham Bo")    -- has spawned
        ns.Feed.Boss("0,0,0,Rosham Bo")    -- your team took it
        ns.Feed.Boss("0,1,0,Rosham Bo")    -- the enemy took it
        ns.Feed.Boss("0,2,0,Rosham Bo")    -- killing blow resolved to nobody
        Print("boss test: both spawn lines and all three slain forms.")
    elseif msg == "stop" then
        ns.Bar.Stop(); ns.Feed.Clear(); Print("hidden.")
    elseif msg == "lock" then
        ns.Bar.SetLocked(true); Print("locked.")
    elseif msg == "unlock" then
        ns.Bar.SetLocked(false); Print("unlocked (drag to move).")
    elseif msg == "reset" then
        ns.Bar.ResetPosition()
        ns.Shop.ResetPosition()
        ns.Minimap.ResetPosition()
        Print("bar, shop and minimap button positions reset.")
    elseif msg == "shop" then
        ns.Shop.Toggle()
    elseif msg == "end" then
        -- Drives the real E path, in the real order: a big line lands, then the
        -- match ends. The line must stay; the revive countdown must go.
        ns.Feed.Respawn(10)
        ns.Feed.Push(ns.C_KILL .. "The enemy base has fallen!" .. ns.C_END, ns.Feed.PRIO.BIG)
        EndMatch()
        Print("match-end test: the base line stays up, the revive countdown clears.")
    else
        Print("commands: test | time <m:ss> | sb <a,e,k,d,a,cs,gold> | kill | death | feed | feeddrop | struct | streak | notice | boss | end | shop | stop | lock | unlock | reset")
    end
end

-- ---- keybinding ----------------------------------------------------------
-- Bindings.xml is auto-loaded from the addon folder and is deliberately NOT in the
-- .toc. Its body runs in the GLOBAL environment, where `ns` does not exist, so this
-- function is the door it knocks on -- the same role the slash command plays above.
-- The Key Bindings UI finds the two labels by concatenating the XML's `header` and
-- `name` attributes onto these prefixes; all four strings have to agree or the panel
-- lists the raw binding name instead.
BINDING_HEADER_MOBAHUD          = "MOBA HUD"
BINDING_NAME_MOBAHUD_TOGGLESHOP = "Toggle shop panel"

function MobaHUD_ToggleShop()
    ns.Shop.Toggle()
end
