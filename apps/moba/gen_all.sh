#!/usr/bin/env bash
# Regenerates every MOBA SQL bundle (and the addon catalog) in dependency order.
# Generators resolve their output paths relative to the repo root, so this cds
# there rather than trusting the caller's cwd.
set -euo pipefail

cd "$(dirname "${BASH_SOURCE[0]}")/../.."

# gen_creep_paths must precede gen_creep_roster: it locks the lane/slot waypoint
# IDs the roster resolves each creep's WaypointPathId from. Everything else is
# independent -- the ID-allocating generators own disjoint entry ranges.
for gen in \
    gen_creep_paths.py \
    gen_creep_roster.py \
    gen_tower_data.py \
    gen_base.py \
    gen_neutral_camps.py \
    gen_store.py \
    gen_player_drops.py
do
    echo "== ${gen}"
    python3 "apps/moba/${gen}"
done
