-- Feed: the two transient centre-screen overlays -- the revive countdown shown
-- while dead, and the kill feed. Neither survives a match end or a /reload.

local ADDON_NAME, ns = ...

local C_ALLY  = ns.C_ALLY
local C_ENEMY = ns.C_ENEMY
local C_DIM   = ns.C_DIM
local C_KILL  = ns.C_KILL
local C_DEATH = ns.C_DEATH
local C_END   = ns.C_END

-- One scale for everything in the feed: font, inline icons, and the row pitch
-- that has to keep up with them. Change these three together -- an icon that
-- outgrows the line height clips into the row above.
local FEED_FONT   = 13   -- point size of a feed line
local FEED_ICON   = 16   -- inline icon edge, px
local FEED_LINE_H = 19   -- vertical pitch between rows, px

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

local function ClassIcon(classId)
    local t = CLASS_TCOORDS[classId]
    if not t then return "" end
    return string.format("|T%s:%d:%d:0:0:256:256:%d:%d:%d:%d|t",
        CLASS_TEX, FEED_ICON, FEED_ICON, t[1], t[2], t[3], t[4])
end

-- The icon tables below hold the texture NAME only; this builds the escape at
-- the current size. Resizing the feed is then one constant rather than a hunt
-- through baked-in ":18:18" strings.
local function Icon(name)
    return string.format("|TInterface\\Icons\\%s:%d:%d|t", name, FEED_ICON, FEED_ICON)
end

-- Non-player death sources (D: payload `cat`). Icons are swappable placeholders.
local DEATH_SOURCE = {
    [0] = { icon = "Spell_Shadow_DeathCoil",  label = "the environment" },
    [1] = { icon = "INV_Misc_Bomb_04",        label = "a tower" },
    [2] = { icon = "INV_Misc_Head_Orc_01",    label = "a lane creep" },
    [3] = { icon = "INV_Misc_MonsterClaw_03", label = "a neutral monster" },
}

-- Structure lines (O: payload). Ids mirror MobaLane and MobaStructureKind in
-- MobaTowerData.h -- all three of that header, gen_tower_data.py's LANE_IDS, and
-- this table must agree. An unmapped id contributes no word rather than erroring,
-- which is how a core (lane 0) needs no special case.
local LANE_NAMES = { [1] = "top", [2] = "mid", [3] = "bot" }
local TIER_NAMES = { [0] = "outer", [1] = "inner" }

-- Icons are swappable placeholders, as with DEATH_SOURCE.
local STRUCT_ICON = {
    [0] = "INV_Misc_Bomb_04",         -- tower
    [1] = "Spell_Holy_Excorcism_02",  -- inhibitor
    [2] = "Spell_Fire_SelfDestruct",  -- core
}

local STRUCT_DESTROYED  = 0
local STRUCT_RESPAWNING = 1
local STRUCT_RESPAWNED  = 2

-- "outer mid turret" / "mid inhibitor" / "base". Tier only qualifies turrets:
-- on an inhibitor or core it is guard-chain depth, not a position in a lane.
local function StructureName(kind, tier, lane)
    if kind == 2 then return "base" end
    local words = {}
    if kind == 0 and TIER_NAMES[tier] then table.insert(words, TIER_NAMES[tier]) end
    if LANE_NAMES[lane] then table.insert(words, LANE_NAMES[lane]) end
    table.insert(words, (kind == 1) and "inhibitor" or "turret")
    return table.concat(words, " ")
end

-- Streak lines (X: payload) and the K: flag that decorates a kill line. The
-- server counts, the client names -- so this is the ONLY home for the ladder,
-- and the server never has to know what a "Rampage" is.
local MULTI_NAMES = {
    [2] = "Double Kill", [3] = "Triple Kill", [4] = "Quadra Kill", [5] = "Penta Kill",
}
local SPREE_NAMES = {
    [3] = "Killing Spree", [4] = "Rampage", [5] = "Unstoppable",
    [6] = "Dominating",    [7] = "Godlike",
}
-- Both ladders top out rather than erroring on an unmapped count: a spree past 7
-- stays Legendary for as long as it lasts, and a multi-kill past 5 has no name in
-- LoL either -- reachable here only if a team ever exceeds five players.
local MULTI_TOP = "Penta Kill"
local SPREE_TOP = "Legendary"

local STREAK_MULTI = 0
local STREAK_SPREE = 1
local STREAK_ACE   = 2

-- Icons are swappable placeholders, as with DEATH_SOURCE.
local STREAK_ICON = {
    [STREAK_MULTI] = "Ability_Rogue_SliceDice",
    [STREAK_SPREE] = "Ability_Warrior_InnerRage",
    [STREAK_ACE]   = "INV_BannerPVP_02",
}

-- K: flag (BG_MOBA_KillFlag). These TAG the kill line instead of pushing a second
-- one: two lines for one event would cost two of five slots and say the same
-- thing twice. nil = an ordinary kill, no tag and no rank promotion.
local KILL_FLAG_TEXT = {
    [1] = "First Blood!",
    [2] = "Shutdown!",
}

-- Boss lines (B: payload). Mirrors BG_MOBA_BossEvent and BG_MOBA_BossSide in
-- BattlegroundMOBA.h. Side 2 is the value K:/O:/X: never needed: a boss belongs
-- to no team, so a spawn takes neither the ally nor the enemy colour -- and a
-- kill can land there too, when the killing blow resolves to nobody.
local BOSS_SLAIN    = 0
local BOSS_SPAWNING = 1
local BOSS_SPAWNED  = 2

local BOSS_SIDE_OURS  = 0
local BOSS_SIDE_ENEMY = 1

-- Icon is a swappable placeholder, as with DEATH_SOURCE.
local BOSS_ICON = "INV_Misc_Head_Dragon_01"

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
--
-- Lines are ranked, not merely queued. A full feed evicts the lowest-ranked
-- line rather than the oldest, so a teamfight's routine kills cannot shove a
-- base kill or a Penta off-screen; a line that outranks nothing already up is
-- refused outright instead of displacing something bigger.
local KILL_MAX   = 5
local KILL_FADE  = 0.75    -- length of the trailing fade
local KILL_ALPHA = 0.9    -- resting opacity; the trailing fade scales down from this

-- Eviction rank, low to high, and the default lifetime each one carries: the
-- bigger the event, the longer it holds the slot it won.
local PRIO_KILL  = 0   -- routine player kill or death
local PRIO_EVENT = 1   -- structures, match flow, killing sprees
local PRIO_BIG   = 2   -- first blood, shutdown, multi-kill, ace, core

local KILL_TTL = {
    [PRIO_KILL]  = 2.5,
    [PRIO_EVENT] = 3.5,
    [PRIO_BIG]   = 4.5,
}

-- Match-flow notices (N: payload). One code per line: the server says WHICH
-- notice, this table holds every word, the icon and the rank -- same split as
-- the structure and streak tables above.
--
-- `good` is nil for the lines that belong to neither side, and the colour falls
-- through to C_DIM. Only the result and surrender pairs are anyone's news; the
-- minion lines and a match with no winner are nobody's. `arg = true` marks a
-- line whose text is a format string fed the payload's trailing number -- opt-in
-- rather than formatting everything, so a future line containing a literal % is
-- not silently mangled.
local NOTICE = {
    [1] = { icon = "INV_Misc_Head_Orc_01",    text = "%d seconds until minions spawn", arg = true, prio = PRIO_EVENT },
    [2] = { icon = "INV_Misc_Head_Orc_01",    text = "Minions have spawned!", prio = PRIO_EVENT },
    [3] = { icon = "INV_BannerPVP_02",        text = "VICTORY!", good = true,  prio = PRIO_BIG   },
    [4] = { icon = "INV_BannerPVP_01",        text = "DEFEAT!",  good = false, prio = PRIO_BIG   },
    [5] = { icon = "INV_Misc_PocketWatch_01", text = "MATCH ENDED -- NO WINNER", prio = PRIO_BIG },
    [6] = { icon = "INV_BannerPVP_02",        text = "THE ENEMY TEAM HAS SURRENDERED", good = true,  prio = PRIO_BIG },
    [7] = { icon = "INV_BannerPVP_01",        text = "YOUR TEAM HAS SURRENDERED",      good = false, prio = PRIO_BIG },
}

local killFrame = CreateFrame("Frame", "MobaHUDKillFeed", UIParent)
killFrame:SetWidth(500)
killFrame:SetHeight(KILL_MAX * FEED_LINE_H)
killFrame:SetPoint("TOP", UIParent, "TOP", 0, -120)
killFrame:SetFrameStrata("HIGH")

local killLines = {}
for i = 1, KILL_MAX do
    local fs = killFrame:CreateFontString(nil, "OVERLAY", "GameFontNormalLarge")
    fs:SetFont("Fonts\\FRIZQT__.TTF", FEED_FONT, "OUTLINE")
    fs:SetPoint("TOP", killFrame, "TOP", 0, -(i - 1) * FEED_LINE_H)
    fs:Hide()
    killLines[i] = fs
end

local killQueue = {}   -- newest first: { text = , prio = , expire = }

local function KillRender(now)
    for i = 1, KILL_MAX do
        local entry = killQueue[i]
        local fs = killLines[i]
        if entry then
            local remain = entry.expire - now
            fs:SetText(entry.text)
            fs:SetAlpha(KILL_ALPHA * (remain < KILL_FADE and (remain / KILL_FADE) or 1))
            fs:Show()
        else
            fs:Hide()
        end
    end
end

-- `ttl` overrides the rank's default lifetime. The two are separate knobs so a
-- low-stakes notice can linger without also becoming hard to evict.
local function PushKill(text, prio, ttl)
    prio = prio or PRIO_KILL
    ttl  = ttl or KILL_TTL[prio] or KILL_TTL[PRIO_KILL]

    if #killQueue >= KILL_MAX then
        -- Oldest -> newest, so ties resolve to the oldest, and only lines the
        -- newcomer matches or outranks are eligible. Within one rank every entry
        -- shares a TTL, so insertion order is expiry order and "oldest" is exact.
        local victim
        for i = #killQueue, 1, -1 do
            local e = killQueue[i]
            if e.prio <= prio and (not victim or e.prio < killQueue[victim].prio) then
                victim = i
            end
        end
        if not victim then return end   -- everything on screen outranks this line
        table.remove(killQueue, victim)
    end

    table.insert(killQueue, 1, { text = text, prio = prio, expire = GetTime() + ttl })
    KillRender(GetTime())
end

local function ClearKills()
    for i = #killQueue, 1, -1 do killQueue[i] = nil end
    for i = 1, KILL_MAX do killLines[i]:Hide() end
end

killFrame:SetScript("OnUpdate", function(self, elapsed)
    if #killQueue == 0 then return end
    local now = GetTime()
    -- Per-rank TTLs mean the tail is no longer the soonest to expire, so the whole
    -- queue is swept. Backwards, so removing one cannot skip over the next.
    for i = #killQueue, 1, -1 do
        if killQueue[i].expire <= now then table.remove(killQueue, i) end
    end
    KillRender(now)
end)

-- ---- payload handlers ----------------------------------------------------
local function HandleKill(payload)
    local pov, kName, kClass, kSide, vName, vClass, vSide, flag =
        string.match(payload, "^(%d+),([^,]+),(%d+),(%d+),([^,]+),(%d+),(%d+),(%d+)$")
    if not pov then return end
    pov    = tonumber(pov)
    kClass = tonumber(kClass)
    vClass = tonumber(vClass)
    local kIcon = ClassIcon(kClass)   -- killer emblem (left, all POVs)
    local vIcon = ClassIcon(vClass)   -- victim emblem (right, all POVs)
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

    local line = kIcon .. " " .. center .. " " .. vIcon
    local tag = KILL_FLAG_TEXT[tonumber(flag)]
    if tag then
        -- The victim is the only one who reads a First Blood or a Shutdown as bad
        -- news; for the killer and every bystander it is the highlight of the feed.
        line = line .. " " .. ((pov == 1) and C_DEATH or C_KILL) .. tag .. C_END
    end
    -- A tagged kill is a big event, an untagged one is routine. The rank is the
    -- tag: nothing else about the line changed.
    PushKill(line, tag and PRIO_BIG or PRIO_KILL)
end

local function HandleDeath(payload)
    local pov, vSide, vClass, vName, cat =
        string.match(payload, "^(%d+),(%d+),(%d+),([^,]+),(%d+)$")
    if not pov then return end
    pov    = tonumber(pov)
    vClass = tonumber(vClass)
    cat    = tonumber(cat)
    local src = DEATH_SOURCE[cat] or DEATH_SOURCE[0]
    local vIcon = ClassIcon(vClass)   -- victim emblem (right, all POVs)
    local center
    if pov == 0 then
        center = C_DEATH .. "You have been slain!" .. C_END
    else
        local vColor = (tonumber(vSide) == 0) and C_ALLY or C_ENEMY
        center = vColor .. vName .. C_END
            .. " " .. C_DIM .. "was slain by " .. src.label .. C_END
    end
    PushKill(Icon(src.icon) .. " " .. center .. " " .. vIcon, PRIO_KILL)
end

local function HandleStructure(payload)
    -- The actor group is [^,]* , not + : a structure finished by creeps carries no
    -- player name and the payload ends on an empty field. A + fails the anchored
    -- match, and the line then vanishes with nothing logged.
    local event, ownerSide, kind, tier, lane, actor =
        string.match(payload, "^(%d+),(%d+),(%d+),(%d+),(%d+),([^,]*)$")
    if not event then return end
    event     = tonumber(event)
    ownerSide = tonumber(ownerSide)
    kind      = tonumber(kind)
    tier      = tonumber(tier)
    lane      = tonumber(lane)

    local name = StructureName(kind, tier, lane)
    local mine = (ownerSide == 0)
    local named = (actor ~= "")
    local text

    if event == STRUCT_DESTROYED then
        if kind == 2 then
            if mine then
                text = named and ("Your base has fallen to " .. actor .. "!")
                              or "Your base has fallen!"
            else
                text = named and (actor .. " destroyed the enemy base!")
                              or "The enemy base has fallen!"
            end
        elseif mine then
            text = named and ("Your " .. name .. " was destroyed by " .. actor .. "!")
                          or ("Your " .. name .. " has been destroyed!")
        else
            text = named and (actor .. " destroyed the enemy " .. name .. "!")
                          or ("The enemy " .. name .. " has been destroyed!")
        end
    elseif event == STRUCT_RESPAWNING then
        text = mine and ("Your " .. name .. " is respawning soon!")
                     or ("The enemy " .. name .. " is respawning soon!")
    elseif event == STRUCT_RESPAWNED then
        text = mine and ("Your " .. name .. " has respawned!")
                     or ("The enemy " .. name .. " has respawned!")
    else
        return
    end

    -- Good news is the enemy's structure falling, or your own coming back -- which
    -- is exactly "destroyed differs from mine".
    local good = ((event == STRUCT_DESTROYED) ~= mine)
    local prio = (kind == 0) and PRIO_EVENT or PRIO_BIG

    PushKill(Icon(STRUCT_ICON[kind] or STRUCT_ICON[0]) .. " "
        .. (good and C_KILL or C_DEATH) .. text .. C_END, prio)
end

local function HandleStreak(payload)
    -- `name` is [^,]* , not + : an ace belongs to a team and carries no subject,
    -- so the payload ends on an empty field. Same trap as O:'s trailing actor --
    -- a + fails the anchored match and the line vanishes with nothing logged.
    local pov, side, name, sType, count =
        string.match(payload, "^(%d+),(%d+),([^,]*),(%d+),(%d+)$")
    if not pov then return end
    pov   = tonumber(pov)
    side  = tonumber(side)
    sType = tonumber(sType)
    count = tonumber(count)

    local mine = (side == 0)
    local text
    if sType == STREAK_MULTI then
        local label = MULTI_NAMES[count] or MULTI_TOP
        text = (pov == 0) and (label .. "!") or (name .. ": " .. label .. "!")
    elseif sType == STREAK_SPREE then
        -- Bare exclamation plus the count, never "is on a <label>": the ladder mixes
        -- nouns and adjectives ("Killing Spree" vs "Unstoppable"), so any connecting
        -- phrase reads wrong for half the tiers and breaks again on the next one added.
        local label = SPREE_NAMES[count] or SPREE_TOP
        text = (pov == 0) and string.format("%s! (%d kills)", label, count)
                          or string.format("%s: %s! (%d kills)", name, label, count)
    elseif sType == STREAK_ACE then
        text = mine and "ACE! The enemy team is wiped!"
                     or "ACE! Your team has been wiped!"
    else
        return
    end

    PushKill(Icon(STREAK_ICON[sType] or STREAK_ICON[STREAK_MULTI]) .. " "
        .. (mine and C_KILL or C_DEATH) .. text .. C_END, PRIO_BIG)
end

local function HandleNotice(payload)
    local code, arg = string.match(payload, "^(%d+),(%d+)$")
    if not code then return end
    local n = NOTICE[tonumber(code)]
    if not n then return end

    local color = C_DIM
    if n.good ~= nil then color = n.good and C_KILL or C_DEATH end
    local text = n.arg and string.format(n.text, tonumber(arg)) or n.text

    PushKill(Icon(n.icon) .. " " .. color .. text .. C_END, n.prio)
end

local function HandleBoss(payload)
    -- `name` reads [^,]* , not + . It is never legitimately empty, but a + turns
    -- a bad data row into a line that vanishes with nothing logged -- the trap
    -- the K: pattern sprang for real in section 2.
    local event, side, arg, name =
        string.match(payload, "^(%d+),(%d+),(%d+),([^,]*)$")
    if not event then return end
    event = tonumber(event)
    side  = tonumber(side)
    arg   = tonumber(arg)
    if name == "" then name = "The boss" end

    local text, color
    if event == BOSS_SLAIN then
        if side == BOSS_SIDE_OURS then
            text, color = "Your team has slain " .. name .. "!", C_KILL
        elseif side == BOSS_SIDE_ENEMY then
            text, color = "The enemy team has slain " .. name .. "!", C_DEATH
        else
            text, color = name .. " has been slain!", C_DIM
        end
    elseif event == BOSS_SPAWNING then
        text, color = string.format("%s spawns in %d seconds!", name, arg), C_DIM
    elseif event == BOSS_SPAWNED then
        text, color = name .. " has spawned!", C_DIM
    else
        return
    end

    PushKill(Icon(BOSS_ICON) .. " " .. color .. text .. C_END, PRIO_BIG)
end

ns.Feed = {
    Kill      = HandleKill,
    Death     = HandleDeath,
    Structure = HandleStructure,
    Streak    = HandleStreak,
    Notice    = HandleNotice,
    Boss      = HandleBoss,
    Respawn   = StartRespawn,
    -- Two teardowns, deliberately not one. Match end resurrects every dead player
    -- (Battleground::EndBattleground), so a live revive countdown would be a lie --
    -- but the feed keeps running: its lines are the record of how the match ended,
    -- and they expire on their own far inside the two-minute TIME_TO_AUTOREMOVE
    -- window. Only leaving the instance tears the whole thing down.
    EndMatch  = function() StartRespawn(0) end,
    Clear     = function() StartRespawn(0); ClearKills() end,
    -- Exposed for the /mhud feed tests, which drive the real push-and-evict path
    -- rather than a shortcut around it. Server payloads never reach these.
    Push      = PushKill,
    PRIO      = { KILL = PRIO_KILL, EVENT = PRIO_EVENT, BIG = PRIO_BIG },
}
