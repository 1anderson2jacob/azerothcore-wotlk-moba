-- Core: the addon-wide namespace and anything shared by more than one UI module.
-- Loads first (see MobaHUD.toc); every other file opens with the same vararg line
-- to reach what is published here. Locals do not cross file boundaries in Lua, so
-- this table is the only channel between modules.

local ADDON_NAME, ns = ...

ns.PREFIX      = "MobaHUD"
ns.SHOP_PREFIX = "MobaShop"

ns.C_ALLY  = "|cff3399ff"  -- you (blue)
ns.C_ENEMY = "|cffff3333"  -- enemy (red)
ns.C_DIM   = "|cff999999"
ns.C_KILL  = "|cff33ff99"  -- "you slew" flavour (green)
ns.C_DEATH = "|cffff3333"  -- "you died" flavour (red)
ns.C_END   = "|r"
-- Shared because the boss's kill-feed lines and its gold float must not drift apart.
-- A swappable placeholder, like every other icon in the addon.
ns.ICON_BOSS = "INV_Misc_Head_Dragon_01"

-- The match wallet in copper, as last pushed by the server's S: payload. It lives
-- here rather than in Bar.lua because the shop header and its affordability
-- greying read the same number, and locals do not cross file boundaries.
--
-- SetGold returns true only when the value actually MOVED: the scoreboard also
-- arrives on a 10s resync that changes nothing, and the shop must not redraw
-- under the player's cursor for that.
ns.gold = 0

function ns.SetGold(copper)
    copper = tonumber(copper) or 0
    if ns.gold == copper then return false end
    ns.gold = copper
    return true
end

-- Renderer for wallet TOTALS. Prices keep Blizzard's GetCoinTextureString, which is
-- fine for a number read once; it is wrong for a number watched, twice over:
--
--   * it DROPS empty denominations ("25s", not "0g 25s 0c"), so a wallet changes
--     shape as it crosses 1g and the reader has to re-parse which coin is which;
--   * it hardcodes yOffset 0 and, given no height, draws coins at their NATIVE
--     size -- taller than most fonts' cap height, which stretches the FontString's
--     bounding box and drags the digits up with it.
--
-- Callers pass the metrics that suit their own font and get a closure back, so the
-- three icon escapes are built once rather than on every render. Keep `size` at or
-- below the font's point size or the box grows again.
local COIN_GOLD   = "Interface\\MoneyFrame\\UI-GoldIcon"
local COIN_SILVER = "Interface\\MoneyFrame\\UI-SilverIcon"
local COIN_COPPER = "Interface\\MoneyFrame\\UI-CopperIcon"

local function Coins(size, yOffset)
    local function coin(path)
        return string.format("|T%s:%d:%d:0:%d|t", path, size, size, yOffset)
    end
    return coin(COIN_GOLD), coin(COIN_SILVER), coin(COIN_COPPER)
end

function ns.MoneyFormatter(size, yOffset)
    local g, s, c = Coins(size, yOffset)

    return function(copper)
        copper = tonumber(copper) or 0
        return math.floor(copper / 10000) .. g .. " "
            .. math.floor((copper % 10000) / 100) .. s .. " "
            .. (copper % 100) .. c
    end
end

-- Same coin metrics for an amount read ONCE -- a gold float, not a watched total. It
-- drops the empty denominations the comment above rules out for the wallet: nothing
-- here is re-read a second later against a different shape.
function ns.MoneyFormatterCompact(size, yOffset)
    local g, s, c = Coins(size, yOffset)

    return function(copper)
        copper = tonumber(copper) or 0
        local parts = {}
        if copper >= 10000 then table.insert(parts, math.floor(copper / 10000) .. g) end
        if math.floor((copper % 10000) / 100) > 0 then
            table.insert(parts, math.floor((copper % 10000) / 100) .. s)
        end
        if copper % 100 > 0 or #parts == 0 then table.insert(parts, (copper % 100) .. c) end
        return table.concat(parts, " ")
    end
end

function ns.Print(msg)
    DEFAULT_CHAT_FRAME:AddMessage("|cff33ff99MobaHUD|r: " .. tostring(msg))
end

-- Saved variables do not exist at file-load time -- they arrive at ADDON_LOADED --
-- so every module's InitSavedVars runs from there, and this runs first. MobaHUDDB
-- was flat back when the bar was the only movable frame; the migration below is
-- idempotent, so a second /reload is a no-op and an upgraded client keeps its bar
-- where the player left it.
function ns.InitDB()
    MobaHUDDB = MobaHUDDB or {}

    if MobaHUDDB.point then
        MobaHUDDB.bar = {
            point = MobaHUDDB.point, relPoint = MobaHUDDB.relPoint,
            x     = MobaHUDDB.x,     y        = MobaHUDDB.y,
            locked = MobaHUDDB.locked,
        }
        MobaHUDDB.point, MobaHUDDB.relPoint = nil, nil
        MobaHUDDB.x, MobaHUDDB.y, MobaHUDDB.locked = nil, nil, nil
    end

    MobaHUDDB.bar     = MobaHUDDB.bar     or {}
    MobaHUDDB.shop    = MobaHUDDB.shop    or {}
    MobaHUDDB.minimap = MobaHUDDB.minimap or {}
end
