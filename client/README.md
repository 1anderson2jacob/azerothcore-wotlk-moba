# Client-side artifacts

Files here ship to the **WoW 3.3.5a game client**, not the worldserver. They are not
compiled and CMake does not touch them. The deferred client-patch work (DBC/MPQ content)
will land here too.

## addons/MobaHUD

Everything the MOBA battleground draws on screen: the scoreboard bar (team score, KDA,
creep score, gold, match clock), the centre-screen revive countdown, the kill feed, the
floating gold numbers, and the item-shop panel with its minimap button.

**Install** — copy `addons/MobaHUD` into your client's `Interface/AddOns/`, so you end up
with `Interface/AddOns/MobaHUD/MobaHUD.toc`. Enable it at the character screen (AddOns
button) and log in. The shop is addon-only: without it a match plays, but nothing can be
bought.

**Recopy it after regenerating the shop.** `Catalog.lua` is generated into this folder by
`apps/moba/gen_store.py` and never crosses the wire, so a catalog change needs a fresh
copy of the addon — restarting the server is not enough.

**How it works** — the addon pings once on entering the world and the server replies with
current HUD state; after that the server pushes events as they happen, plus a periodic
resync. The clock counts up locally between messages. The wire protocol is documented in
`MobaHUD.lua`'s header, beside the code that parses it; the design is in
`.github/MOBA_GUIDE.md` under "HUD bar".

**Commands** (`/mobahud`, or `/mhud`) — `unlock` to drag things into place, `lock` when
done, `reset` to recentre bar, shop and minimap button, `shop` to toggle the panel,
`test` for a sample bar, `stop` to hide. The rest are feed and bar preview harnesses used
while developing the addon; the slash handler in `MobaHUD.lua` is their current list.
