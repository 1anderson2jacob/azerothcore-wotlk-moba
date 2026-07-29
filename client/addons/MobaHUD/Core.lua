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
