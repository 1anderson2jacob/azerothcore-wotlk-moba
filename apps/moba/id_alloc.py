#!/usr/bin/env python3
"""
Central ID registry for the MOBA generators.

Every custom ID this fork assigns -- creature_template entries, creature spawn
guids, gameobject_template entries, waypoint_data ids, item_template copies --
is owned by exactly one generator, and each owner's claim is a block declared in
apps/moba/id_blocks.json. This module reads that ledger plus every SOURCE OF
TRUTH (the map-bundle lockfiles and the hand-assigned fields in the configs) and
answers two questions: who owns id N in namespace X, and does a proposed id
collide with anything.

It deliberately does NOT read generated SQL. Doing that is what left two holes:
the creep/neutral scanner matches only `DELETE ... WHERE entry IN (...)`, so
gen_store.py's `DELETE ... BETWEEN` window was invisible to it, and it made
allocation depend on generator run order and on the generated SQL being present.

Namespaces are INDEPENDENT: waypoint_data 900206 and creature_template 900206
are unrelated ids and both legal. lane_config.yaml's waypoint pool 900100-900499
overlaps the neutral, store and dome blocks numerically for exactly that reason,
so every lookup here is keyed by (namespace, id) and never by id alone.

Usage (from the repo root):
    python3 apps/moba/id_alloc.py --audit
"""

import argparse
import json
import sys
from collections import namedtuple
from pathlib import Path

import yaml

MOBA_DIR = Path(__file__).parent
MAPS_DIR = MOBA_DIR / "maps"
LEDGER_PATH = MOBA_DIR / "id_blocks.json"

CREATURE_TEMPLATE = "creature_template"
CREATURE_SPAWN = "creature"          # the guid column, not the entry column
GAMEOBJECT_TEMPLATE = "gameobject_template"
GAME_GRAVEYARD = "game_graveyard"
BATTLEGROUND_TEMPLATE = "battleground_template"
WAYPOINT_DATA = "waypoint_data"
ITEM_TEMPLATE = "item_template"
# New blocks are this wide and start on a multiple of it. ID space is not scarce
# (900000+ is ~100k wide) but ledger readability is, so blocks stay round.
BLOCK_SIZE = 100

# One id in use right now: which owner claimed it, what the owner calls it, and
# the file that says so. `label` is the owner's own vocabulary -- a lockfile key,
# a tower name, a lane/slot -- so audit output reads in the terms you edit in.
Assignment = namedtuple("Assignment", "namespace id owner label source")


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def _rel(path):
    """Paths print relative to apps/moba so audit lines stay narrow."""
    try:
        return str(Path(path).relative_to(MOBA_DIR))
    except ValueError:
        return str(path)


def _load_yaml(path):
    return yaml.safe_load(path.read_text()) or {}


def _load_lock(path):
    return json.loads(path.read_text()) if path.is_file() else {}


# ------------------------------------------------------------------ readers
# One per source of truth. Each yields Assignments; none of them read SQL.

def _read_towers():
    yield from _read_entry_locks("*/tower_config.lock.json", "towers")


def _read_shopkeepers():
    # shopkeeper.teams[].entry ONLY. store_config's `tabs` are full of `entry:`
    # keys as well, but those are stock item ids in another namespace, and a
    # regex over "entry:" would drag them in here as creature entries.
    #
    # One field, TWO allocations: gen_store.py spawns each shopkeeper with
    # guid == entry, so the same number is also claimed in `creature`.
    for path in sorted(MAPS_DIR.glob("*/store_config.yaml")):
        for t in (_load_yaml(path).get("shopkeeper") or {}).get("teams") or []:
            if isinstance(t.get("entry"), int):
                label = t.get("name", "?")
                yield Assignment(CREATURE_TEMPLATE, t["entry"], "store", label, path)
                yield Assignment(CREATURE_SPAWN, t["entry"], "store",
                                 f"{label} (spawn guid)", path)


def _read_domes():
    for path in sorted(MAPS_DIR.glob("*/base_config.yaml")):
        dome = (_load_yaml(path).get("spawn") or {}).get("dome") or {}
        for key in ("alliance_entry", "horde_entry"):
            if isinstance(dome.get(key), int):
                yield Assignment(GAMEOBJECT_TEMPLATE, dome[key], "base_dome", key, path)


def _read_graveyards():
    for path in sorted(MAPS_DIR.glob("*/base_config.yaml")):
        spawn = _load_yaml(path).get("spawn") or {}
        for team in ("alliance", "horde"):
            gid = (spawn.get(team) or {}).get("graveyard_id")
            if isinstance(gid, int):
                yield Assignment(GAME_GRAVEYARD, gid, "base_spawn", team, path)


def _read_entry_locks(pattern, owner):
    """creep and neutral lockfiles share one shape: {"entries": {key: id}}."""
    for path in sorted(MAPS_DIR.glob(pattern)):
        for key, entry in sorted(_load_lock(path).get("entries", {}).items()):
            yield Assignment(CREATURE_TEMPLATE, entry, owner, key, path)


def _read_creeps():
    yield from _read_entry_locks("*/creep_config.lock.json", "creeps")


def _read_neutrals():
    yield from _read_entry_locks("*/neutral_config.lock.json", "neutrals")


def _read_creep_paths():
    for path in sorted(MAPS_DIR.glob("*/lane_config.lock.json")):
        lanes = _load_lock(path).get("path_ids", {})
        for lane in sorted(lanes):
            for slot in sorted(lanes[lane]):
                for direction, pid in sorted(lanes[lane][slot].items()):
                    yield Assignment(WAYPOINT_DATA, pid, "creep_paths",
                                     f"{lane}/{slot} {direction}", path)


# item_template has no reader: a copy's entry is its source entry + 900000,
# derived from the catalog at emit time and never recorded anywhere. The ledger
# reserves the mirror; there is nothing to enumerate.
READERS = (_read_towers, _read_shopkeepers, _read_domes, _read_graveyards,
           _read_creeps, _read_neutrals, _read_creep_paths)


# ----------------------------------------------------------------- registry

class Registry:
    def __init__(self, ledger_path=LEDGER_PATH):
        self.ledger_path = ledger_path
        if not ledger_path.is_file():
            fail(f"ID ledger not found: {ledger_path}")
        # Kept verbatim so add_block can write back without losing `note` and
        # `_comment`: the parsed form below drops every field it does not use,
        # and round-tripping through it would quietly strip the whole file's
        # documentation the first time a block grew.
        self._raw = json.loads(ledger_path.read_text())
        self.ledger = self._parse_ledger(self._raw, ledger_path)
        self.assignments = [a for reader in READERS for a in reader()]
        self._taken = {}
        for a in self.assignments:
            self._taken.setdefault(a.namespace, {}).setdefault(a.id, []).append(a)

    @staticmethod
    def _parse_ledger(raw, path):
        ledger = {}
        for namespace, owners in raw.items():
            if namespace.startswith("_"):
                continue
            ledger[namespace] = {}
            for owner, spec in owners.items():
                blocks = []
                for text in spec["blocks"]:
                    try:
                        lo, hi = (int(v) for v in str(text).split("-"))
                    except ValueError:
                        fail(f'{path}: {namespace}/{owner} block "{text}" '
                             f'must read "low-high"')
                    if lo > hi:
                        fail(f"{path}: {namespace}/{owner} block {text} is inverted")
                    blocks.append((lo, hi))
                ledger[namespace][owner] = {
                    "blocks": blocks,
                    "enumerated": spec.get("enumerated", True),
                    "source": spec.get("source", ""),
                }
        return ledger

    def blocks_of(self, namespace, owner):
        return self.ledger.get(namespace, {}).get(owner, {}).get("blocks", [])

    def capacity(self, namespace, owner):
        return sum(hi - lo + 1 for lo, hi in self.blocks_of(namespace, owner))

    def owner_of(self, namespace, id_):
        """The owner whose declared block contains id_, or None."""
        for owner in self.ledger.get(namespace, {}):
            if any(lo <= id_ <= hi for lo, hi in self.blocks_of(namespace, owner)):
                return owner
        return None

    def taken(self, namespace):
        """{id: [Assignment, ...]} -- a list, because collisions are the point."""
        return self._taken.get(namespace, {})

    def conflicts(self, namespace, owner, id_, label):
        """Why `owner` may not use `id_`. An empty list means it is free.

        Callers pass their own (owner, label) so re-validating an id the caller
        already holds is not reported as colliding with itself -- which is what
        lets a generator validate the very entries this registry just read back
        out of its config.
        """
        problems = []
        for a in self.taken(namespace).get(id_, []):
            if a.owner == owner and a.label == label:
                continue
            problems.append(f'held by {a.owner} "{a.label}" ({_rel(a.source)})')
        holder = self.owner_of(namespace, id_)
        if holder is None:
            problems.append(f"outside every declared {namespace} block "
                            f"({_rel(LEDGER_PATH)})")
        elif holder != owner:
            span = next(f"{lo}-{hi}" for lo, hi in self.blocks_of(namespace, holder)
                        if lo <= id_ <= hi)
            problems.append(f"inside the {holder} block {span}")
        return problems

    def add_block(self, namespace, owner, lo, hi):
        """Record a newly carved block, in memory and on disk.

        Writes through the raw JSON, not the parsed ledger, so `note` and
        `_comment` survive. Re-runs the overlap check first and refuses to save
        a ledger that breaks it -- two owners sharing a block is the one failure
        this whole design cannot survive, since each window-clears its own range.
        """
        self.ledger[namespace][owner]["blocks"].append((lo, hi))
        self._raw[namespace][owner]["blocks"].append(f"{lo}-{hi}")
        if problems := _ledger_problems(self):
            fail(f"refusing to write {_rel(self.ledger_path)} -- the new block "
                 f"{namespace}/{owner} {lo}-{hi} breaks it:\n  " + "\n  ".join(problems))
        self.ledger_path.write_text(json.dumps(self._raw, indent=2, sort_keys=True) + "\n")


class Allocator:
    """Issues ids inside one owner's declared blocks, lowest first.

    Seeded from the registry, so it starts knowing every id held by every other
    map bundle and every other owner. That is what replaces collect_used_entries:
    the old scan regexed generated SQL, so it could not see gen_store.py's
    `DELETE ... BETWEEN` window at all, and it went blind whenever the generated
    SQL was absent or a generator ran out of order.

    Ids issued during this run are remembered too, so repeated takes in one pass
    never repeat.
    """

    def __init__(self, registry, namespace, owner):
        self.registry = registry
        self.namespace = namespace
        self.owner = owner
        self.blocks = registry.blocks_of(namespace, owner)
        if not self.blocks:
            fail(f"no {namespace} block declared for owner {owner!r} "
                 f"in {_rel(LEDGER_PATH)}")
        self._used = set(registry.taken(namespace))
        self.issued = []
        self.grown = []

    def _grow(self):
        """Carve another block for this owner: BLOCK_SIZE wide, aligned, above
        every block already declared in this namespace.

        Above rather than into a gap, so ids stay in declaration order and the
        ledger reads top to bottom. Gaps do get wasted; ID space does not matter
        here and a tower entry landing at 900100 next to waypoint 900100 does.
        """
        highest = max(hi for owner in self.registry.ledger[self.namespace]
                      for _lo, hi in self.registry.blocks_of(self.namespace, owner))
        lo = (highest // BLOCK_SIZE + 1) * BLOCK_SIZE
        hi = lo + BLOCK_SIZE - 1
        self.registry.add_block(self.namespace, self.owner, lo, hi)
        self.blocks = self.registry.blocks_of(self.namespace, self.owner)
        self.grown.append((lo, hi))
        print(f"  id_alloc: {self.owner} filled its {self.namespace} blocks -- "
              f"carved {lo}-{hi} and recorded it in {_rel(self.registry.ledger_path)}")
        return lo, hi

    def take(self, count=1):
        """`count` free ids, growing the owner's blocks when they fill.

        NOT guaranteed contiguous -- the existing waypoint pairs are 900110 and
        900120, not neighbours, and a take spanning a growth never is.
        """
        out = []
        while len(out) < count:
            for lo, hi in self.blocks:
                candidate = lo
                while candidate <= hi and len(out) < count:
                    if candidate not in self._used:
                        self._used.add(candidate)
                        out.append(candidate)
                    candidate += 1
                if len(out) == count:
                    break
            if len(out) < count:
                self._grow()
        self.issued += out
        return out  


def sql_window(blocks, column):
    """A SQL predicate covering every block an owner holds.

    Generators clear by block, not by roster. A DELETE built from the CURRENT
    roster can never name an entry the config no longer has, so dropping a mob
    from config left its creature_template row live in acore_world forever.
    Sweeping the whole block needs no memory of what was stranded.

    One block renders bare; several render parenthesised, since these land in a
    WHERE that may carry other terms.
    """
    if not blocks:
        fail(f"sql_window called with no blocks for {column}")
    parts = [f"{column} BETWEEN {lo} AND {hi}" for lo, hi in blocks]
    return parts[0] if len(parts) == 1 else "(" + " OR ".join(parts) + ")"


def validate_owner(namespace, owner, registry=None):
    """Hard-fail unless every id the registry attributes to `owner` is legal.

    The guard gen_tower_data.py, gen_store.py and gen_base.py call. It takes no
    ids: the registry already read them out of the same config the caller
    parsed, so there is one source of truth for both the number and the name.
    Passing them in would duplicate the label -- which conflicts() uses as
    identity -- and a drift between the two spellings would read as a false
    collision.

    Without this a hand-typed tower entry of 900050 lands on a lane creep and
    nothing anywhere notices: gen_tower_data.py checks only tower-vs-tower
    uniqueness, and the creep scanner never read tower config at all.
    """
    reg = registry or Registry()
    problems = []
    for a in reg.assignments:
        if a.namespace != namespace or a.owner != owner:
            continue
        for why in reg.conflicts(a.namespace, a.owner, a.id, a.label):
            problems.append(f'  {a.id} "{a.label}" ({_rel(a.source)}): {why}')
    if problems:
        fail(f"{owner} has {namespace} id conflicts:\n" + "\n".join(problems)
             + f"\n\nBlocks are declared in {_rel(LEDGER_PATH)}.")
    return reg


# -------------------------------------------------------------------- audit

def _ledger_problems(reg):
    """Blocks must not overlap -- a window-clear by either owner wipes the other."""
    problems = []
    for namespace in sorted(reg.ledger):
        spans = sorted((lo, hi, owner)
                       for owner in reg.ledger[namespace]
                       for lo, hi in reg.blocks_of(namespace, owner))
        for (lo, hi, owner), (lo2, hi2, owner2) in zip(spans, spans[1:]):
            if lo2 <= hi:
                who = (f"{owner}'s own blocks {lo}-{hi} and {lo2}-{hi2} overlap"
                       if owner == owner2 else
                       f"{owner} {lo}-{hi} overlaps {owner2} {lo2}-{hi2} -- a "
                       f"window-clear by either would wipe the other")
                problems.append(f"{namespace}: {who}")
    return problems


def audit():
    reg = Registry()
    problems = _ledger_problems(reg)

    print(f"MOBA ID registry -- {_rel(LEDGER_PATH)}")
    namespaces = sorted(set(reg.ledger) | {a.namespace for a in reg.assignments})
    for namespace in namespaces:
        print(f"\n{namespace}")
        taken = reg.taken(namespace)
        for owner in sorted(reg.ledger.get(namespace, {})):
            spec = reg.ledger[namespace][owner]
            spans = ", ".join(f"{lo}-{hi}" for lo, hi in spec["blocks"])
            mine = sorted((i, a) for i, holders in taken.items()
                          for a in holders if a.owner == owner)
            if not spec["enumerated"]:
                usage = "derived, not enumerated"
            else:
                usage = f"{len(mine)}/{reg.capacity(namespace, owner)} used"
            print(f"  {owner:<12} {spans:<17} {usage:<24} {spec['source']}")
            for id_, a in mine:
                print(f"      {id_}  {a.label:<34} {_rel(a.source)}")

        orphans = sorted((i, a) for i, holders in taken.items() for a in holders
                         if a.owner not in reg.ledger.get(namespace, {}))
        for id_, a in orphans:
            print(f"  (no block declared for owner {a.owner!r})")
            print(f"      {id_}  {a.label:<34} {_rel(a.source)}")

    for a in reg.assignments:
        for why in reg.conflicts(a.namespace, a.owner, a.id, a.label):
            problems.append(f'{a.namespace} {a.id} "{a.label}" ({a.owner}): {why}')

    print()
    if problems:
        for p in problems:
            print(f"COLLISION: {p}", file=sys.stderr)
        print(f"\n{len(problems)} problem(s).", file=sys.stderr)
        return 1
    print(f"Clean -- {len(reg.assignments)} assignment(s) across "
          f"{len(namespaces)} namespace(s), no collisions.")
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__.strip().splitlines()[0])
    parser.add_argument("--audit", action="store_true",
                        help="print every allocation and flag collisions (the default)")
    parser.parse_args()
    sys.exit(audit())


if __name__ == "__main__":
    main()
