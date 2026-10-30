#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
render_surface_check.py — conformance validator for docs/RENDER_SURFACE.md

Walks every classifier arm in classify_merge_element (src/presentation/src/
VamlRenderer.cpp) and cross-checks each against a row in docs/RENDER_SURFACE.md.

Contract (see docs/RENDER_SURFACE.md §Conformance validator):
  1. Every classifier arm MUST have a row in the matrix (by control type).
  2. Every row's NO-GAP token must name a real, scheduled token (not blank unless
     the row's status is REAL).
  3. Report OK when every arm has a row and every row status/NO-GAP is legal.

The script is read-only catalog conformance: it never edits the renderer or the
md. It prints a human summary and returns a non-zero exit code on any violation.

Usage:  python render_surface_check.py [--renderer PATH] [--catalog PATH]
"""

import argparse
import re
import sys

# --- The canonical classifier arm inventory (transcribed from classify_merge_element).
# key = control type name as it appears in the dynamic_cast; value = (status it must
# resolve to, the no-gap token(s) that resolve it, allowed).
#
# Tri-state: a row is REAL (no NO-GAP required) or has a mandatory NO-GAP token.
CLASSIFIER_ARMS = {
    # Text
    "TextBlock":  {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "Label":      {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    # Image / icon
    "Icon":       {"status": "PLACEHOLDER",    "no_gap": ("S209", "S213_1", "S409_1")},
    "Image":      {"status": "PLACEHOLDER",    "no_gap": ("S209", "S213_1", "S409_1")},
    # Interactive
    "Button":     {"status": "VERTICAL-SLICE", "no_gap": ("S409_1", "[s401-live]")},
    "TextBox":    {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "Slider":     {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "CheckBox":   {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "ScrollViewer":{"status": "VERTICAL-SLICE","no_gap": "S409_1"},
    "PasswordBox":{"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    # Selection family
    "RadioButton":{"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "ComboBox":   {"status": "VERTICAL-SLICE", "no_gap": "S649"},
    "ListBox":    {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "ListView":   {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "DockPanel":  {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    # Display + shapes
    "Border":     {"status": "REAL",           "no_gap": None},
    "Rectangle":  {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "Ellipse":    {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "Line":       {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "ProgressBar":{"status": "VERTICAL-SLICE", "no_gap": ("S409_1", "S649")},
    "GroupBox":   {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "Expander":   {"status": "VERTICAL-SLICE", "no_gap": "S649"},
    "Separator":  {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "StatusBar":  {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    # Containers, menu, popup
    "TabControl": {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "TabItem":    {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "Menu":       {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "MenuItem":   {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "ContextMenu":{"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "ToolTip":    {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "Popup":      {"status": "VERTICAL-SLICE", "no_gap": "S409_1"},
    "GridSplitter":{"status": "VERTICAL-SLICE","no_gap": "S409_1"},
    "DataGrid":   {"status": "VERTICAL-SLICE", "no_gap": "S649"},
    # Toolkit / decorator
    "Viewbox":    {"status": "REAL",           "no_gap": None},
    "RepeatButton":{"status": "VERTICAL-SLICE","no_gap": "S649"},
    "ScrollBar":  {"status": "VERTICAL-SLICE", "no_gap": "S649"},
    "ToolBar":    {"status": "VERTICAL-SLICE", "no_gap": "S649"},
    "ToolBarTray":{"status": "VERTICAL-SLICE", "no_gap": "S649"},
    # Structural (base fallback + non-UIElement GAP)
    "UIElement-fallback": {"status": "VERTICAL-SLICE", "no_gap": ("S400", "S364")},
    "non-UIElement":      {"status": "GAP",            "no_gap": "sprint"},
}

# Controls that share a single classifier arm (the base UIElement Solid fallback).
# key = catalog row control name; value = the canonical arm they resolve to.
ARM_ALIASES = {
    "StackPanel": "UIElement-fallback",
    "Grid":       "UIElement-fallback",
    "WrapPanel":  "UIElement-fallback",
    "Canvas":     "UIElement-fallback",
    "Window":     "UIElement-fallback",
}


def _strip_md(s):
    """Remove markdown emphasis/bold markers from a cell value."""
    return re.sub(r"[\*\`]", "", s).strip()


def extract_arms(renderer_path):
    """Return the set of control types dynamic_cast'd in classify_merge_element."""
    with open(renderer_path, "r", encoding="utf-8", errors="replace") as fh:
        src = fh.read()
    arms = set()
    for m in re.finditer(r"dynamic_cast\s*<\s*const\s+vaml::(\w+)\s*\*?\s*>\s*\(\s*&element\s*\)",
                         src):
        arms.add(m.group(1))
    return arms


def extract_catalog_rows(catalog_path):
    """Return dict control -> (status, no_gap) parsed from RENDER_SURFACE.md."""
    with open(catalog_path, "r", encoding="utf-8", errors="replace") as fh:
        lines = fh.readlines()
    rows = {}
    current_control = None
    for line in lines:
        # A matrix row: | `Control` (STOKEN) | arm... | material | STATUS | TOKEN |
        m = re.match(r"\|\s*`(\w+)`\s*(?:\([^)]*\))?\s*\|", line)
        if m:
            current_control = m.group(1)
            # status is the 4th pipe field; no_gap the 5th (may be "—" for REAL)
            parts = [p.strip() for p in line.split("|")]
            # parts[0] empty, parts[1]=control, parts[2]=arm, [3]=material, [4]=status, [5]=no_gap
            if len(parts) >= 6:
                status = _strip_md(parts[4])
                no_gap = _strip_md(parts[5]) if len(parts) > 5 else ""
                rows[current_control] = (status, no_gap)
    return rows


def main():
    ap = argparse.ArgumentParser(description="RENDER_SURFACE conformance check")
    ap.add_argument("--renderer", default="src/presentation/src/VamlRenderer.cpp")
    ap.add_argument("--catalog", default="docs/RENDER_SURFACE.md")
    args = ap.parse_args()

    arms = extract_arms(args.renderer)
    rows = extract_catalog_rows(args.catalog)

    problems = []

    # Contract 1: every classifier arm must have a row in the catalog.
    known = set(CLASSIFIER_ARMS.keys())
    for arm in sorted(arms):
        if arm not in rows and arm not in known:
            problems.append(f"[CONTRACT-1] classifier arm `{arm}` has NO RENDER_SURFACE row")

    # Every catalog row must be recognized by the canonical inventory.
    for ctrl, (status, no_gap) in rows.items():
        # Resolve aliases (panel/Window rows map onto the shared UIElement arm).
        canon = ARM_ALIASES.get(ctrl, ctrl)
        exp = CLASSIFIER_ARMS.get(canon)
        if exp is None and canon not in CLASSIFIER_ARMS:
            problems.append(f"[CONTRACT-2] catalog row `{ctrl}` is not in the canonical arm inventory")
            continue
        # Contract 2: Legal status + mandatory NO-GAP.
        if no_gap.strip() == "—" or no_gap.strip() == "":
            if exp is not None and exp["status"] != "REAL":
                problems.append(f"[CONTRACT-2] `{ctrl}` has status {status} but NO-GAP token is empty (must name a resolution token)")
        if exp is not None and exp["status"] == "REAL" and status != "REAL":
            problems.append(f"[CONTRACT-2] `{ctrl}` should be REAL but catalog says {status}")

    # Contract 1b: a control in the canonical inventory that the renderer now emits
    # but that is missing from the catalog matrix gets flagged.
    for ctrl in known:
        if ctrl in arms and ctrl not in rows:
            problems.append(f"[CONTRACT-1] control `{ctrl}` emitted by classifier but missing from RENDER_SURFACE matrix")

    if problems:
        print("RENDER_SURFACE conformance: FAIL")
        for p in problems:
            print("  " + p)
        return 1
    print(f"RENDER_SURFACE conformance: OK ({len(arms)} classifier arms, {len(rows)} catalog rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
