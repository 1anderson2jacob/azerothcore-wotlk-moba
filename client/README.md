# Client-side artifacts

Files here ship to the **WoW 3.3.5a game client**, not the worldserver. They are
not compiled and are not touched by CMake. This is the home for the deferred
client-patch work too (DBC/MPQ content) as it lands.

## addons/MobaHUD

The on-screen match HUD bar for the MOBA battleground: team kill score, your KDA,
creep score (CS), and the elapsed match clock.

**Install:** copy `addons/MobaHUD` into your WoW 3.3.5a client's `Interface/AddOns/`
folder, so you end up with `Interface/AddOns/MobaHUD/MobaHUD.toc`. Enable it at the
character screen (AddOns button) and log in.

**How it works:** on entering a battleground the addon sends one "ready" ping to the
server, which replies with the current HUD state. After that the server pushes updates
as things happen (kills, creep last-hits), plus a periodic safety resync, and an `E` to
hide the bar at match end or when you leave. The clock counts up locally between
messages. See `BattlegroundMOBA::SendHudMessage` / `SendHudStateTo` and
`src/server/scripts/Custom/moba_hud.cpp` for the server side, and
`.github/MOBA_GUIDE.md` ("How the HUD bar works") for the full protocol.

**Commands:** `/mobahud test` shows the bar with sample values so you can drag it into
position, then `/mobahud lock`. Also `stop | unlock | reset` (or `/mhud`).
