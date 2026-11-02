#!/usr/bin/env python3
# Sprint 11m S559 — DAG RE-STATE (the DEFINED drift-closure step).
#
# gap_dp_daggen.py is a GENERATOR that OVERWRITES the whole docs/DAG_controls.md
# (the S503a GEN-LOSSY note): re-running it wipes the manual-add rows. This script
# therefore:
#   1) snapshots the current (manual-add-rich) DAG,
#   2) runs the generator (base-regen — the orphan-class killer; the stale
#      PLANNED rows for implemented-but-unstamped controls flip per the updated
#      VAML_STATUS ground truth),
#   3) re-applies ONLY the manual-add NOTE blocks verbatim (the GEN-LOSSY escape:
#      the 12k-5 trigger family, the 11f behaviors/DataTemplate family, and the
#      11n-0 DAG-EXT cluster). It NEVER copies `|` table rows back, so the
#      regenerated DAG keeps EXACTLY-ONE-STATE per row (the DAG contract).
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DAG = os.path.join(HERE, "docs", "DAG_controls.md")

def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()

def write(path, s):
    with open(path, "w", encoding="utf-8") as f:
        f.write(s)

# 1) snapshot the current (manual-add-rich) DAG
pre = read(DAG)
pre_lines = pre.splitlines(keepends=True)

# 2) run the generator (base-regen)
subprocess.run([sys.executable, os.path.join(HERE, "gap_dp_daggen.py")],
               check=True, cwd=HERE)

def is_table_row(l):
    # a real table data row begins with a pipe after whitespace
    t = l.lstrip()
    return t.startswith("|") and "``" in t

def extract_manual_notes(lines):
    """Return ONLY the manual-add NOTE blocks (contiguous `>` quote lines whose
    first line names the escape), plus the `>`-only 11n-0 DAG-EXT cluster. Never
    table rows; never the generator's own intro notes."""
    blocks = []
    i = 0
    n = len(lines)
    # We only want the notes the GENERATOR cannot emit: the ones that name the
    # escape. Capture each contiguous `>` block whose first line mentions the
    # marker; skip all `|` table rows and the intro `>` notes (11k-0a/tri-state).
    while i < n:
        t = lines[i].lstrip()
        if t.startswith(">") and any(m in lines[i] for m in ("manual add", "DAG-EXT", "GEN-LOSSY", "EventTrigger")):
            j = i
            block = []
            while j < n and lines[j].lstrip().startswith(">"):
                block.append(lines[j]); j += 1
            blocks.append("".join(block))
            i = j
            continue
        i += 1
    return blocks

blocks = extract_manual_notes(pre_lines)

# dedup by first line (idempotent re-runs)
out = read(DAG).rstrip() + "\n"
seen = set()
kept = []
for blk in blocks:
    key = blk.splitlines()[0].strip() if blk.splitlines() else blk
    if key in seen:
        continue
    seen.add(key)
    kept.append(blk)

if kept:
    out += "\n## Manual-add rows (re-applied verbatim — the S503a GEN-LOSSY escape)\n\n"
    out += "\n\n".join(kept) + "\n"

write(DAG, out)
print("S559 DAG re-state complete;", len(kept), "manual note-blocks re-applied.")
