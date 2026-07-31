-- MobaHUD: MOBA battleground HUD -- scoreboard bar, revive countdown, kill feed.
-- Fed by the server over LANG_ADDON chat messages, prefix "MobaHUD".
--
-- Loads last, and draws nothing itself: this is the orchestrator. It owns the
-- wire protocol, the event frame and the slash command, and drives the three UI
-- modules through ns.Bar / ns.Feed / ns.Shop.
--
-- Payloads (server -> client):
--   T:<seconds>                        clock start/sync (bar counts up locally), show bar
--   S:<ally>,<enemy>,<k>,<d>,<a>,<cs>  scoreboard update (team-relative: ally = you)
--   R:<seconds>                        revive countdown start (client ticks down); 0 = hide
--   K:<pov>,<killer>,<kClass>,<kSide>,<victim>,<vClass>,<vSide>
--                                      transient kill-feed line, built per recipient:
--                                        pov  0 = you got the kill, 1 = you died, 2 = bystander
--                                        side 0 = your team (blue), 1 = enemy (red)
--                                        class = WoW class id (1-11) -> class emblem
--   D:<pov>,<vSide>,<vClass>,<victim>,<cat>
--                                      transient non-player death line (per recipient):
--                                        pov 0 = you died, 1 = bystander
--                                        vSide/vClass as in K:  (side colour, class emblem)
--                                        cat 0=environment 1=tower 2=lane creep 3=neutral
--   E                                  match over -- freeze the bar; it hides when
--                                      you leave the instance, not on this

local ADDON_NAME, ns = ...

local PREFIX      = ns.PREFIX
local SHOP_PREFIX = ns.SHOP_PREFIX
local END_MSG     = "E"
local Print       = ns.Print

local function HideAll()
    ns.Bar.Stop()
    ns.Feed.Clear()
    ns.Shop.Hide()
end

-- The server sends E for both "match ended" and "you left", so it cannot mean hide --
-- the bar has to survive the end-of-match scoreboard screen. Freeze it and drop the
-- match-time affordances; leaving the instance is what tears the HUD down.
local function EndMatch()
    ns.Bar.Freeze()
    ns.Feed.Clear()
    ns.Shop.Hide()
end

local function HandlePayload(payload)
    if payload == END_MSG then EndMatch(); return end
    local sec = tonumber(string.match(payload, "^T:(%d+)$"))
    if sec then ns.Bar.StartClock(sec); return end
    local rsec = tonumber(string.match(payload, "^R:(%d+)$"))
    if rsec ~= nil then ns.Feed.Respawn(rsec); return end
    local scores = string.match(payload, "^S:(.+)$")
    if scores then ns.Bar.Scoreboard(scores); return end
    local kill = string.match(payload, "^K:(.+)$")
    if kill then ns.Feed.Kill(kill); return end
    local death = string.match(payload, "^D:(.+)$")
    if death then ns.Feed.Death(death); return end
end

local function Dispatch(prefix, payload)
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
ev:RegisterEvent("PLAYER_MONEY")
ev:RegisterEvent("CURSOR_UPDATE")
ev:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        OnAddonMessage(prefix, message)
    elseif event == "DISPLAY_SIZE_CHANGED" or event == "UI_SCALE_CHANGED" then
        ns.Bar.RelayoutForScale()
    elseif event == "CURSOR_UPDATE" then
        ns.Shop.UpdateSellZone()
    elseif event == "PLAYER_MONEY" then
        if ns.Shop.IsShown() then ns.Shop.Render() end
    elseif event == "PLAYER_ENTERING_WORLD" then
        -- One-shot "ready" ping; the server answers with current state. Only in a
        -- battleground (covers entering the BG, joining mid-match, and /reload).
        -- Anywhere else means we just left one, which is what hides the HUD -- the
        -- E payload only freezes it.
        local _, instanceType = IsInInstance()
        if instanceType == "pvp" then
            SendAddonMessage(PREFIX, "REQ", "BATTLEGROUND")
            SendAddonMessage(SHOP_PREFIX, "HELLO", "BATTLEGROUND")
        else
            HideAll()
        end
    elseif event == "ADDON_LOADED" then
        local name = ...
        if name == ADDON_NAME then
            ns.InitDB()               -- must precede both: it owns the sub-tables
            ns.Bar.InitSavedVars()
            ns.Shop.InitSavedVars()
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
        ns.Bar.Scoreboard("12,5,8,2,1,85")
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
        if payload and payload:match("^%d+,%d+,%d+,%d+,%d+,%d+$") then
            ns.Bar.Scoreboard(payload)
            Print("scoreboard set to " .. payload .. ".")
        else
            Print("usage: /mhud sb <ally,enemy,k,d,a,cs>  e.g. /mhud sb 9,9,9,9,9,99")
        end
    elseif msg == "death" then
        ns.Feed.Respawn(10); Print("revive countdown test (10s).")
    elseif msg == "kill" then
        ns.Feed.Kill("2,Alice,1,0,Bob,4,1")
        ns.Feed.Kill("2,Carl,8,1,Dave,5,0")
        ns.Feed.Kill("0,Me,7,0,Foe,9,1")
        ns.Feed.Death("1,1,4,Eve,1")    -- enemy Eve slain by a tower
        ns.Feed.Death("1,0,5,Finn,3")   -- ally Finn slain by a neutral monster
        Print("kill + death feed test.")
    elseif msg == "stop" then
        ns.Bar.Stop(); ns.Feed.Clear(); Print("hidden.")
    elseif msg == "lock" then
        ns.Bar.SetLocked(true); Print("locked.")
    elseif msg == "unlock" then
        ns.Bar.SetLocked(false); Print("unlocked (drag to move).")
    elseif msg == "reset" then
        ns.Bar.ResetPosition()
        ns.Shop.ResetPosition()
        Print("bar and shop positions reset.")
    else
        Print("commands: test | time <m:ss> | sb <a,e,k,d,a,cs> | kill | death | stop | lock | unlock | reset")
    end
end
