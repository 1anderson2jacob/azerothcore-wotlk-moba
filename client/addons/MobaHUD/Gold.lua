-- Gold: the floating "+<amount>" that announces earned gold. Two renderers: corpse loot
-- floats above your own character, every other source floats off the bar's gold column
-- with a source icon.
--
-- Nothing here survives a /reload or a match end, because the server never re-sends G:.

local ADDON_NAME, ns = ...

-- Payload `source` field; mirrors BG_MOBA_GoldSource in BattlegroundMOBA.h.
-- MOBA_GOLD_SILENT (0) never arrives -- the server drops the payload instead.
local SRC_CORPSE    = 1
local SRC_KILL      = 2
local SRC_STRUCTURE = 3
local SRC_OBJECTIVE = 4

-- Icons are swappable placeholders, as in Feed.lua: the table holds the texture NAME
-- only, so the size lives in one constant. CORPSE is absent on purpose -- creep gold is
-- the most frequent event in a match, and an icon on every one of them is noise.
-- OBJECTIVE borrows the boss icon because the boss is the only team_gold source there
-- is; a second one needs its own.
local SOURCE_ICON = {
    [SRC_KILL]      = "INV_Sword_04",
    [SRC_STRUCTURE] = "INV_Misc_Bomb_04",
    [SRC_OBJECTIVE] = ns.ICON_BOSS,
}

local FLOAT_FONT   = 16
local FLOAT_ICON   = 16
local COIN_SIZE    = 12
local COIN_Y       = -1

local FLOAT_TTL    = 1.5   -- seconds on screen
local FLOAT_TRAVEL = 30    -- px travelled over that lifetime
local FADE_AT      = 0.6   -- fraction of the lifetime before the fade starts

local BAR_SLOTS    = 4     -- concurrent bar floats before the oldest is recycled
local BAR_PITCH    = 20    -- px between stacked bar floats
local BAR_GAP      = 12    -- px between the bar's edge and the first float
local COALESCE     = 0.3   -- seconds within which same-source bar floats merge

local C_GOLD = "|cffffd200"

local FormatMoney = ns.MoneyFormatterCompact(COIN_SIZE, COIN_Y)

local function Icon(name)
    return string.format("|TInterface\\Icons\\%s:%d:%d|t", name, FLOAT_ICON, FLOAT_ICON)
end

local function Amount(copper)
    return C_GOLD .. "+" .. FormatMoney(copper) .. ns.C_END
end

local function BarText(copper, source)
    local icon = SOURCE_ICON[source]
    return (icon and (Icon(icon) .. " ") or "") .. Amount(copper)
end

-- A 3.3.5 addon has no world-to-screen call, so corpse gold cannot be drawn on the
-- corpse it came from. It floats here instead: screen centre, a little high, which is
-- roughly where your own model sits. Doubles as the bar float's anchor when the bar has
-- not been laid out yet.
local function PlayerPoint()
    return UIParent:GetWidth() / 2, UIParent:GetHeight() * 0.55
end

-- ---- the float layer -----------------------------------------------------
-- One frame drives every live float. It hides itself when the last one retires, which
-- is what keeps the OnUpdate off entirely between kills.
local layer = CreateFrame("Frame", "MobaHUDGoldFloats", UIParent)
layer:SetAllPoints(UIParent)
layer:SetFrameStrata("HIGH")

local pool     = {}   -- retired FontStrings, reused
local active   = {}   -- { fs, x, y, dir, slot, source, copper, born }
local barSlots = {}   -- slot index -> taken

local function Acquire()
    local fs = table.remove(pool)
    if not fs then
        fs = layer:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
        fs:SetFont("Fonts\\FRIZQT__.TTF", FLOAT_FONT, "OUTLINE")
    end
    fs:Show()
    return fs
end

local function Release(f)
    if f.slot then barSlots[f.slot] = nil end
    f.fs:Hide()
    f.fs:ClearAllPoints()
    table.insert(pool, f.fs)
end

local function Place(f, now)
    local t = (now - f.born) / FLOAT_TTL
    if t > 1 then t = 1 end
    f.fs:ClearAllPoints()
    f.fs:SetPoint("CENTER", layer, "BOTTOMLEFT", f.x, f.y + f.dir * FLOAT_TRAVEL * t)
    f.fs:SetAlpha(t < FADE_AT and 1 or (1 - t) / (1 - FADE_AT))
end

local function Spawn(text, x, y, dir, slot, source, copper)
    local f = {
        fs = Acquire(), x = x, y = y, dir = dir, slot = slot,
        source = source, copper = copper, born = GetTime(),
    }
    f.fs:SetText(text)
    table.insert(active, f)
    Place(f, f.born)   -- ahead of the first OnUpdate, or it draws one frame at 0,0
    layer:Show()
end

layer:SetScript("OnUpdate", function(self)
    if #active == 0 then self:Hide(); return end
    local now = GetTime()
    -- Backwards, so removing one cannot skip over the next.
    for i = #active, 1, -1 do
        local f = active[i]
        if now - f.born >= FLOAT_TTL then
            Release(f)
            table.remove(active, i)
        else
            Place(f, now)
        end
    end
end)
layer:Hide()

-- ---- bar floats ----------------------------------------------------------
local function TakeSlot()
    for i = 1, BAR_SLOTS do
        if not barSlots[i] then barSlots[i] = true; return i end
    end
    -- Every slot busy: the oldest gives up its place, rather than the newest number
    -- being the one that goes unshown.
    local oldest
    for i = 1, #active do
        local f = active[i]
        if f.slot and (not oldest or f.born < active[oldest].born) then oldest = i end
    end
    local slot = active[oldest].slot
    Release(table.remove(active, oldest))
    barSlots[slot] = true
    return slot
end

local function BarFloat(copper, source)
    local now = GetTime()
    -- The last hitter on a structure takes TWO grants -- the team payout and the
    -- last-hit bonus -- for one turret. Merging inside the window is what makes those
    -- one number instead of two stacked ones.
    for i = 1, #active do
        local f = active[i]
        if f.slot and f.source == source and now - f.born <= COALESCE then
            f.copper = f.copper + copper
            f.fs:SetText(BarText(f.copper, source))
            return
        end
    end

    local x, y, halfH = ns.Bar.GoldAnchor()
    if not x then
        x, y = PlayerPoint()
        halfH = 0
    end
    -- The bar is usually parked at the top of the screen, where a rising number leaves
    -- immediately. Travel away from whichever edge it sits against -- and start clear of
    -- the backdrop on that same side, so the first float never draws over the bar.
    local dir  = (y > UIParent:GetHeight() / 2) and -1 or 1
    local base = y + dir * (halfH + BAR_GAP)
    local slot = TakeSlot()
    Spawn(BarText(copper, source), x, base + dir * (slot - 1) * BAR_PITCH, dir, slot, source, copper)
end

-- Corpse loot: rises off your own character, carries no icon, and never coalesces.
local function CorpseFloat(copper)
    local x, y = PlayerPoint()
    Spawn(Amount(copper), x, y, 1, nil, SRC_CORPSE, copper)
end

-- ---- payload handler -----------------------------------------------------
local function HandleGrant(payload)
    -- The trailing name group is [^,]* , not + : every non-corpse source ends the
    -- payload on an empty field. A + fails the anchored match and the float vanishes
    -- with nothing logged -- the trap K:, O:, X: and B: each sprang for real. Nothing
    -- reads the name; the pattern still has to allow for it.
    local copper, source = string.match(payload, "^(%d+),(%d+),[^,]*$")
    if not copper then return end
    copper = tonumber(copper)
    source = tonumber(source)
    if copper <= 0 then return end

    if source == SRC_CORPSE then
        CorpseFloat(copper)
    elseif source == SRC_KILL or source == SRC_STRUCTURE or source == SRC_OBJECTIVE then
        BarFloat(copper, source)
    end
end

ns.Gold = {
    Grant = HandleGrant,
}
