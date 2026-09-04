#!/usr/bin/env python3
"""
Row lookups into the committed base-world dumps in data/sql/base/db_world.

These are mysqldump output: a CREATE TABLE naming every column in order, then
INSERT ... VALUES carrying no column list. Column order therefore has exactly
one source -- the CREATE TABLE in the same file -- and positional tuples are
zipped against it.

Shared by gen_store.py (item_template) and the creep/neutral generators
(creature_template).
"""

import hashlib
import re
from functools import lru_cache
from pathlib import Path

CREATURE_TEMPLATE_SQL = Path("data/sql/base/db_world/creature_template.sql")


def parse_tuple_at(data, start):
    """Split the SQL VALUES tuple beginning at data[start] == '(' into raw
    fields. Returns None if it is not a well-formed tuple."""
    if data[start] != "(":
        return None
    fields, cur, in_quote = [], [], False
    i = start + 1
    n = len(data)
    while i < n:
        c = data[i]
        if in_quote:
            if c == "\\":
                cur.append(data[i:i + 2])
                i += 2
                continue
            if c == "'":
                in_quote = False
            cur.append(c)
            i += 1
            continue
        if c == "'":
            in_quote = True
            cur.append(c)
            i += 1
            continue
        if c == ",":
            fields.append("".join(cur))
            cur = []
            i += 1
            continue
        if c == ")":
            fields.append("".join(cur))
            return fields
        if c == "\n":       # a tuple never spans rows in these dumps
            return None
        cur.append(c)
        i += 1
    return None


def unquote(s):
    s = s.strip()
    if s.startswith("'") and s.endswith("'"):
        s = s[1:-1]
    return s.replace("\\'", "'").replace("\\\\", "\\")


@lru_cache(maxsize=None)
def _creature_template():
    """(file text, ordered columns). Parsed once -- the file is ~5 MB."""
    text = CREATURE_TEMPLATE_SQL.read_text(encoding="utf-8", errors="replace")
    m = re.search(r"CREATE TABLE `creature_template` \((.*?)\n\) ENGINE", text, re.S)
    cols = re.findall(r"^\s*`([A-Za-z0-9_]+)`\s", m.group(1), re.M) if m else []
    return text, cols


def creature_template_columns():
    return _creature_template()[1]


def load_creature_row(entry):
    """One creature_template row -> (dict col->value, ordered cols, digest),
    or (None, None, None) when the entry is absent.

    The digest covers the row's raw fields, so callers can pin a source creature
    against upstream retuning it.
    """
    text, cols = _creature_template()
    if not cols:
        return None, None, None
    for m in re.finditer(r"\((%d)," % entry, text):
        fields = parse_tuple_at(text, m.start())
        # The id also appears as a spell id, a loot id and plain stat values, so
        # a tuple counts only when it starts with the entry AND is row-shaped.
        if not fields or len(fields) != len(cols) or fields[0] != str(entry):
            continue
        digest = hashlib.sha256("\x1f".join(fields).encode()).hexdigest()[:16]
        return dict(zip(cols, (unquote(f) for f in fields))), cols, digest
    return None, None, None
