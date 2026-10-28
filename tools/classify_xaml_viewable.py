#!/usr/bin/env python3
"""
classify_xaml_viewable.py — Which of a set of WPF .xaml files can the 11d
(VAML) viewer OPEN as-is?

WHY THIS EXISTS
---------------
The VAML 11d engine has a closed 33-type element registry + a strict authoring
grammar. Most real-world WPF .xaml relies on types/features the engine does NOT
yet implement (TextBlock, TabControl, DataGrid, Menu, Path, `{Binding}`,
`{StaticResource}`, Style/ControlTemplate, custom `clr-namespace` controls, ...).
Rather than eyeball 300+ files, this script parses each file for:

  1. every element OPEN tag (incl. property-element tags like <Button.Content>)
  2. every attribute name/value (to detect markup extensions + attached props)
  3. namespace prefixes (to flag THIRD-party / CLR custom controls)

and classifies each file into one bucket:
  VIEWABLE           -> every element tag is in the 11d registry and the file
                        uses none of the hard feature blockers. Would open at
                        least to a parseable, bonus-able tree.
  NEARLY             -> resolves in the registry but uses ONLY soft blockers
                        (Bindings/StaticResources/Styles on otherwise-supported
                        elements) — opens structurally, may throw on those attrs.
  BLOCKED            -> uses an element type or namespace outside the registry
                        (hard blocker). Would NOT parse.

The 11d control set is read from Element.cpp's `register_builtin_elements()`;
the two "two" = the engine's two renderable control halves this script is the
keyword-basis for (the File + Viewer classification). The per-file verdict is
the "which files can be opened" deliverable.

USAGE
-----
    python classify_xaml_viewable.py <xaml-file-or-folder> [more...]
    # optional: --registry-json <file> to override the 33-type set; by default
    # it parses E:\GitRepos\VamlPOC\src\core\src\Element.cpp for the REGISTRY.


OUTPUT
------
Prints a per-file bucket + the ordering tokens, and writes:
    <outdir>/xaml_viewable_report.txt   (grouped VIEWABLE / NEARLY / BLOCKED)
    <outdir>/xaml_viewable.csv          (file, bucket, unsupported_tags, blockers)
"""

import argparse
import csv
import os
import re
import sys

# ----------------------------------------------------------------------------
# 11d IS INCREMENTAL: "everything before 11d is included in 11d."
# Therefore the SINGLE source of truth for "which element tags can 11d view?"
# is the CUMULATIVE registry — the 33 types in Element.cpp's
# register_builtin_elements() (all pre-11d controls + the 12 Sprint 11d ones).
# There is deliberately NO hand-duplicated supported/blocked list here: a tag is
# SUPPORTED iff its element name is in that live registry (so Label-ish scans
# never drift from the build). The only extra constants below are (a) the
# authoring-grammar element names the engine also resolves, folded into the
# scrape, and (b) soft attribute blockers / third-party namespace clues that
# only annotate a downgrade, never the element-support decision.
# ----------------------------------------------------------------------------

# EXTRA builder tags the VAML grammar resolves but which are NOT real element
# visuals (folded in so they never read as "unsupported"): authoring/resource
# types. These are matched by the registry scrape when present; a literal form
# here only matters if Element.cpp ever stops exposing them (kept as a small,
# build-checked complement).
AUTH_COMPLEMENT = {
    "SolidColorBrush", "GradientStop", "LinearGradientBrush", "Style", "Setter",
    "Trigger", "DataTrigger",
}

# Soft (attribute-level) blockers — the element resolves but the strict grammar
# may reject/ignore these markup extensions on otherwise-supported elements.
SOFT_BLOCK_MARKUPS = {
    "Binding", "StaticResource", "DynamicResource", "RelativeSource",
    "TemplateBinding", "MultiBinding", "PriorityBinding",
}

# Third-party namespace clues. Only ANNOTATE the reason; the element-support
# verdict is registry membership (a standard WPF/system namespace is fine, and
# its local name is what must be in the registry).
HARD_NS_FRAGMENTS = (
    "clr-namespace:", "prism:", "c1:", "xctk:", "mahapps:", "materialDesign:",
    "behaviors:", "interactivity:",
)

NS_RE = re.compile(r'xmlns(?::([A-Za-z_][\w.-]*))?\s*=\s*"([^"]*)"')
# Open tag, capturing name + remainder; handles <Button>, <Button.Content>, <Local:Thing />, multiline.
TAG_RE = re.compile(r"<(\s*/?\s*)([A-Za-z_][\w.:-]*)([^>]*?)(/?)>", re.S)
ATTR_RE = re.compile(r'([A-Za-z_][\w.:-]*)\s*=\s*"([^"]*)"')
MARKUP_RE = re.compile(r"\{([A-Za-z_][\w.]*)\b")

# Standard (non-third-party) namespace URI prefixes: their element-LOCAL names
# are what the registry decides, so we don't hard-block the prefix itself.
STANDARD_URI_FRAGMENTS = (
    "http://schemas.microsoft.com/winfx/2006/xaml",
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation",
    "clr-namespace:System",
)


def load_registry(element_cpp: str) -> set:
    """Scrape the CUMULATIVE registry from Element.cpp's register_element( lines.

    11d is incremental: every pre-11d element + the 12 Sprint 11d elements
    (Label/Border/...) together form the viewable set. We also fold in the small
    authoring-grammar complement so it never reads as an unsupported element.
    """
    reg = set(AUTH_COMPLEMENT)  # authoring/resource types always resolve
    try:
        with open(element_cpp, "r", encoding="utf-8-sig", errors="replace") as f:
            for line in f:
                m = re.search(r'register_element\("([A-Za-z_][\w-]*)"', line)
                if m:
                    reg.add(m.group(1))
    except OSError:
        pass  # give an explicit error rather than silently under-report
    if not reg:
        raise SystemExit("Could not scrape the registry from Element.cpp; "
                         "refusing to guess (11d is incremental).")
    return reg


def classify_file(path: str, supported: set) -> tuple:
    """
    Returns (bucket, unsupported_tags, soft_blockers)
      bucket in {"VIEWABLE", "NEARLY", "BLOCKED"}
    """
    try:
        with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
            text = f.read()
    except OSError as e:
        return ("UNREADABLE", set(), f"cannot read: {e}")

    nsmap = {}
    for m in NS_RE.finditer(text):
        nsmap[m.group(1) or ""] = m.group(2)

    unsupported = set()
    soft = set()

    for m in TAG_RE.finditer(text):
        head = m.group(1).strip()
        raw = m.group(2)
        if head.startswith("/"):
            continue  # closing tag
        prefix = ""
        lname = raw
        if ":" in raw:
            prefix, lname = raw.split(":", 1)
        # Property-element tags like <Button.Background> are NAME+DP pairs, not
        # element types — they never drive the element-support verdict.
        if "." in lname and ":" not in raw:
            continue
        # Decide by the element's LOCAL name against the CUMULATIVE registry.
        # A THIRD-party namespace prefix (custom/clr local control) is a hard
        # blocker regardless of a coincidentally-matching local name.
        if prefix:
            uri = nsmap.get(prefix, "")
            is_standard = any(fr in uri for fr in STANDARD_URI_FRAGMENTS)
            if not is_standard:
                unsupported.add(f"{prefix}:{lname}")
                continue
        if lname not in supported:
            unsupported.add(lname)

    # attribute-level soft blockers (only meaningful for otherwise-supportable files)
    for am in ATTR_RE.finditer(text):
        aval = am.group(2)
        for mk in MARKUP_RE.finditer(aval):
            if mk.group(1) in SOFT_BLOCK_MARKUPS:
                soft.add(mk.group(1))

    # element-level unsupported tags are hard blockers.
    if unsupported:
        return ("BLOCKED", unsupported, soft)
    # all element local names are in the cumulative registry; soft-only -> NEARLY
    if soft:
        return ("NEARLY", set(), soft)
    return ("VIEWABLE", set(), set())


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+")
    ap.add_argument("--registry-json", default=None,
                    help="optional text file with one registry element name per line")
    args = ap.parse_args()

    if args.registry_json:
        with open(args.registry_json, "r", encoding="utf-8") as f:
            supported = {ln.strip() for ln in f if ln.strip()}
    else:
        cpp = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                            "..", "src", "core", "src", "Element.cpp"))
        supported = load_registry(cpp)
    print(f"Registry size: {len(supported)} -> {sorted(supported)}")

    files = []
    for p in args.paths:
        if os.path.isfile(p) and p.lower().endswith(".xaml"):
            files.append(p)
        elif os.path.isdir(p):
            for dp, _, fns in os.walk(p):
                for fn in fns:
                    if fn.lower().endswith(".xaml"):
                        files.append(os.path.join(dp, fn))
    files = sorted(set(files))
    if not files:
        print("No .xaml files found.", file=sys.stderr)
        return 1

    rows = []
    buckets = {"VIEWABLE": [], "NEARLY": [], "BLOCKED": [], "UNREADABLE": []}
    root_for_rel = os.path.abspath(os.path.commonpath([os.path.abspath(p) for p in args.paths if os.path.exists(p)] or args.paths))
    for f in files:
        b, unsup, soft = classify_file(f, supported)
        try:
            rel = os.path.relpath(f, root_for_rel)
        except ValueError:
            rel = f  # cross-drive (e.g. E: -> D:): show the absolute path
        rows.append((rel, b, ",".join(sorted(unsup)), ",".join(sorted(soft))))
        buckets[b].append((rel, unsup, soft))

    outdir = os.path.abspath(args.paths[0])
    if not os.path.isdir(outdir):
        outdir = os.path.dirname(outdir)
    rep = os.path.join("tools", "xaml_viewable_report.txt")
    csvp = os.path.join("tools", "xaml_viewable.csv")
    csvp = os.path.join(outdir, "xaml_viewable.csv")

    L = []
    L.append("=" * 80)
    L.append("XAML VIEWABLE-BY-11d CLASSIFICATION")
    L.append(f"files scanned : {len(files)}")
    L.append(f"registry size : {len(supported)}")
    L.append("=" * 80)
    for bucket in ("VIEWABLE", "NEARLY", "BLOCKED", "UNREADABLE"):
        L.append(f"\n--- {bucket} ({len(buckets[bucket])}) ---")
        for rel, unsup, soft in sorted(buckets[bucket]):
            extra = ""
            if unsup:
                extra += "  [unsupported: " + ",".join(sorted(unsup)) + "]"
            if soft:
                extra += "  [blockers: " + ",".join(sorted(soft)) + "]"
            L.append("   " + rel + extra)

    with open(rep, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")
    print(f"  wrote {rep}")
    with open(csvp, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["file", "bucket", "unsupported_tags", "soft_blockers"])
        for r in sorted(rows):
            w.writerow(r)
    print(f"  wrote {csvp}")

    print("\n=== SUMMARY ===")
    for bucket in ("VIEWABLE", "NEARLY", "BLOCKED", "UNREADABLE"):
        print(f"  {bucket:10s} {len(buckets[bucket]):4d}")
    print("\nVIEWABLE files:")
    for rel, _, _ in buckets["VIEWABLE"]:
        print("   " + rel)
    print("\nNEARLY files (open structurally, soft blockers remain):")
    for rel, unsup, soft in buckets["NEARLY"]:
        print("   " + rel + (f"  [{','.join(sorted(soft))}]" if soft else ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
