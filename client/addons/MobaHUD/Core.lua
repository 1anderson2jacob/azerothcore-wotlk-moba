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
