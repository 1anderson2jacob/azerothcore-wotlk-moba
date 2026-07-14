# Client-side artifacts

Files here ship to the **WoW 3.3.5a game client**, not the worldserver. They are
not compiled and are not touched by CMake. This is the home for the deferred
client-patch work too (DBC/MPQ content) as it lands.

## addons/MobaClock

An on-screen elapsed-match-time clock for the MOBA battleground.

**Install:** copy `addons/MobaClock` into your WoW client's
`Interface/AddOns/` folder, so you end up with
`Interface/AddOns/MobaClock/MobaClock.toc`. Enable it at the character screen
(AddOns button) and log in.

**How it works:** the server sends the elapsed time on the addon channel
(prefix `MobaClock`) at match start, to late joiners, every ~10s as a resync,
and an `E` to hide it at match end. The addon counts up locally between updates.
See `BattlegroundMOBA::SendMatchClock` for the server side.

**Test without the server:** `/mobaclock test` shows the clock counting from 0
so you can drag it into position, then `/mobaclock lock`. Other commands:
`/mobaclock stop | lock | unlock | reset` (or `/mclock`).
