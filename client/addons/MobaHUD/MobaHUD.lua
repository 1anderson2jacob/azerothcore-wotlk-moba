-- MobaHUD: MOBA battleground scoreboard bar (team score, KDA, creep score, match clock).
-- Fed by the server over LANG_ADDON chat messages, prefix "MobaHUD".
--
-- Payloads (server -> client):
--   T:<seconds>                        clock start/sync (bar counts up locally), show bar
--   S:<ally>,<enemy>,<k>,<d>,<a>,<cs>  scoreboard update (team-relative: ally = you)
--   E                                  hide the bar (match end / left the match)

local ADDON_NAME = "MobaHUD"
local PREFIX     = "MobaHUD"
local HIDE_MSG   = "E"

local DEFAULTS = { point = "TOP", relPoint = "TOP", x = 0, y = -40, locked = false }

-- Swappable placeholder icons (safe stock 3.3.5a paths; a wrong path shows a "?" box).
local ICON_KDA   = "|TInterface\\Icons\\INV_Sword_04:14:14|t"
local ICON_CS    = "|TInterface\\Icons\\INV_Misc_Bone_01:14:14|t"
local ICON_CLOCK = "|TInterface\\Icons\\INV_Misc_PocketWatch_01:14:14|t"

local C_ALLY  = "|cff3399ff"  -- you (blue)
local C_ENEMY = "|cffff3333"  -- enemy (red)
local C_DIM   = "|cff999999"
local C_END   = "|r"

local function Print(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff33ff99MobaHUD|r: " .. tostring(msg))
end

-- ---- frame ---------------------------------------------------------------
local frame = CreateFrame("Frame", "MobaHUDFrame", UIParent)
frame:SetHeight(28)
frame:SetWidth(320)
frame:SetFrameStrata("MEDIUM")
frame:SetMovable(true)
frame:SetClampedToScreen(true)
frame:RegisterForDrag("LeftButton")
frame:SetPoint("TOP", UIParent, "TOP", 0, -40)   -- valid default anchor; overridden on load
frame:Hide()

local bg = frame:CreateTexture(nil, "BACKGROUND")
bg:SetAllPoints(frame)
bg:SetTexture(0, 0, 0, 0.5)

local label = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
label:SetPoint("CENTER", frame, "CENTER", 0, 0)
label:SetFont("Fonts\\FRIZQT__.TTF", 16, "OUTLINE")

-- ---- state ---------------------------------------------------------------
local running  = false
local baseTime = 0
local throttle = 0
local sb = { ally = 0, enemy = 0, k = 0, d = 0, a = 0, cs = 0 }

local function FormatTime(sec)
    sec = math.floor(sec + 0.5)
    if sec < 0 then sec = 0 end
    local h = math.floor(sec / 3600)
    local m = math.floor((sec % 3600) / 60)
    local s = sec % 60
    if h > 0 then return string.format("%d:%02d:%02d", h, m, s) end
    return string.format("%d:%02d", m, s)
end

local function Render()
    local clock = running and FormatTime(GetTime() - baseTime) or "0:00"
    local sep = "  " .. C_DIM .. "||" .. C_END .. "  "
    local score = C_ALLY .. sb.ally .. C_END .. " " .. C_DIM .. "vs" .. C_END .. " " .. C_ENEMY .. sb.enemy .. C_END
    local kda   = ICON_KDA .. " " .. sb.k .. "/" .. sb.d .. "/" .. sb.a
    local cs    = ICON_CS .. " " .. sb.cs
    local time  = ICON_CLOCK .. " " .. clock
    label:SetText(score .. sep .. kda .. sep .. cs .. sep .. time)
    frame:SetWidth(label:GetStringWidth() + 24)
end

frame:SetScript("OnUpdate", function(self, elapsed)
    if not running then return end
    throttle = throttle + elapsed
    if throttle < 0.2 then return end
    throttle = 0
    Render()
end)

frame:SetScript("OnDragStart", function(self) if self:IsMovable() then self:StartMoving() end end)
frame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    local point, _, relPoint, x, y = self:GetPoint()
    MobaHUDDB.point, MobaHUDDB.relPoint, MobaHUDDB.x, MobaHUDDB.y = point, relPoint, x, y
end)

local function ApplyPosition()
    frame:ClearAllPoints()
    frame:SetPoint(MobaHUDDB.point, UIParent, MobaHUDDB.relPoint, MobaHUDDB.x, MobaHUDDB.y)
end

local function ApplyLock()
    frame:EnableMouse(not MobaHUDDB.locked)
end

-- ---- message handling ----------------------------------------------------
local function ShowBar()
    frame:Show()
    Render()
end

local function StartClock(elapsedSec)
    baseTime = GetTime() - elapsedSec
    running  = true
    ShowBar()
end

local function StopBar()
    running = false
    frame:Hide()
end

local function HandleScoreboard(payload)
    local ally, enemy, k, d, a, cs =
        string.match(payload, "^(%d+),(%d+),(%d+),(%d+),(%d+),(%d+)$")
    if not ally then return end
    sb.ally = tonumber(ally); sb.enemy = tonumber(enemy)
    sb.k    = tonumber(k);    sb.d     = tonumber(d)
    sb.a    = tonumber(a);    sb.cs    = tonumber(cs)
    ShowBar()
end

local function HandlePayload(payload)
    if payload == HIDE_MSG then StopBar(); return end
    local sec = tonumber(string.match(payload, "^T:(%d+)$"))
    if sec then StartClock(sec); return end
    local scores = string.match(payload, "^S:(.+)$")
    if scores then HandleScoreboard(scores); return end
end

local function OnAddonMessage(prefix, message)
    if prefix == PREFIX then HandlePayload(message); return end
    local p, rest = string.match(message or "", "^([^\t]+)\t(.*)$")
    if p == PREFIX then HandlePayload(rest) end
end

-- ---- events --------------------------------------------------------------
local ev = CreateFrame("Frame")
ev:RegisterEvent("ADDON_LOADED")
ev:RegisterEvent("CHAT_MSG_ADDON")
ev:RegisterEvent("PLAYER_ENTERING_WORLD")
ev:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        OnAddonMessage(prefix, message)
    elseif event == "PLAYER_ENTERING_WORLD" then
        -- One-shot "ready" ping; the server answers with current state. Only in a
        -- battleground (covers entering the BG, joining mid-match, and /reload).
        local _, instanceType = IsInInstance()
        if instanceType == "pvp" then
            SendAddonMessage(PREFIX, "REQ", "BATTLEGROUND")
        end
    elseif event == "ADDON_LOADED" then
        local name = ...
        if name == ADDON_NAME then
            MobaHUDDB = MobaHUDDB or {}
            for k, v in pairs(DEFAULTS) do
                if MobaHUDDB[k] == nil then MobaHUDDB[k] = v end
            end
            ApplyPosition()
            ApplyLock()
        end
    end
end)

-- ---- slash ---------------------------------------------------------------
SLASH_MOBAHUD1 = "/mobahud"
SLASH_MOBAHUD2 = "/mhud"
SlashCmdList["MOBAHUD"] = function(msg)
    msg = string.lower(msg or "")
    if msg == "test" then
        MobaHUDDB.locked = false; ApplyLock()
        sb.ally, sb.enemy, sb.k, sb.d, sb.a, sb.cs = 12, 5, 8, 2, 1, 85
        StartClock(0)
        Print("test bar shown (drag to position). '/mobahud lock' when done, '/mobahud stop' to hide.")
    elseif msg == "stop" then
        StopBar(); Print("hidden.")
    elseif msg == "lock" then
        MobaHUDDB.locked = true; ApplyLock(); Print("locked.")
    elseif msg == "unlock" then
        MobaHUDDB.locked = false; ApplyLock(); Print("unlocked (drag to move).")
    elseif msg == "reset" then
        MobaHUDDB.point, MobaHUDDB.relPoint = DEFAULTS.point, DEFAULTS.relPoint
        MobaHUDDB.x, MobaHUDDB.y = DEFAULTS.x, DEFAULTS.y
        ApplyPosition(); Print("position reset.")
    else
        Print("commands: test | stop | lock | unlock | reset")
    end
end
