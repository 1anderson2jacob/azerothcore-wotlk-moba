-- MobaClock: shows elapsed MOBA match time, fed by the server over the addon channel.
--
-- Protocol (server -> client, LANG_ADDON chat message, prefix "MobaClock"):
--   "T:<seconds>"  (re)start/sync the clock to <seconds> elapsed, and show it
--   "E"            hide the clock (match ended)
-- The addon counts up locally between messages, so the server only sends on
-- start, on late-join, every ~10s as a resync, and on end.

local ADDON_NAME = "MobaClock"
local PREFIX     = "MobaClock"
local HIDE_MSG   = "E"

local DEFAULTS = { point = "TOP", relPoint = "TOP", x = 0, y = -200, locked = false }

local function Print(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff33ff99MobaClock|r: " .. tostring(msg))
end

-- ---- the on-screen frame -------------------------------------------------
local frame = CreateFrame("Frame", "MobaClockFrame", UIParent)
frame:SetWidth(120)
frame:SetHeight(30)
frame:SetFrameStrata("MEDIUM")
frame:SetMovable(true)
frame:SetClampedToScreen(true)
frame:RegisterForDrag("LeftButton")
frame:SetPoint("TOP", UIParent, "TOP", 0, -200)   -- valid default anchor; overridden on load
frame:Hide()

local bg = frame:CreateTexture(nil, "BACKGROUND")
bg:SetAllPoints(frame)
bg:SetTexture(0, 0, 0, 0.5)

local label = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
label:SetPoint("CENTER", frame, "CENTER", 0, 0)
label:SetFont("Fonts\\FRIZQT__.TTF", 20, "OUTLINE")
label:SetText("0:00")

-- ---- state ---------------------------------------------------------------
local running  = false
local baseTime = 0    -- GetTime() value that corresponds to elapsed 0
local throttle = 0

local function FormatTime(sec)
    sec = math.floor(sec + 0.5)
    if sec < 0 then sec = 0 end
    local h = math.floor(sec / 3600)
    local m = math.floor((sec % 3600) / 60)
    local s = sec % 60
    if h > 0 then
        return string.format("%d:%02d:%02d", h, m, s)
    end
    return string.format("%d:%02d", m, s)
end

frame:SetScript("OnUpdate", function(self, elapsed)
    if not running then return end
    throttle = throttle + elapsed
    if throttle < 0.2 then return end     -- refresh ~5x/sec, plenty for MM:SS
    throttle = 0
    label:SetText(FormatTime(GetTime() - baseTime))
end)

frame:SetScript("OnDragStart", function(self)
    if self:IsMovable() then self:StartMoving() end
end)
frame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    local point, _, relPoint, x, y = self:GetPoint()
    MobaClockDB.point, MobaClockDB.relPoint, MobaClockDB.x, MobaClockDB.y = point, relPoint, x, y
end)

local function ApplyPosition()
    frame:ClearAllPoints()
    frame:SetPoint(MobaClockDB.point, UIParent, MobaClockDB.relPoint, MobaClockDB.x, MobaClockDB.y)
end

local function ApplyLock()
    frame:EnableMouse(not MobaClockDB.locked)
end

local function StartClock(elapsedSec)
    baseTime = GetTime() - elapsedSec
    running  = true
    label:SetText(FormatTime(elapsedSec))
    frame:Show()
end

local function StopClock()
    running = false
    frame:Hide()
end

-- ---- addon-message handling ---------------------------------------------
local function HandlePayload(payload)
    if payload == HIDE_MSG then
        StopClock()
        return
    end
    local sec = tonumber(string.match(payload, "^T:(%d+)$"))
    if sec then StartClock(sec) end
end

local function OnAddonMessage(prefix, message)
    if prefix == PREFIX then
        HandlePayload(message)
        return
    end
    -- Fallback for clients that don't split the tab-prefix themselves.
    local p, rest = string.match(message or "", "^([^\t]+)\t(.*)$")
    if p == PREFIX then HandlePayload(rest) end
end

-- ---- events --------------------------------------------------------------
local ev = CreateFrame("Frame")
ev:RegisterEvent("ADDON_LOADED")
ev:RegisterEvent("CHAT_MSG_ADDON")
ev:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        OnAddonMessage(prefix, message)
    elseif event == "ADDON_LOADED" then
        local name = ...
        if name == ADDON_NAME then
            MobaClockDB = MobaClockDB or {}
            for k, v in pairs(DEFAULTS) do
                if MobaClockDB[k] == nil then MobaClockDB[k] = v end
            end
            ApplyPosition()
            ApplyLock()
        end
    end
end)

-- ---- slash commands ------------------------------------------------------
SLASH_MOBACLOCK1 = "/mobaclock"
SLASH_MOBACLOCK2 = "/mclock"
SlashCmdList["MOBACLOCK"] = function(msg)
    msg = string.lower(msg or "")
    if msg == "test" then
        MobaClockDB.locked = false; ApplyLock()
        StartClock(0)
        Print("test clock started (drag to position). '/mobaclock lock' when done, '/mobaclock stop' to hide.")
    elseif msg == "stop" then
        StopClock(); Print("hidden.")
    elseif msg == "lock" then
        MobaClockDB.locked = true; ApplyLock(); Print("locked.")
    elseif msg == "unlock" then
        MobaClockDB.locked = false; ApplyLock(); Print("unlocked (drag to move).")
    elseif msg == "reset" then
        MobaClockDB.point, MobaClockDB.relPoint = DEFAULTS.point, DEFAULTS.relPoint
        MobaClockDB.x, MobaClockDB.y = DEFAULTS.x, DEFAULTS.y
        ApplyPosition(); Print("position reset.")
    else
        Print("commands: test | stop | lock | unlock | reset")
    end
end
