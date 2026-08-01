-- Minimap: the shop button on the minimap ring. Its own file so that the panel and
-- the thing that raises it fail independently, and so nothing here knows anything
-- about the shop beyond ns.Shop.Toggle.

local ADDON_NAME, ns = ...

-- Position is ONE number: the button is constrained to the ring, so an angle says
-- everything a point/offset pair would, and it survives a minimap that has been
-- moved or rescaled by another addon. Degrees, counter-clockwise from 3 o'clock.
local DEFAULTS = { angle = 200 }

-- Ring geometry, all of it load-bearing and none of it arbitrary -- these are
-- LibDBIcon's numbers, which is what every other minimap button on screen uses.
-- RADIUS 80 sits just outside the 140px minimap; BORDER_SIZE is what the border
-- art was drawn for and will look wrong at any other size.
local RADIUS      = 80
local BUTTON_SIZE = 31
local ICON_SIZE   = 20
local BORDER_SIZE = 53

local button = CreateFrame("Button", "MobaHUDMinimapButton", Minimap)
button:SetWidth(BUTTON_SIZE)
button:SetHeight(BUTTON_SIZE)
button:SetFrameStrata("MEDIUM")
button:SetFrameLevel(8)
button:SetMovable(true)
button:RegisterForClicks("LeftButtonUp")
button:RegisterForDrag("LeftButton")
button:Hide()

local icon = button:CreateTexture(nil, "BACKGROUND")
icon:SetTexture("Interface\\Icons\\INV_Misc_Coin_01")
icon:SetWidth(ICON_SIZE)
icon:SetHeight(ICON_SIZE)
icon:SetPoint("CENTER", button, "CENTER", 0, 1)
-- Icon art fills its square to the edge but the border is a CIRCLE, so the corners
-- poke out through it unless they are cropped away.
icon:SetTexCoord(0.07, 0.93, 0.07, 0.93)

local border = button:CreateTexture(nil, "OVERLAY")
border:SetTexture("Interface\\Minimap\\MiniMap-TrackingBorder")
border:SetWidth(BORDER_SIZE)
border:SetHeight(BORDER_SIZE)
border:SetPoint("TOPLEFT", button, "TOPLEFT", 0, 0)

button:SetHighlightTexture("Interface\\Minimap\\UI-Minimap-ZoomButton-Highlight")

local function ApplyPosition()
    local a = math.rad(MobaHUDDB.minimap.angle)
    button:ClearAllPoints()
    button:SetPoint("CENTER", Minimap, "CENTER", RADIUS * math.cos(a), RADIUS * math.sin(a))
end

-- Cursor coordinates come back in SCREEN pixels while GetCenter answers in the
-- frame's own units, so one of the two has to be converted or the button lags the
-- pointer by however far the UI scale is from 1.
local function OnDragUpdate()
    local mx, my = Minimap:GetCenter()
    local cx, cy = GetCursorPosition()
    local scale  = Minimap:GetEffectiveScale()

    MobaHUDDB.minimap.angle = math.deg(math.atan2(cy / scale - my, cx / scale - mx)) % 360
    ApplyPosition()
end

button:SetScript("OnDragStart", function(self) self:SetScript("OnUpdate", OnDragUpdate) end)
button:SetScript("OnDragStop",  function(self) self:SetScript("OnUpdate", nil) end)

button:SetScript("OnClick", function() ns.Shop.Toggle() end)

button:SetScript("OnEnter", function(self)
    GameTooltip:SetOwner(self, "ANCHOR_LEFT")
    GameTooltip:AddLine("MOBA Shop")
    GameTooltip:AddLine("Click to open the shop.", 0.8, 0.8, 0.8)
    GameTooltip:AddLine("Drag to move this button.", 0.8, 0.8, 0.8)
    GameTooltip:Show()
end)
button:SetScript("OnLeave", function() GameTooltip:Hide() end)

ns.Minimap = {
    Show = function() button:Show() end,
    Hide = function() button:Hide() end,

    -- ns.InitDB has already created MobaHUDDB.minimap.
    InitSavedVars = function()
        for k, v in pairs(DEFAULTS) do
            if MobaHUDDB.minimap[k] == nil then MobaHUDDB.minimap[k] = v end
        end
        ApplyPosition()
    end,

    ResetPosition = function()
        for k, v in pairs(DEFAULTS) do MobaHUDDB.minimap[k] = v end
        ApplyPosition()
    end,
}
