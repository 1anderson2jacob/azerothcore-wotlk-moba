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

local ADDON_NAME  = "MobaHUD"
local PREFIX      = "MobaHUD"
local SHOP_PREFIX = "MobaShop"
local HIDE_MSG    = "E"

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

-- ---- shop panel ----------------------------------------------------------
-- Sidebar filters | icon grid | detail pane. All geometry is fixed UI units and
-- nothing measures text, so a mid-session resolution change needs no relayout
-- (contrast RelayoutForScale, which exists only for the content-sized HUD bar).
--
-- Everything drawn here comes from Catalog.lua; the server is contacted only to
-- buy. Quality is uniform per tab so it is not a filter -- the tab is the filter.

local SHOP_W,  SHOP_H   = 800, 520
local SIDE_W,  DETAIL_W = 140, 200
local CARD_W,  CARD_H   = 64, 78
local PITCH_X, PITCH_Y  = 70, 84
local GRID_COLS, GRID_ROWS = 6, 5
local MAX_PIECES = 9            -- largest bundle in the catalog
local ROW_H      = 13           -- sidebar filter row pitch
local CONTENT_Y  = -66          -- first row below the tab strip (tabs end at -54)

local QUALITY_COLOR = {
    [0] = "|cff9d9d9d", [1] = "|cffffffff", [2] = "|cff1eff00",
    [3] = "|cff0070dd", [4] = "|cffa335ee", [5] = "|cffff8000",
}

-- InventoryType -> slot name, for the lower filter under armour categories.
local SLOT_NAME = {
    [1] = "Head", [2] = "Neck", [3] = "Shoulder", [5] = "Chest", [6] = "Waist",
    [7] = "Legs", [8] = "Feet", [9] = "Wrist", [10] = "Hands", [11] = "Finger",
    [13] = "One-Hand", [14] = "Off Hand", [15] = "Ranged", [16] = "Back",
    [17] = "Two-Hand", [20] = "Chest", [21] = "Main Hand", [26] = "Ranged",
}

-- Weapon subclass -> name, for the lower filter under Weapons. Only the ids the
-- catalog actually uses are listed.
local WEAPON_NAME = {
    [0] = "Axe (1H)", [1] = "Axe (2H)", [2] = "Bow", [3] = "Gun",
    [4] = "Mace (1H)", [5] = "Mace (2H)", [6] = "Polearm", [7] = "Sword (1H)",
    [8] = "Sword (2H)", [10] = "Staff", [13] = "Fist", [15] = "Dagger",
    [18] = "Crossbow", [19] = "Wand",
}

local WEAPON_CATEGORY = "Weapons"

local shop = CreateFrame("Frame", "MobaShopFrame", UIParent)
shop:SetWidth(SHOP_W)
shop:SetHeight(SHOP_H)
shop:SetPoint("CENTER", UIParent, "CENTER", 0, 0)
shop:SetBackdrop({
    bgFile   = "Interface\\DialogFrame\\UI-DialogBox-Background",
    edgeFile = "Interface\\DialogFrame\\UI-DialogBox-Border",
    tile = true, tileSize = 32, edgeSize = 32,
    insets = { left = 11, right = 12, top = 12, bottom = 11 },
})
shop:SetToplevel(true)
shop:EnableMouse(true)
shop:Hide()
tinsert(UISpecialFrames, "MobaShopFrame")   -- Esc closes it

local shopGold = shop:CreateFontString(nil, "OVERLAY", "GameFontNormal")
shopGold:SetPoint("TOP", shop, "TOP", 0, -18)

local shopClose = CreateFrame("Button", nil, shop, "UIPanelCloseButton")
shopClose:SetPoint("TOPRIGHT", shop, "TOPRIGHT", -6, -6)

local shopStatus = shop:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
shopStatus:SetPoint("BOTTOMLEFT", shop, "BOTTOMLEFT", SIDE_W + 4, 16)

-- ---- state ---------------------------------------------------------------
local fCategory, fSub, fSearch = nil, nil, ""
local selected, scrollOffset = nil, 0
local filtered = {}
local tabButtons, catButtons, subButtons, cards, pieceRows = {}, {}, {}, {}, {}
local suffixFactor = {}   -- entry -> RandPropPoints factor, pushed by the server
local RenderShop          -- forward declaration; the widgets below call it

-- ---- catalog helpers -----------------------------------------------------
local function Cat()      return MobaShopCatalog and MobaShopCatalog[shop.mapId] end
local function ItemMeta(entry)
    local c = Cat()
    return c and c.items[entry]
end

local function ShopTab()
    local c = Cat()
    if not c then return nil end
    for _, tab in ipairs(c.tabs) do
        if tab.vendorId == shop.tabId then return tab end
    end
    return nil
end

local function LeafCategory(leaf)
    return leaf.path and leaf.path[1] or "Other"
end

local function LeafName(leaf)
    local m = leaf.pieces[1] and ItemMeta(leaf.pieces[1].entry)
    return m and m.name or "?"
end

-- Cards lead with the suffix: inside one category every starting-gear leaf grants
-- the same base items, so the item name alone distinguishes nothing.
local function LeafLabel(leaf)
    if #leaf.pieces > 1 then return leaf.label end
    return leaf.label ~= "" and leaf.label or LeafName(leaf)
end

local function LeafIcon(leaf)
    local entry = leaf.pieces[1] and leaf.pieces[1].entry
    if not entry then return "Interface\\Icons\\INV_Misc_QuestionMark" end
    -- GetItemIcon reads client DBCs and needs no item cache; GetItemInfo's texture
    -- does need one, so it is only the fallback.
    return (GetItemIcon and GetItemIcon(entry))
        or select(10, GetItemInfo(entry))
        or "Interface\\Icons\\INV_Misc_QuestionMark"
end

local function PieceLink(piece)
    local lvl = UnitLevel("player") or 80
    local s = piece.suffix or 0
    if s == 0 then
        return string.format("item:%d:0:0:0:0:0:0:0:%d", piece.entry, lvl)
    end
    -- Field 8 is the suffix factor and field 9 the player level. Both are
    -- required: omitting the level leaves the client unable to resolve the
    -- suffix, and every stat renders as +0.
    return string.format("item:%d:0:0:0:0:0:%d:%d:%d",
        piece.entry, -s, suffixFactor[piece.entry] or 0, lvl)
end

local function MatchesSearch(leaf)
    if fSearch == "" then return true end
    local needle = string.lower(fSearch)
    if string.find(string.lower(leaf.label or ""), needle, 1, true) then return true end
    for _, p in ipairs(leaf.pieces) do
        local m = ItemMeta(p.entry)
        if m and string.find(string.lower(m.name), needle, 1, true) then return true end
    end
    return false
end

local function Matches(leaf)
    if not MatchesSearch(leaf) then return false end
    if fCategory and LeafCategory(leaf) ~= fCategory then return false end
    if fSub then
        if fCategory == WEAPON_CATEGORY then
            local m = leaf.pieces[1] and ItemMeta(leaf.pieces[1].entry)
            if not m or WEAPON_NAME[m.subclass] ~= fSub then return false end
        else
            local hit = false
            for _, p in ipairs(leaf.pieces) do
                local m = ItemMeta(p.entry)
                if m and SLOT_NAME[m.slot] == fSub then hit = true; break end
            end
            if not hit then return false end
        end
    end
    return true
end

-- ---- sidebar -------------------------------------------------------------
local searchBox = CreateFrame("EditBox", nil, shop, "InputBoxTemplate")
searchBox:SetWidth(SIDE_W - 40)
searchBox:SetHeight(18)
searchBox:SetPoint("TOPLEFT", shop, "TOPLEFT", 30, CONTENT_Y)
searchBox:SetAutoFocus(false)
searchBox:SetScript("OnTextChanged", function(self)
    fSearch = self:GetText() or ""
    scrollOffset = 0
    RenderShop()
end)
searchBox:SetScript("OnEscapePressed", function(self) self:ClearFocus() end)

local function FilterButton(store, parentAnchorY, index)
    local b = CreateFrame("Button", nil, shop)
    b:SetWidth(SIDE_W - 24)
    b:SetHeight(14)
    b:SetPoint("TOPLEFT", shop, "TOPLEFT", 20, parentAnchorY - (index - 1) * ROW_H)
    b:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
    local t = b:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    t:SetPoint("LEFT", b, "LEFT", 2, 0)
    t:SetJustifyH("LEFT")
    b.text = t
    b:Hide()
    store[index] = b
    return b
end

local CAT_TOP, SUB_TOP = -110, -266
local catHeader = shop:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
catHeader:SetPoint("TOPLEFT", shop, "TOPLEFT", 20, CAT_TOP + 16)
catHeader:SetText("|cffffd100CATEGORY|r")
local subHeader = shop:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
subHeader:SetPoint("TOPLEFT", shop, "TOPLEFT", 20, SUB_TOP + 16)

for i = 1, 10 do FilterButton(catButtons, CAT_TOP, i) end
for i = 1, 18 do FilterButton(subButtons, SUB_TOP, i) end

-- ---- grid ----------------------------------------------------------------
local GRID_X = SIDE_W + 4
local GRID_Y = CONTENT_Y

local scroll = CreateFrame("ScrollFrame", "MobaShopScroll", shop, "FauxScrollFrameTemplate")
scroll:SetWidth(GRID_COLS * PITCH_X)
scroll:SetHeight(GRID_ROWS * PITCH_Y)
scroll:SetPoint("TOPLEFT", shop, "TOPLEFT", GRID_X, GRID_Y)
scroll:SetScript("OnVerticalScroll", function(self, offset)
    FauxScrollFrame_OnVerticalScroll(self, offset, PITCH_Y, function()
        scrollOffset = FauxScrollFrame_GetOffset(self)
        RenderShop()
    end)
end)

for i = 1, GRID_COLS * GRID_ROWS do
    local col, row = (i - 1) % GRID_COLS, math.floor((i - 1) / GRID_COLS)
    local b = CreateFrame("Button", nil, shop)
    b:SetWidth(CARD_W)
    b:SetHeight(CARD_H)
    b:SetPoint("TOPLEFT", shop, "TOPLEFT", GRID_X + col * PITCH_X, GRID_Y - row * PITCH_Y)
    b:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")

    local icon = b:CreateTexture(nil, "ARTWORK")
    icon:SetWidth(32)
    icon:SetHeight(32)
    icon:SetPoint("TOP", b, "TOP", 0, -4)
    b.icon = icon

    local sel = b:CreateTexture(nil, "OVERLAY")
    sel:SetTexture("Interface\\Buttons\\CheckButtonHilight")
    sel:SetBlendMode("ADD")
    sel:SetAllPoints(icon)
    sel:Hide()
    b.sel = sel

    local label = b:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    label:SetPoint("TOP", icon, "BOTTOM", 0, -2)
    label:SetWidth(CARD_W)
    label:SetHeight(24)
    label:SetJustifyH("CENTER")
    b.label = label

    local price = b:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    price:SetPoint("BOTTOM", b, "BOTTOM", 0, 2)
    b.price = price

    b:SetScript("OnClick", function(self)
        if self.leaf then
            selected = self.leaf
            RenderShop()
        end
    end)
    b:SetScript("OnEnter", function(self)
        if not self.leaf then return end
        GameTooltip:SetOwner(self, "ANCHOR_RIGHT")
        if #self.leaf.pieces > 1 then
            GameTooltip:SetText(self.leaf.label)
            GameTooltip:AddLine(string.format("%d-piece set -- click to list", #self.leaf.pieces), 1, 1, 1)
            GameTooltip:Show()
        else
            GameTooltip:SetHyperlink(PieceLink(self.leaf.pieces[1]))
        end
    end)
    b:SetScript("OnLeave", function() GameTooltip:Hide() end)
    b:Hide()
    cards[i] = b
end

-- ---- detail pane ---------------------------------------------------------
local DETAIL_X = SHOP_W - DETAIL_W - 12

-- The title doubles as the hover target for a single item, so its name is not
-- repeated as a piece row beneath it. Fixed height: dSub anchors to its bottom,
-- so a two-line title must not push into it.
local dTitleBtn = CreateFrame("Button", nil, shop)
dTitleBtn:SetPoint("TOPLEFT", shop, "TOPLEFT", DETAIL_X, CONTENT_Y)
dTitleBtn:SetWidth(DETAIL_W - 8)
dTitleBtn:SetHeight(36)
local dTitle = dTitleBtn:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
dTitle:SetAllPoints(dTitleBtn)
dTitle:SetJustifyH("LEFT")
dTitle:SetJustifyV("TOP")
dTitleBtn:SetScript("OnEnter", function(self)
    if not self.piece then return end
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:SetHyperlink(PieceLink(self.piece))
end)
dTitleBtn:SetScript("OnLeave", function() GameTooltip:Hide() end)

local dSub = shop:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
dSub:SetPoint("TOPLEFT", dTitleBtn, "BOTTOMLEFT", 0, -2)
dSub:SetWidth(DETAIL_W - 8)
dSub:SetHeight(26)
dSub:SetJustifyH("LEFT")
dSub:SetJustifyV("TOP")
-- A real tooltip embedded in the pane, so a single item shows its full stats
-- without needing to be hovered. Bundles keep the piece list instead.
-- Hidden scanner. SetHyperlink populates its font strings and we copy them into
-- the pane: showing the tooltip frame itself sizes to its own content and spills
-- outside the panel, which is what made it look like a floating modal.
local scan = CreateFrame("GameTooltip", "MobaShopScan", nil, "GameTooltipTemplate")
scan:SetOwner(UIParent, "ANCHOR_NONE")

-- Usability cache. Proficiency is not exposed to Lua, but the client paints
-- requirements you fail in red -- scanning for that is the only way to see it.
-- Keyed by entry: it cannot change during a match.
local usable = {}
local function PieceUsable(piece)
    local cached = usable[piece.entry]
    if cached ~= nil then return cached end
    scan:ClearLines()
    scan:SetOwner(UIParent, "ANCHOR_NONE")
    scan:SetHyperlink(PieceLink(piece))

    local ok = true
    for i = 1, scan:NumLines() do
        -- Both columns. A failed armour-class or weapon-proficiency requirement
        -- reddens the type name ("Plate", "Sword") in the RIGHT column; only
        -- class and level requirements appear red on the left.
        for _, side in ipairs({ "Left", "Right" }) do
            local fs = _G["MobaShopScanText" .. side .. i]
            local txt = fs and fs:GetText()
            if txt and txt ~= "" then
                local r, g, b = fs:GetTextColor()
                if r and r > 0.9 and g < 0.2 and b < 0.2 then ok = false end
            end
        end
        if not ok then break end
    end

    usable[piece.entry] = ok
    return ok
end

-- A bundle is armour-class homogeneous, so one unusable piece condemns the set.
local function LeafUsable(leaf)
    for _, p in ipairs(leaf.pieces) do
        if not PieceUsable(p) then return false end
    end
    return true
end

local INFO_LINES = 16
local infoRows = {}
for i = 1, INFO_LINES do
    local fs = shop:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    fs:SetWidth(DETAIL_W - 8)
    fs:SetJustifyH("LEFT")
    -- Chained anchors so a wrapped line pushes the rest down instead of
    -- overlapping them. Wrapping is driven by UI-unit width, so this stays
    -- stable across a resolution change.
    if i == 1 then
        fs:SetPoint("TOPLEFT", dSub, "BOTTOMLEFT", 0, -8)
    else
        fs:SetPoint("TOPLEFT", infoRows[i - 1], "BOTTOMLEFT", 0, -1)
    end
    fs:Hide()
    infoRows[i] = fs
end

local function HexColor(r, g, b)
    return string.format("|cff%02x%02x%02x",
        math.floor((r or 1) * 255), math.floor((g or 1) * 255), math.floor((b or 1) * 255))
end

local function HideInfo()
    for _, fs in ipairs(infoRows) do fs:SetText(""); fs:Hide() end
end

-- Copies an item's own tooltip into the pane. Line 1 is the item name, which
-- dTitle already shows, so it is skipped.
local function RenderInfo(link)
    HideInfo()
    scan:ClearLines()
    scan:SetOwner(UIParent, "ANCHOR_NONE")
    scan:SetHyperlink(link)

    local row = 0
    for i = 2, scan:NumLines() do
        local l = _G["MobaShopScanTextLeft" .. i]
        local r = _G["MobaShopScanTextRight" .. i]
        local lt = l and l:GetText()
        if lt and lt ~= "" then
            row = row + 1
            if row > INFO_LINES then break end
            local text = HexColor(l:GetTextColor()) .. lt .. "|r"
            local rt = r and r:GetText()
            if rt and rt ~= "" then
                text = text .. "   " .. HexColor(r:GetTextColor()) .. rt .. "|r"
            end
            infoRows[row]:SetText(text)
            infoRows[row]:Show()
        end
    end
end

local function Separator(x, y, w, h)
    local t = shop:CreateTexture(nil, "ARTWORK")
    t:SetTexture("Interface\\Buttons\\WHITE8X8")
    t:SetVertexColor(1, 1, 1, 0.12)
    t:SetWidth(w)
    t:SetHeight(h)
    t:SetPoint("TOPLEFT", shop, "TOPLEFT", x, y)
    return t
end

Separator(16, -58, SHOP_W - 32, 1)                    -- under the tab strip
Separator(SIDE_W - 4, -62, 1, SHOP_H - 92)            -- sidebar | grid
Separator(DETAIL_X - 10, -62, 1, SHOP_H - 92)         -- grid | detail
Separator(20, CAT_TOP + 12, SIDE_W - 34, 1)           -- above CATEGORY
local subSep = Separator(20, SUB_TOP + 12, SIDE_W - 34, 1)   -- hidden with the filter

for i = 1, MAX_PIECES do
    local b = CreateFrame("Button", nil, shop)
    b:SetWidth(DETAIL_W - 8)
    b:SetHeight(18)
    b:SetPoint("TOPLEFT", dSub, "BOTTOMLEFT", 0, -4 - (i - 1) * 19)
    b:SetHighlightTexture("Interface\\QuestFrame\\UI-QuestTitleHighlight")
    local icon = b:CreateTexture(nil, "ARTWORK")
    icon:SetWidth(16)
    icon:SetHeight(16)
    icon:SetPoint("LEFT", b, "LEFT", 0, 0)
    b.icon = icon
    local t = b:CreateFontString(nil, "OVERLAY", "GameFontNormalSmall")
    t:SetPoint("LEFT", icon, "RIGHT", 4, 0)
    t:SetJustifyH("LEFT")
    b.text = t
    b:SetScript("OnEnter", function(self)
        if not self.piece then return end
        GameTooltip:SetOwner(self, "ANCHOR_LEFT")
        GameTooltip:SetHyperlink(PieceLink(self.piece))
    end)
    b:SetScript("OnLeave", function() GameTooltip:Hide() end)
    b:Hide()
    pieceRows[i] = b
end

local buyButton = CreateFrame("Button", nil, shop, "UIPanelButtonTemplate")
buyButton:SetWidth(DETAIL_W - 8)
buyButton:SetHeight(24)
buyButton:SetPoint("BOTTOMLEFT", shop, "BOTTOMLEFT", DETAIL_X, 14)
buyButton:SetText("Purchase")
buyButton:SetScript("OnClick", function()
    if not selected then return end
    shopStatus:SetText("...")
    SendAddonMessage(SHOP_PREFIX, "BUY:" .. shop.tabId .. "," .. selected.node, "BATTLEGROUND")
end)

-- ---- tabs ----------------------------------------------------------------
for i = 1, 6 do
    local b = CreateFrame("Button", nil, shop, "UIPanelButtonTemplate")
    b:SetHeight(20)
    b:SetPoint("TOPLEFT", shop, "TOPLEFT", 20 + (i - 1) * 116, -34)
    b:SetWidth(112)
    b:SetScript("OnClick", function(self)
        shop.tabId = self.vendorId
        fCategory, fSub, selected, scrollOffset = nil, nil, nil, 0
        RenderShop()
    end)
    b:Hide()
    tabButtons[i] = b
end

-- ---- render --------------------------------------------------------------
local function RenderTabs()
    local c = Cat()
    for i, b in ipairs(tabButtons) do
        local tab = c and c.tabs[i]
        if tab then
            b.vendorId = tab.vendorId
            b:SetText(tab.name)
            if tab.vendorId == shop.tabId then b:Disable() else b:Enable() end
            b:Show()
        else
            b:Hide()
        end
    end
end

local function RenderSidebar(tab)
    -- Counts answer "what do I get if I click this", so they honour search but
    -- not the category/slot selection they would replace.
    local cats, order, total = {}, {}, 0
    for _, leaf in ipairs(tab.leaves) do
        if MatchesSearch(leaf) then
            local k = LeafCategory(leaf)
            if not cats[k] then cats[k] = 0; table.insert(order, k) end
            cats[k] = cats[k] + 1
            total = total + 1
        end
    end

    local rows = { { label = "All", value = nil, count = total } }
    for _, k in ipairs(order) do
        table.insert(rows, { label = k, value = k, count = cats[k] })
    end

    for i, b in ipairs(catButtons) do
        local r = rows[i]
        if r then
            local mark = (fCategory == r.value) and "|cffffd100>|r " or "   "
            b.text:SetText(string.format("%s%s |cff808080%d|r", mark, r.label, r.count))
            b.value = r.value
            b:SetScript("OnClick", function(self)
                fCategory, fSub, selected, scrollOffset = self.value, nil, nil, 0
                RenderShop()
            end)
            b:Show()
        else
            b:Hide()
        end
    end

    -- Lower filter: weapon types under Weapons, equipment slots elsewhere. Slots
    -- cannot express Sword-vs-Axe, which is why this switches. Keyed by NAME, not
    -- id: InventoryType 5 and 20 are both Chest, 15 and 26 both Ranged, and two
    -- rows saying "Chest" is worse than useless.
    local isWeapons = (fCategory == WEAPON_CATEGORY)
    local count, rank, names, subTotal, hasBundle = {}, {}, {}, 0, false
    local function note(name, key)
        if not name then return end
        if not count[name] then
            count[name], rank[name] = 0, key
            table.insert(names, name)
        end
        count[name] = count[name] + 1
    end

    for _, leaf in ipairs(tab.leaves) do
        if MatchesSearch(leaf) and (not fCategory or LeafCategory(leaf) == fCategory) then
            subTotal = subTotal + 1
            if #leaf.pieces > 1 then hasBundle = true end
            if isWeapons then
                local m = leaf.pieces[1] and ItemMeta(leaf.pieces[1].entry)
                if m then note(WEAPON_NAME[m.subclass], m.subclass) end
            else
                local seen = {}
                for _, p in ipairs(leaf.pieces) do
                    local m = ItemMeta(p.entry)
                    local n = m and SLOT_NAME[m.slot]
                    -- Count each leaf once per slot NAME: a robe hitting both
                    -- InventoryType 5 and 20 must not count twice.
                    if n and not seen[n] then seen[n] = true; note(n, m.slot) end
                end
            end
        end
    end

    -- Show this filter only if it partitions anything. A bundle covers every slot
    -- at once, so a slot filter matches all of them and narrows nothing; the same
    -- goes for a category whose every item shares one slot, and for Consumables,
    -- which have no equipment slot at all.
    local narrows = false
    for _, n in ipairs(names) do
        if count[n] < subTotal then narrows = true; break end
    end
    local useful = narrows and (isWeapons or not hasBundle)

    if not useful then
        fSub = nil
        subHeader:Hide()
        subSep:Hide()
        for _, b in ipairs(subButtons) do b:Hide() end
        return
    end

    subHeader:SetText(isWeapons and "|cffffd100WEAPON TYPE|r" or "|cffffd100SLOT|r")
    subHeader:Show()
    subSep:Show()
    table.sort(names, function(a, b) return rank[a] < rank[b] end)

    local subRows = { { label = "All", value = nil, count = subTotal } }
    for _, n in ipairs(names) do
        table.insert(subRows, { label = n, value = n, count = count[n] })
    end

    for i, b in ipairs(subButtons) do
        local r = subRows[i]
        if r then
            local mark = (fSub == r.value) and "|cffffd100>|r " or "   "
            b.text:SetText(string.format("%s%s |cff808080%d|r", mark, r.label, r.count))
            b.value = r.value
            b:SetScript("OnClick", function(self)
                fSub, selected, scrollOffset = self.value, nil, 0
                RenderShop()
            end)
            b:Show()
        else
            b:Hide()
        end
    end
end

local function RenderGrid(tab)
    filtered = {}
    for _, leaf in ipairs(tab.leaves) do
        if Matches(leaf) then table.insert(filtered, leaf) end
    end

    local rows = math.ceil(#filtered / GRID_COLS)
    FauxScrollFrame_Update(scroll, rows, GRID_ROWS, PITCH_Y)
    scrollOffset = FauxScrollFrame_GetOffset(scroll)

    local money = GetMoney()
    for i, b in ipairs(cards) do
        local leaf = filtered[scrollOffset * GRID_COLS + i]
        if leaf then
            local meta   = ItemMeta(leaf.pieces[1].entry)
            local color  = QUALITY_COLOR[meta and meta.quality or 1]
            local canUse = LeafUsable(leaf)
            local afford = (leaf.cost == 0) or (money >= leaf.cost)

            b.icon:SetTexture(LeafIcon(leaf))
            -- Two distinct signals: red label = cannot use, red price = cannot afford.
            b.label:SetText((canUse and color or "|cffff3333") .. LeafLabel(leaf) .. "|r")
            if leaf.cost == 0 then
                b.price:SetText("|cff33ff99free|r")
            elseif afford then
                b.price:SetText(GetCoinTextureString(leaf.cost))
            else
                b.price:SetText("|cffff3333" .. GetCoinTextureString(leaf.cost) .. "|r")
            end
            b.icon:SetDesaturated(not canUse or not afford)

            if selected == leaf then b.sel:Show() else b.sel:Hide() end
            b.leaf = leaf
            b:Show()
        else
            b.leaf = nil
            b:Hide()
        end
    end

    shopStatus:SetText(string.format("|cff808080showing %d of %d|r", #filtered, #tab.leaves))
end

local function RenderDetail()
    if not selected then
        dTitle:SetText("")
        dTitleBtn.piece = nil
        dSub:SetText("|cff808080Select an item.|r")
        for _, r in ipairs(pieceRows) do r:Hide(); r.piece = nil end
        HideInfo()
        buyButton:Disable()
        return
    end

    local isBundle = #selected.pieces > 1
    local meta   = ItemMeta(selected.pieces[1].entry)
    local color  = QUALITY_COLOR[meta and meta.quality or 1]
    local cost   = selected.cost == 0 and "|cff33ff99Free|r" or GetCoinTextureString(selected.cost)
    local canUse = LeafUsable(selected)
    local afford = (selected.cost == 0) or (GetMoney() >= selected.cost)

    -- Usability first: an item you can never wear is not worth telling someone
    -- they also cannot afford.
    local warn = ""
    if not canUse then
        warn = "   |cffff3333Cannot use|r"
    elseif not afford then
        warn = "   |cffff3333Too expensive|r"
    end

    dTitle:SetText(color .. LeafLabel(selected) .. "|r")
    dTitleBtn.piece = nil   -- stats are inline now, so no hover-to-magnify

    if isBundle then
        dSub:SetText(string.format("|cffffffff%s set|r  %d pieces  %s%s",
            LeafName(selected), #selected.pieces, cost, warn))
        HideInfo()
        for i, r in ipairs(pieceRows) do
            local p = selected.pieces[i]
            if p then
                local m = ItemMeta(p.entry)
                r.icon:SetTexture((GetItemIcon and GetItemIcon(p.entry))
                    or select(10, GetItemInfo(p.entry))
                    or "Interface\\Icons\\INV_Misc_QuestionMark")
                r.text:SetText((QUALITY_COLOR[m and m.quality or 1]) .. (m and m.name or "?") .. "|r")
                r.piece = p
                r:Show()
            else
                r.piece = nil
                r:Hide()
            end
        end
    else
        dSub:SetText(cost .. warn)
        for _, r in ipairs(pieceRows) do r:Hide(); r.piece = nil end
        RenderInfo(PieceLink(selected.pieces[1]))
    end

    if canUse and afford then buyButton:Enable() else buyButton:Disable() end
end

RenderShop = function()
    local tab = ShopTab()
    RenderTabs()
    shopGold:SetText(GetCoinTextureString(GetMoney()))
    if not tab then
        shopStatus:SetText("|cffff3333no catalog for this map|r")
        return
    end
    RenderSidebar(tab)
    RenderGrid(tab)
    RenderDetail()
end

local function HandleShopPayload(payload)
    local mapId, tabId = string.match(payload, "^OPEN:(%d+),(%d+)$")
    if mapId then
        shop.mapId = tonumber(mapId)
        shop.tabId = tonumber(tabId)
        fCategory, fSub, fSearch, selected, scrollOffset = nil, nil, "", nil, 0
        searchBox:SetText("")
        shopStatus:SetText("")
        RenderShop()
        shop:Show()
        return
    end
    -- CLOSE may carry a reason. It goes to the chat frame, not shopStatus: that
    -- font string is parented to the panel this is about to hide.
    local reason = string.match(payload, "^CLOSE:?(.*)$")
    if reason then
        shop:Hide()
        if reason ~= "" then Print(reason) end
        return
    end
    local sf = string.match(payload, "^SF:(.+)$")
    if sf then
        for entry, factor in string.gmatch(sf, "(%d+):(%d+)") do
            suffixFactor[tonumber(entry)] = tonumber(factor)
        end
        return
    end
    local err = string.match(payload, "^ERR:(.+)$")
    if err then shopStatus:SetText("|cffff3333" .. err .. "|r"); return end
    if string.match(payload, "^OK:") then
        shopStatus:SetText("|cff33ff99Purchased.|r")
        RenderShop()   -- gold changed; refresh affordability
        return
    end
end

local function HandlePayload(payload)
    if payload == HIDE_MSG then StopBar(); shop:Hide(); return end
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

local function Dispatch(prefix, payload)
    if prefix == PREFIX then HandlePayload(payload)
    elseif prefix == SHOP_PREFIX then HandleShopPayload(payload) end
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
ev:SetScript("OnEvent", function(self, event, ...)
    if event == "CHAT_MSG_ADDON" then
        local prefix, message = ...
        OnAddonMessage(prefix, message)
    elseif event == "DISPLAY_SIZE_CHANGED" or event == "UI_SCALE_CHANGED" then
        RelayoutForScale()
    elseif event == "PLAYER_MONEY" then
        if shop:IsShown() then RenderShop() end
    elseif event == "PLAYER_ENTERING_WORLD" then
        -- One-shot "ready" ping; the server answers with current state. Only in a
        -- battleground (covers entering the BG, joining mid-match, and /reload).
        local _, instanceType = IsInInstance()
        if instanceType == "pvp" then
            SendAddonMessage(PREFIX, "REQ", "BATTLEGROUND")
            SendAddonMessage(SHOP_PREFIX, "HELLO", "BATTLEGROUND")
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
