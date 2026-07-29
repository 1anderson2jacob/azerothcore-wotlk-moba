-- Feed: the two transient centre-screen overlays -- the revive countdown shown
-- while dead, and the kill feed. Neither survives a match end or a /reload.

local ADDON_NAME, ns = ...

local C_ALLY  = ns.C_ALLY
local C_ENEMY = ns.C_ENEMY
local C_DIM   = ns.C_DIM
local C_KILL  = ns.C_KILL
local C_DEATH = ns.C_DEATH
local C_END   = ns.C_END

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

-- ---- payload handlers ----------------------------------------------------
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

ns.Feed = {
    Kill    = HandleKill,
    Death   = HandleDeath,
    Respawn = StartRespawn,
    Clear   = function() StartRespawn(0); ClearKills() end,
}
