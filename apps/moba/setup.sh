#!/usr/bin/env bash
# apps/moba/setup.sh — apply the fork's runtime config after `make install`.
#
# Regenerates worldserver.conf + module confs from their tracked .dist templates,
# then layers optional local (gitignored) test-knob overrides on top.
# DB needs nothing here: custom SQL in data/sql/custom/db_world and the vendored
# module's SQL auto-apply on worldserver boot (Updates.AutoSetup).
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ETC="$REPO_ROOT/env/dist/etc"
LOCAL="$REPO_ROOT/apps/moba/local.conf"

[[ -d "$ETC" ]] || { echo "error: $ETC missing — run 'make install' first." >&2; exit 1; }

# 1. Generate runtime .conf from each installed .dist (worldserver + modules).
shopt -s nullglob
for dist in "$ETC/worldserver.conf.dist" "$ETC"/modules/*.conf.dist; do
  cp "$dist" "${dist%.dist}"
  echo "wrote ${dist%.dist}"
done

# 2. Layer local test-knob overrides (gitignored; optional).
if [[ -f "$LOCAL" ]]; then
  echo "applying local overrides from apps/moba/local.conf"
  while IFS= read -r line || [[ -n "$line" ]]; do
    [[ "$line" =~ ^[[:space:]]*# || -z "${line//[[:space:]]/}" ]] && continue
    key="${line%%=*}"; key="${key//[[:space:]]/}"
    for conf in "$ETC/worldserver.conf" "$ETC"/modules/*.conf; do
      grep -q "^${key} " "$conf" && sed -i.bak "s|^${key} .*|${line}|" "$conf" && rm -f "$conf.bak"
    done
  done < "$LOCAL"
fi

echo
echo "config applied. next:"
echo "  - start authserver + worldserver (DBs self-populate on first boot)"
echo "  - in-game: .debug bg   (re-run after every worldserver restart)"
