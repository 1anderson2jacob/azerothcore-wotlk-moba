-- MobaHUD: MOBA battleground HUD -- scoreboard bar, revive countdown, kill feed.
-- Fed by the server over LANG_ADDON chat messages, prefix "MobaHUD".
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
--   E                                  hide the bar (match end / left the match)

local ADDON_NAME = "MobaHUD"
local PREFIX     = "MobaHUD"
local HIDE_MSG   = "E"

local DEFAULTS = { point = "TOP", relPoint = "TOP", x = 0, y = -40, locked = false }

-- Swappable placeholder icons (safe stock 3.3.5a paths; a wrong path shows a "?" box).
-- ICON_Y nudges inline icons onto the text's optical centre (they render a touch
-- high by default). Flip the sign if they end up too low after a /reload.
local ICON_Y = -1
local function Icon(path, size)
    size = size or 14
    return string.format("|T%s:%d:%d:0:%d|t", path, size, size, ICON_Y)
end
local ICON_KDA   = Icon("Interface\\Icons\\INV_Sword_04")
local ICON_CS    = Icon("Interface\\Icons\\INV_Misc_Bone_01")

local C_ALLY  = "|cff3399ff"  -- you (blue)
local C_ENEMY = "|cffff3333"  -- enemy (red)
local C_DIM   = "|cff999999"
local C_KILL  = "|cff33ff99"  -- "you slew" flavour (green)
local C_DEATH = "|cffff3333"  -- "you died" flavour (red)
local C_END   = "|r"

-- Class emblems: the round class icons atlas (256x256). Values are texel rects
-- {left, right, top, bottom} indexed by WoW class id, fed to the inline-texture
-- escape's crop coords. Mirrors the client's CLASS_ICON_TCOORDS.
local CLASS_TEX = "Interface\\TargetingFrame\\UI-Classes-Circles"
local CLASS_TCOORDS = {
    [1]  = { 0,   64,  0,   64  },  -- Warrior
    [2]  = { 0,   64,  128, 192 },  -- Paladin
    [3]  = { 0,   64,  64,  128 },  -- Hunter
    [4]  = { 127, 190, 0,   64  },  -- Rogue
    [5]  = { 127, 190, 64,  128 },  -- Priest
    [6]  = { 64,  128, 128, 192 },  -- Death Knight
    [7]  = { 64,  127, 64,  128 },  -- Shaman
    [8]  = { 64,  127, 0,   64  },  -- Mage
    [9]  = { 190, 253, 64,  128 },  -- Warlock
    [11] = { 190, 253, 0,   64  },  -- Druid
}

local function ClassIcon(classId, size)
    local t = CLASS_TCOORDS[classId]
    if not t then return "" end
    return string.format("|T%s:%d:%d:0:0:256:256:%d:%d:%d:%d|t",
        CLASS_TEX, size, size, t[1], t[2], t[3], t[4])
end

-- Non-player death sources (D: payload `cat`). Icons are swappable placeholders.
local DEATH_SOURCE = {
    [0] = { icon = "|TInterface\\Icons\\Spell_Shadow_DeathCoil:18:18|t",  label = "the environment" },
    [1] = { icon = "|TInterface\\Icons\\INV_Misc_Bomb_04:18:18|t",        label = "a tower" },
    [2] = { icon = "|TInterface\\Icons\\INV_Misc_Head_Orc_01:18:18|t",            label = "a lane creep" },
    [3] = { icon = "|TInterface\\Icons\\INV_Misc_MonsterClaw_03:18:18|t", label = "a neutral monster" },
}

local function Print(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff33ff99MobaHUD|r: " .. tostring(msg))
end

-- ---- scoreboard bar ------------------------------------------------------
local BAR_PAD      = 10        -- inner horizontal padding (inside the border)
local ICON_W       = 14        -- clock icon size
local ICON_LEAD    = 6         -- gap between the CS segment and the clock icon
local ICON_DIGIT   = 3         -- gap between the clock icon and the digit box
local DIGIT_SAMPLE = "88:88"   -- widest MM:SS; its MEASURED width sizes the digit box
                               -- (measuring beats hardcoding -- correct at any UI scale)

local frame = CreateFrame("Frame", "MobaHUDFrame", UIParent)
frame:SetHeight(30)
frame:SetWidth(320)
frame:SetFrameStrata("MEDIUM")
frame:SetMovable(true)
frame:SetClampedToScreen(true)
frame:RegisterForDrag("LeftButton")
frame:SetPoint("TOP", UIParent, "TOP", 0, -40)   -- valid default anchor; overridden on load
frame:SetBackdrop({
    bgFile   = "Interface\\Tooltips\\UI-Tooltip-Background",
    edgeFile = "Interface\\Tooltips\\UI-Tooltip-Border",
    tile = true, tileSize = 16, edgeSize = 14,
    insets = { left = 4, right = 4, top = 4, bottom = 4 },
})
frame:SetBackdropColor(0, 0, 0, 0.7)
frame:SetBackdropBorderColor(0.5, 0.5, 0.5, 1)
frame:Hide()

-- Static segments (score | kda | cs), pinned to the left edge.
local label = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
label:SetPoint("LEFT", frame, "LEFT", BAR_PAD, 0)
label:SetJustifyH("LEFT")
label:SetFont("Fonts\\FRIZQT__.TTF", 16, "OUTLINE")

-- Clock digit box, positioned RELATIVE to the label's right edge (so the gap after CS
-- is constant at any resolution) with a FIXED, measured width, right-justified. The
-- box never moves, so ticking only shuffles digits inside it.
local clockDigits = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
clockDigits:SetJustifyH("RIGHT")
clockDigits:SetPoint("LEFT", label, "RIGHT", ICON_LEAD + ICON_W + ICON_DIGIT, 0)
clockDigits:SetFont("Fonts\\FRIZQT__.TTF", 16, "OUTLINE")

-- Stopwatch icon anchored to the box's fixed left edge -- stays put, no jiggle.
local clockIcon = frame:CreateTexture(nil, "OVERLAY")
clockIcon:SetTexture("Interface\\Icons\\INV_Misc_PocketWatch_01")
clockIcon:SetWidth(ICON_W)
clockIcon:SetHeight(ICON_W)
clockIcon:SetPoint("RIGHT", clockDigits, "LEFT", -ICON_DIGIT, 0)

local running   = false
local baseTime  = 0
local throttle  = 0
local sb = { ally = 0, enemy = 0, k = 0, d = 0, a = 0, cs = 0 }

-- Measured width of DIGIT_SAMPLE at the live resolution; locks the digit box once known.
local digitReserve = 0
local function EnsureDigitReserve()
    if digitReserve > 0 then return end
    clockDigits:SetText(DIGIT_SAMPLE)
    local w = clockDigits:GetStringWidth()
    if w > 0 then
        digitReserve = w
        clockDigits:SetWidth(digitReserve)
    end
end

local function FormatTime(sec)
    sec = math.floor(sec + 0.5)
    if sec < 0 then sec = 0 end
    local h = math.floor(sec / 3600)
    local m = math.floor((sec % 3600) / 60)
    local s = sec % 60
    if h > 0 then return string.format("%d:%02d:%02d", h, m, s) end
    return string.format("%d:%02d", m, s)
end

local SEP = "  " .. C_DIM .. "||" .. C_END .. "  "

-- Bar width = padding + static segments + clock zone + padding. Clock zone uses the
-- measured reserve (fallback until it's known), so it's constant between scoreboard
-- updates -> no jitter, backdrop stays matched, stale GetStringWidth self-heals.
local function UpdateWidth()
    local reserve   = digitReserve > 0 and digitReserve or 56
    local clockZone = ICON_LEAD + ICON_W + ICON_DIGIT + reserve
    frame:SetWidth(BAR_PAD + label:GetStringWidth() + clockZone + BAR_PAD)
end

-- Static segments; call when score/KDA/CS change.
local function RenderStatic()
    local score = C_ALLY .. sb.ally .. C_END .. " " .. C_DIM .. "vs" .. C_END .. " " .. C_ENEMY .. sb.enemy .. C_END
    local kda   = ICON_KDA .. " " .. sb.k .. "/" .. sb.d .. "/" .. sb.a
    local cs    = ICON_CS .. " " .. sb.cs
    label:SetText(score .. SEP .. kda .. SEP .. cs)
    UpdateWidth()
end

-- Clock digits only; called every tick. The icon is set once and never touched.
local function RenderClock()
    EnsureDigitReserve()
    clockDigits:SetText(running and FormatTime(GetTime() - baseTime) or "0:00")
end

frame:SetScript("OnUpdate", function(self, elapsed)
    if not frame:IsShown() then return end
    throttle = throttle + elapsed
    if throttle < 0.2 then return end
    throttle = 0
    RenderClock()
    UpdateWidth()
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

-- ---- revive countdown ----------------------------------------------------
-- Big center-screen number while dead. Seeded once by "R:<sec>"; the client
-- ticks it down locally (like the clock). "R:0" or "E" hides it.
local respawn = CreateFrame("Frame", "MobaHUDRespawn", UIParent)
respawn:SetWidth(200)
respawn:SetHeight(80)
respawn:SetPoint("CENTER", UIParent, "CENTER", 0, 150)
respawn:SetFrameStrata("HIGH")
respawn:Hide()

local rNumber = respawn:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
rNumber:SetPoint("CENTER", respawn, "CENTER", 0, 0)
rNumber:SetFont("Fonts\\FRIZQT__.TTF", 44, "OUTLINE")

local rSub = respawn:CreateFontString(nil, "OVERLAY", "GameFontNormal")
rSub:SetPoint("TOP", rNumber, "BOTTOM", 0, -2)
rSub:SetFont("Fonts\\FRIZQT__.TTF", 13, "OUTLINE")
rSub:SetTextColor(0.8, 0.8, 0.8)
rSub:SetText("Respawning")

local rRunning  = false
local rEndTime  = 0
local rThrottle = 0

local function RespawnRender()
    local left = math.ceil(rEndTime - GetTime())
    if left < 1 then left = 1 end
    rNumber:SetText(tostring(left))
end

local function StartRespawn(sec)
    if not sec or sec <= 0 then
        rRunning = false
        respawn:Hide()
        return
    end
    rEndTime = GetTime() + sec
    rRunning = true
    respawn:Show()
    RespawnRender()
end

respawn:SetScript("OnUpdate", function(self, elapsed)
    if not rRunning then return end
    rThrottle = rThrottle + elapsed
    if rThrottle < 0.1 then return end
    rThrottle = 0
    if rEndTime - GetTime() <= 0 then
        rRunning = false
        respawn:Hide()
        return
    end
    RespawnRender()
end)

-- ---- kill feed -----------------------------------------------------------
-- Transient stack of the most recent kills, newest on top, each fading out
-- near the end of its lifetime. Not re-sent on join/reload -> no stale lines.
local KILL_MAX  = 3
local KILL_TTL  = 5.0    -- seconds a line stays up
local KILL_FADE = 1.0    -- length of the trailing fade

local killFrame = CreateFrame("Frame", "MobaHUDKillFeed", UIParent)
killFrame:SetWidth(500)
killFrame:SetHeight(KILL_MAX * 22)
killFrame:SetPoint("TOP", UIParent, "TOP", 0, -120)
killFrame:SetFrameStrata("HIGH")

local killLines = {}
for i = 1, KILL_MAX do
    local fs = killFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    fs:SetFont("Fonts\\FRIZQT__.TTF", 15, "OUTLINE")
    fs:SetPoint("TOP", killFrame, "TOP", 0, -(i - 1) * 22)
    fs:Hide()
    killLines[i] = fs
end

local killQueue = {}   -- newest first: { text = , expire = }

local function KillRender(now)
    for i = 1, KILL_MAX do
        local entry = killQueue[i]
        local fs = killLines[i]
        if entry then
            local remain = entry.expire - now
            fs:SetText(entry.text)
            fs:SetAlpha(remain < KILL_FADE and (remain / KILL_FADE) or 1)
            fs:Show()
        else
            fs:Hide()
        end
    end
end

local function PushKill(text)
    table.insert(killQueue, 1, { text = text, expire = GetTime() + KILL_TTL })
    while #killQueue > KILL_MAX do table.remove(killQueue) end
    KillRender(GetTime())
end

local function ClearKills()
    for i = #killQueue, 1, -1 do killQueue[i] = nil end
    for i = 1, KILL_MAX do killLines[i]:Hide() end
end

killFrame:SetScript("OnUpdate", function(self, elapsed)
    if #killQueue == 0 then return end
    local now = GetTime()
    -- Constant TTL + front insertion => oldest (soonest to expire) sits at the tail.
    while #killQueue > 0 and killQueue[#killQueue].expire <= now do
        table.remove(killQueue)
    end
    KillRender(now)
end)

-- ---- message handling ----------------------------------------------------
local function ShowBar()
    frame:Show()
    RenderStatic()
    RenderClock()
end

local function StartClock(elapsedSec)
    baseTime = GetTime() - elapsedSec
    running  = true
    ShowBar()
end

local function StopBar()
    running = false
    frame:Hide()
    rRunning = false
    respawn:Hide()
    ClearKills()
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

local function HandleKill(payload)
    local pov, kName, kClass, kSide, vName, vClass, vSide =
        string.match(payload, "^(%d+),([^,]+),(%d+),(%d+),([^,]+),(%d+),(%d+)$")
    if not pov then return end
    pov    = tonumber(pov)
    kClass = tonumber(kClass)
    vClass = tonumber(vClass)
    local kIcon = ClassIcon(kClass, 18)   -- killer emblem (left, all POVs)
    local vIcon = ClassIcon(vClass, 18)   -- victim emblem (right, all POVs)
    local center
    if pov == 0 then
        center = C_KILL .. "You have slain an enemy!" .. C_END
    elseif pov == 1 then
        center = C_DEATH .. "You have been slain!" .. C_END
    else
        local kColor = (tonumber(kSide) == 0) and C_ALLY or C_ENEMY
        local vColor = (tonumber(vSide) == 0) and C_ALLY or C_ENEMY
        center = kColor .. kName .. C_END
            .. " " .. C_DIM .. "has slain" .. C_END .. " "
            .. vColor .. vName .. C_END
    end
    PushKill(kIcon .. " " .. center .. " " .. vIcon)
end

local function HandleDeath(payload)
    local pov, vSide, vClass, vName, cat =
        string.match(payload, "^(%d+),(%d+),(%d+),([^,]+),(%d+)$")
    if not pov then return end
    pov    = tonumber(pov)
    vClass = tonumber(vClass)
    cat    = tonumber(cat)
    local src = DEATH_SOURCE[cat] or DEATH_SOURCE[0]
    local vIcon = ClassIcon(vClass, 18)   -- victim emblem (right, all POVs)
    local center
    if pov == 0 then
        center = C_DEATH .. "You have been slain!" .. C_END
    else
        local vColor = (tonumber(vSide) == 0) and C_ALLY or C_ENEMY
        center = vColor .. vName .. C_END
            .. " " .. C_DIM .. "was slain by " .. src.label .. C_END
    end
    PushKill(src.icon .. " " .. center .. " " .. vIcon)
end

local function HandlePayload(payload)
    if payload == HIDE_MSG then StopBar(); return end
    local sec = tonumber(string.match(payload, "^T:(%d+)$"))
    if sec then StartClock(sec); return end
    local rsec = tonumber(string.match(payload, "^R:(%d+)$"))
    if rsec ~= nil then StartRespawn(rsec); return end
    local scores = string.match(payload, "^S:(.+)$")
    if scores then HandleScoreboard(scores); return end
    local kill = string.match(payload, "^K:(.+)$")
    if kill then HandleKill(kill); return end
    local death = string.match(payload, "^D:(.+)$")
    if death then HandleDeath(death); return end
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
    elseif msg == "time" or msg:match("^time%s") then
        local arg = msg:match("^time%s+(.+)$")
        local secs
        if arg then
            local m, s = arg:match("^(%d+):(%d+)$")
            if m then secs = tonumber(m) * 60 + tonumber(s)
            else secs = tonumber(arg) end
        end
        if secs then
            StartClock(secs)
            Print("clock set to " .. FormatTime(secs) .. ".")
        else
            Print("usage: /mhud time <seconds|m:ss>  e.g. /mhud time 10:00")
        end
    elseif msg == "death" then
        StartRespawn(10); Print("revive countdown test (10s).")
    elseif msg == "kill" then
        HandleKill("2,Alice,1,0,Bob,4,1")
        HandleKill("2,Carl,8,1,Dave,5,0")
        HandleKill("0,Me,7,0,Foe,9,1")
        HandleDeath("1,1,4,Eve,1")    -- enemy Eve slain by a tower
        HandleDeath("1,0,5,Finn,3")   -- ally Finn slain by a neutral monster
        Print("kill + death feed test.")
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
        Print("commands: test | time <m:ss> | kill | death | stop | lock | unlock | reset")
    end
end
