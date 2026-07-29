-- Bar: the scoreboard bar -- team score, KDA, creep score, match clock. Also owns
-- the saved position and lock, since MobaHUDDB exists only to persist this frame.

local ADDON_NAME, ns = ...

local C_ALLY  = ns.C_ALLY
local C_ENEMY = ns.C_ENEMY
local C_DIM   = ns.C_DIM
local C_END   = ns.C_END

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

-- GetStringWidth rasterizes at the CURRENT effective scale, so a resolution or
-- UI-scale change invalidates every measured width. The bar's label re-measures
-- each OnUpdate tick and self-heals; digitReserve is latched on first measure and
-- would stay stale until /reload. Fixed-size widgets (revive, kill feed) never
-- measure and need nothing here.
local function RelayoutForScale()
    digitReserve = 0
    RenderClock()       -- re-runs EnsureDigitReserve at the new scale
    RenderStatic()
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

-- Bar only. Clearing the revive countdown and kill feed is ns.Feed.Clear's job;
-- the orchestrator calls both.
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

ns.Bar = {
    StartClock       = StartClock,
    Stop             = StopBar,
    Scoreboard       = HandleScoreboard,
    FormatTime       = FormatTime,
    RelayoutForScale = RelayoutForScale,

    InitSavedVars = function()
        MobaHUDDB = MobaHUDDB or {}
        for k, v in pairs(DEFAULTS) do
            if MobaHUDDB[k] == nil then MobaHUDDB[k] = v end
        end
        ApplyPosition()
        ApplyLock()
    end,

    SetLocked = function(locked)
        MobaHUDDB.locked = locked
        ApplyLock()
    end,

    ResetPosition = function()
        MobaHUDDB.point, MobaHUDDB.relPoint = DEFAULTS.point, DEFAULTS.relPoint
        MobaHUDDB.x, MobaHUDDB.y = DEFAULTS.x, DEFAULTS.y
        ApplyPosition()
    end,
}
