#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
parity_matrix_gen.py — S642 GENERIC CAPABILITY PARITY: PARITY MATRIX generator.

Reads the SINGLE-SOURCE-OF-TRUTH capability catalogue (tools/capability_catalogue.json)
and derives docs/PARITY_MATRIX.md from it. The matrix is NEVER hand-edited — it is the
derived output of the catalogue (the same single-source discipline as:
  gap_dp_daggen.py -> docs/DAG_controls.md
  render_surface_check.py -> docs/RENDER_SURFACE.md
).

PARITY MATRIX contract (never silent):
  A row =  capability | our-side status | WPF-side status | parity | named gap.
  The generator FAILS LOUDLY (non-zero exit) when a capability's status is blank or
  unknown, or when `parity` is not one of the legal values (yes/no) — so a capability
  can never silently sit outside the matrix. A `no` parity MUST carry a named gap.

Usage:  python parity_matrix_gen.py [--catalogue tools/capability_catalogue.json]
                                     [--out docs/PARITY_MATRIX.md]
"""

import argparse
import json
import sys

LEGAL_STATES = {"introduced", "done", "planned", "out-of-scope"}
LEGAL_PARITY = {"yes", "no"}


def _fail(msg):
    print("PARITY MATRIX GEN: FAIL — " + msg)
    return False


def generate(catalogue_path, out_path):
    with open(catalogue_path, "r", encoding="utf-8") as fh:
        cat = json.load(fh)

    cap = cat.get("capabilities", [])
    if not isinstance(cap, list) or not cap:
        return _fail("catalogue has no 'capabilities' list")

    families = cat.get("families", {})
    version = cat.get("version", 1)
    soft_start = cat.get("counts_are_soft_start", False)

    rows = []
    ok = True
    for c in cap:
        cid = c.get("id", "")
        our = c.get("our", "")
        wpf = c.get("wpf", "")
        parity = c.get("parity", "")
        gap = c.get("gap", "")
        kind = c.get("kind", "")
        tol = str(c.get("tolerance", ""))
        typ = c.get("type", "")

        # --- never silent: every field must be present & legal ---
        if not cid:
            ok = _fail("a capability row is missing its 'id'") and ok
        if our not in LEGAL_STATES:
            ok = _fail("capability %s: our-side status '%s' is blank/unknown" % (cid, our)) and ok
        if wpf not in LEGAL_STATES:
            ok = _fail("capability %s: WPF-side status '%s' is blank/unknown" % (cid, wpf)) and ok
        if parity not in LEGAL_PARITY:
            ok = _fail("capability %s: parity '%s' must be yes|no" % (cid, parity)) and ok
        if parity == "no" and not gap:
            ok = _fail("capability %s: parity=no but no named gap (never silent)" % cid) and ok

        rows.append((cid, typ, kind, tol, our, wpf, parity, gap))

    if not ok:
        return False

    # --- emit the markdown ---
    lines = []
    lines.append("# PARITY_MATRIX.md — S642 GENERIC CAPABILITY PARITY (generated)")
    lines.append("")
    lines.append("> **Generated** by `tools/parity_matrix_gen.py` from the SINGLE-SOURCE-OF-TRUTH")
    lines.append("> `tools/capability_catalogue.json`. **Do NOT hand-edit this file** — re-run the")
    lines.append("> generator when the catalogue advances (the same discipline as `gap_dp_daggen.py`")
    lines.append("> → `docs/DAG_controls.md`).")
    lines.append(">")
    lines.append("> **PARITY — never silent.** Every capability row states our-side status, the")
    lines.append("> WPF-emitter-side status, and parity yes/no. A `no` parity MUST carry a named gap.")
    lines.append("> The SYNC TOKEN mechanism (S642): a capability lands on OUR side AND the")
    lines.append("> WPF-emitter side IN THE SAME STEP, then the matrix is regenerated — no drift.")
    lines.append(">")
    lines.append("> **COUNT SCOPE — SOFT START:** the catalogue families (10 int / 10 bool / 10 string")
    lines.append("> / 10 double / 10 icon / 10 text) are an **initial exemplar, NOT a hard limit**.")
    lines.append("> When testing permutations demand more of a family, the catalogue + this matrix +")
    lines.append("> the sync check GROW IN PLACE (no new step, no backfill) — recorded as the S642")
    lines.append("> count-scope token.")
    lines.append("")
    lines.append(f"version: {version}")
    if soft_start:
        lines.append("counts_are_soft_start: true")
    lines.append("")
    lines.append("| capability | type | kind | tolerance | our side | WPF side | parity | named gap |")
    lines.append("|-----------|------|------|-----------|----------|----------|--------|-----------|")
    for cid, typ, kind, tol, our, wpf, parity, gap in rows:
        lines.append(f"| `{cid}` | {typ} | {kind} | {tol} | {our} | {wpf} | {parity} | {gap or '—'} |")

    # --- family summary table (soft-start extensibility) ---
    lines.append("")
    lines.append("## Family summary (soft-start counts)")
    lines.append("")
    lines.append("| family | type | count | kind | default tolerance |")
    lines.append("|--------|------|-------|------|-------------------|")
    for fname, meta in families.items():
        lines.append(f"| `{fname}` | `{meta.get('type','')}` | {meta.get('count','')} | "
                     f"`{meta.get('kind','')}` | {meta.get('tolerance','')} |")

    with open(out_path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines) + "\n")

    print(f"PARITY MATRIX GEN: OK — {len(rows)} capability rows -> {out_path}")
    return True


def main():
    ap = argparse.ArgumentParser(description="PARITY MATRIX generator (never silent)")
    ap.add_argument("--catalogue", default="tools/capability_catalogue.json")
    ap.add_argument("--out", default="docs/PARITY_MATRIX.md")
    args = ap.parse_args()
    if not generate(args.catalogue, args.out):
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
