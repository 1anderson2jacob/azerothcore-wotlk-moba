-- Bar: the scoreboard bar -- team score, KDA, creep score, match gold, match clock.
-- Also owns its saved position and lock, which live under MobaHUDDB.bar (ns.InitDB
-- owns the table and migrated the flat layout this file used to assume).

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
local ICON_KDA = Icon("Interface\\Icons\\INV_Sword_04")
local ICON_CS  = Icon("Interface\\Icons\\INV_Misc_Bone_01")

-- Coin metrics for this bar's 16pt font; see ns.MoneyFormatter for why the stock
-- helper is not used. COIN_Y is the same idea as ICON_Y, kept separate because coin
-- art is not icon art and the two do not want the same nudge.
local COIN_SIZE = 12
local COIN_Y    = -1
local FormatMoney = ns.MoneyFormatter(COIN_SIZE, COIN_Y)

local BAR_PAD   = 10        -- inner horizontal padding (inside the border)
local SEP_PAD   = 6         -- breathing room either side of a || separator
local ICON_GAP  = 4         -- gap between a column's icon and its number
local CLOCK_GAP = 12        -- gap between the CS segment and the clock
local COL_SLACK = 2         -- per-column overflow guard; see EnsureReserves

-- The clock is the ONE thing on the bar that redraws while nothing has happened, so
-- it is the one thing that must not reflow. FRIZQT__ is proportional -- its digits
-- have different advance widths, so 0:02 and 0:03 do not measure the same and the
-- string visibly breathes. ARIALN has tabular figures, which is why Blizzard uses it
-- for floating combat numbers. It is deliberately the only non-FRIZQT__ font in this
-- addon; the clock is dimmed anyway, so reading as a separate element is fine.
-- ARIALN renders narrower than FRIZQT__ at the same pt -- CLOCK_SIZE is the knob if
-- it looks small next to the scores.
--
-- CLOCK_Y is the same trick as ICON_Y, for the same reason: FontStrings centre their
-- BOUNDING BOX, not their baseline, and ARIALN's descent is proportionally deeper
-- than FRIZQT__'s. A digits-only string never uses that reserved descender space, so
-- centring the boxes leaves the clock sitting low. Re-tune it if CLOCK_SIZE changes.
local CLOCK_FONT = "Fonts\\ARIALN.TTF"
local CLOCK_SIZE = 17
local CLOCK_Y    = 0.5

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

local SEP_TEXT = C_DIM .. "||" .. C_END

local function MakeText(justify)
    local fs = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    fs:SetFont("Fonts\\FRIZQT__.TTF", 16, "OUTLINE")
    fs:SetJustifyH(justify)
    return fs
end

-- Three fixed-width columns chained left to right, each sized once for its own worst
-- case. Column boundaries, separators and both bar ends therefore never move, whatever
-- the score does -- the old single-FontString label re-widthed the whole frame, and
-- because the frame is centre-anchored that shuffled both ends on every digit gained.
--
-- Every column centres its number inside its box, so numbers DO nudge half a digit
-- when the digit count changes -- twice a match per field, at 10 and at 100. Each
-- icon is a separate fixed element rather than part of the centred string, so icons
-- never drift with them.
local scoreCol = MakeText("CENTER")
scoreCol:SetPoint("LEFT", frame, "LEFT", BAR_PAD, 0)

local sep1 = MakeText("LEFT")
sep1:SetText(SEP_TEXT)
sep1:SetPoint("LEFT", scoreCol, "RIGHT", SEP_PAD, 0)

local kdaIcon = MakeText("LEFT")
kdaIcon:SetText(ICON_KDA)
kdaIcon:SetPoint("LEFT", sep1, "RIGHT", SEP_PAD, 0)

local kdaNum = MakeText("CENTER")
kdaNum:SetPoint("LEFT", kdaIcon, "RIGHT", ICON_GAP, 0)

local sep2 = MakeText("LEFT")
sep2:SetText(SEP_TEXT)
sep2:SetPoint("LEFT", kdaNum, "RIGHT", SEP_PAD, 0)

local csIcon = MakeText("LEFT")
csIcon:SetText(ICON_CS)
csIcon:SetPoint("LEFT", sep2, "RIGHT", SEP_PAD, 0)

local csNum = MakeText("CENTER")
csNum:SetPoint("LEFT", csIcon, "RIGHT", ICON_GAP, 0)

-- Gold gets no icon of its own: FormatMoney already carries the coin artwork
-- inline, so a leading icon would just say "money" twice.
local sep3 = MakeText("LEFT")
sep3:SetText(SEP_TEXT)
sep3:SetPoint("LEFT", csNum, "RIGHT", SEP_PAD, 0)

local goldNum = MakeText("CENTER")
goldNum:SetPoint("LEFT", sep3, "RIGHT", SEP_PAD, 0)

-- Clock digit box, anchored to the FRAME's right edge -- never to a column, whose box
-- would drag it around. Fixed measured width; LEFT justify parks the spare character
-- at the border, where it reads as padding rather than as a gap after CS.
local clockDigits = frame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
clockDigits:SetJustifyH("LEFT")
clockDigits:SetPoint("RIGHT", frame, "RIGHT", -BAR_PAD, CLOCK_Y)
clockDigits:SetFont(CLOCK_FONT, CLOCK_SIZE, "OUTLINE")

local running   = false
local baseTime  = 0
local frozen    = nil       -- final elapsed seconds, held after the match ends
local throttle  = 0
local sb = { ally = 0, enemy = 0, k = 0, d = 0, a = 0, cs = 0, gold = 0 }

-- Widest digit at the LIVE scale in fs's own font. Never assume a font's digits are
-- tabular, and never hardcode 8: rasterisation rounds each glyph to whole pixels
-- independently, so which digit is widest changes with the effective scale (windowed
-- mode made it 5 here). Leaves sample text behind; every caller re-renders after.
local function WidestDigit(fs)
    local widest, widestW = "0", 0
    for i = 0, 9 do
        fs:SetText(tostring(i))
        local w = fs:GetStringWidth()
        if w > widestW then widest, widestW = tostring(i), w end
    end
    return widest, widestW
end

-- Measured width of the worst-case MM:SS; locks the digit box once known. A box sized
-- under the rendered string does not clip, it WRAPS to a second line and spills out of
-- the backdrop -- hence measuring the true widest digit, plus slack.
local digitReserve = 0
local function EnsureDigitReserve()
    if digitReserve > 0 then return end
    local d, dw = WidestDigit(clockDigits)
    if dw <= 0 then return end
    clockDigits:SetText(d .. d .. ":" .. d .. d)
    local w = clockDigits:GetStringWidth()
    if w <= 0 then return end
    digitReserve = w + COL_SLACK
    clockDigits:SetWidth(digitReserve)
end

-- Sizes every column and the frame, once. Reserves are two digits for score and KDA,
-- three for CS, and two gold / two silver / two copper for the wallet; past that a
-- column wraps rather than overruns, so raise the sample if a match ever gets there.
-- Leaves sample text in the columns if it succeeds and bails mid-way if the widgets
-- are not laid out yet, so callers must re-render immediately.
local reserved = false
local function EnsureReserves()
    if reserved then return end
    EnsureDigitReserve()
    if digitReserve <= 0 then return end

    local d, dw = WidestDigit(scoreCol)
    if dw <= 0 then return end
    local dd = d .. d

    local function Measure(fs, text)
        fs:SetText(text)
        return fs:GetStringWidth()
    end

    -- Gold is measured as a real coin string, not from digits: at worst case it
    -- carries three inline icons whose width no digit sample would account for.
    -- Composed from the widest digit for the same reason every other column is,
    -- with a fallback because a sample of all zeroes would collapse to "0c".
    local gd = (d ~= "0") and d or "9"
    local goldSample = FormatMoney(tonumber(gd .. gd .. gd .. gd .. gd .. gd))

    local scoreW   = Measure(scoreCol, dd .. " vs " .. dd)
    local kdaNumW  = Measure(kdaNum,   dd .. "/" .. dd .. "/" .. dd)
    local csNumW   = Measure(csNum,    d .. dd)
    local goldW    = Measure(goldNum,  goldSample)
    local sepW     = sep1:GetStringWidth()
    local kdaIconW = kdaIcon:GetStringWidth()
    local csIconW  = csIcon:GetStringWidth()
    if scoreW <= 0 or kdaNumW <= 0 or csNumW <= 0 or goldW <= 0
       or sepW <= 0 or kdaIconW <= 0 or csIconW <= 0 then return end

    scoreW, kdaNumW = scoreW + COL_SLACK, kdaNumW + COL_SLACK
    csNumW, goldW   = csNumW + COL_SLACK, goldW + COL_SLACK
    scoreCol:SetWidth(scoreW)
    kdaNum:SetWidth(kdaNumW)
    csNum:SetWidth(csNumW)
    goldNum:SetWidth(goldW)

    frame:SetWidth(BAR_PAD + scoreW
                   + SEP_PAD + sepW + SEP_PAD + kdaIconW + ICON_GAP + kdaNumW
                   + SEP_PAD + sepW + SEP_PAD + csIconW  + ICON_GAP + csNumW
                   + SEP_PAD + sepW + SEP_PAD + goldW
                   + CLOCK_GAP + digitReserve + BAR_PAD)
    reserved = true
end

local function FormatTime(sec)
    sec = math.floor(sec + 0.5)
    if sec < 0 then sec = 0 end
    -- Minutes are uncapped rather than rolling into H:MM:SS -- MOBA convention (72:35,
    -- not 1:12:35), and it keeps the string at five characters, which is what the
    -- measured digit box is sized for. Past 99:59 it grows a sixth and would wrap.
    return string.format("%d:%02d", math.floor(sec / 60), sec % 60)
end

-- Static segments; call when score/KDA/CS/gold change. Never touches width.
local function RenderStatic()
    EnsureReserves()
    scoreCol:SetText(C_ALLY .. sb.ally .. C_END .. " " .. C_DIM .. "vs" .. C_END .. " " .. C_ENEMY .. sb.enemy .. C_END)
    kdaNum:SetText(sb.k .. "/" .. sb.d .. "/" .. sb.a)
    csNum:SetText(tostring(sb.cs))
    goldNum:SetText(FormatMoney(sb.gold))
end

-- Clock digits only; called every tick. Never touches width.
local function RenderClock()
    EnsureReserves()
    local elapsed = running and (GetTime() - baseTime) or frozen or 0
    clockDigits:SetText(C_DIM .. FormatTime(elapsed) .. C_END)
end

-- GetStringWidth rasterizes at the CURRENT effective scale, so a resolution or
-- UI-scale change invalidates every measured width. All of them are latched on first
-- measure and would stay stale until /reload. Fixed-size widgets (revive, kill feed)
-- never measure and need nothing here.
local function RelayoutForScale()
    reserved, digitReserve = false, 0
    RenderStatic()      -- re-measures at the new scale
    RenderClock()       -- ...and clears the sample text both may have left
end

frame:SetScript("OnUpdate", function(self, elapsed)
    if not frame:IsShown() then return end
    throttle = throttle + elapsed
    if throttle < 0.2 then return end
    throttle = 0
    RenderClock()
end)

frame:SetScript("OnDragStart", function(self) if self:IsMovable() then self:StartMoving() end end)
frame:SetScript("OnDragStop", function(self)
    self:StopMovingOrSizing()
    local point, _, relPoint, x, y = self:GetPoint()
    local db = MobaHUDDB.bar
    db.point, db.relPoint, db.x, db.y = point, relPoint, x, y
end)

local function ApplyPosition()
    local db = MobaHUDDB.bar
    frame:ClearAllPoints()
    frame:SetPoint(db.point, UIParent, db.relPoint, db.x, db.y)
end

local function ApplyLock()
    frame:EnableMouse(not MobaHUDDB.bar.locked)
end

local function ShowBar()
    frame:Show()
    RenderStatic()
    RenderClock()
end

local function StartClock(elapsedSec)
    baseTime = GetTime() - elapsedSec
    running  = true
    frozen   = nil
    ShowBar()
end

-- Match over, player still in the battleground: hold the final time and score on
-- screen. The server's E payload cannot distinguish "match ended" from "you left",
-- so hiding is driven by leaving the instance (MobaHUD.lua), never by E.
local function FreezeClock()
    if running then frozen = GetTime() - baseTime end
    running = false
    RenderClock()
end

-- Bar only. Clearing the revive countdown and kill feed is ns.Feed.Clear's job;
-- the orchestrator calls both.
local function StopBar()
    running = false
    frozen  = nil
    frame:Hide()
end

-- The wallet is mirrored into ns.gold rather than kept private to `sb`, because the
-- shop reads it too. The return value says whether it moved, so the orchestrator
-- can skip a shop redraw on the 10s resync.
local function HandleScoreboard(payload)
    local ally, enemy, k, d, a, cs, gold =
        string.match(payload, "^(%d+),(%d+),(%d+),(%d+),(%d+),(%d+),(%d+)$")
    if not ally then return false end
    sb.ally = tonumber(ally); sb.enemy = tonumber(enemy)
    sb.k    = tonumber(k);    sb.d     = tonumber(d)
    sb.a    = tonumber(a);    sb.cs    = tonumber(cs)
    sb.gold = tonumber(gold)
    local moved = ns.SetGold(sb.gold)
    ShowBar()
    return moved
end

ns.Bar = {
    StartClock       = StartClock,
    Freeze           = FreezeClock,
    Stop             = StopBar,
    Scoreboard       = HandleScoreboard,
    FormatTime       = FormatTime,
    RelayoutForScale = RelayoutForScale,

    -- ns.InitDB has already created MobaHUDDB.bar; this only fills the gaps.
    InitSavedVars = function()
        for k, v in pairs(DEFAULTS) do
            if MobaHUDDB.bar[k] == nil then MobaHUDDB.bar[k] = v end
        end
        ApplyPosition()
        ApplyLock()
    end,

    SetLocked = function(locked)
        MobaHUDDB.bar.locked = locked
        ApplyLock()
    end,

    ResetPosition = function()
        local db = MobaHUDDB.bar
        db.point, db.relPoint = DEFAULTS.point, DEFAULTS.relPoint
        db.x, db.y = DEFAULTS.x, DEFAULTS.y
        ApplyPosition()
    end,
}
