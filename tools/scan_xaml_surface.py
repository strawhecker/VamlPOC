#!/usr/bin/env python3
"""
scan_xaml_surface.py — Inventory the surface of a set of WPF .xaml files.

WHY THIS EXISTS
---------------
Sprint 11a (docs/roadmap.md) must decide which WPF controls, attributes, markup
extensions and namespaces the VAML engine supports. Rather than enumerate WPF's
surface from memory (which is known to be silently incomplete), we grind the REAL
files the engine will be asked to render and produce a usage-frequency report.
This is the data-driven basis for the Sprint 11a support matrix.

WHAT IT PRODUCES
----------------
For each input folder, writes alongside it:
  1. <root>/xaml_surface_report.txt   - human-readable summary
  2. <root>/xaml_surface.csv          - machine-readable aggregate

The report classifies every element into:
  CORE   - WPF standard namespaces (candidate surface for the VAML engine)
  DESIGN - design-time-only namespaces (d:, mc:) -> parse-and-ignore
  THIRD  - external assemblies (Prism, ComponentOne, in-house Toolkit) -> out of
           scope for a runtime VAML engine

USAGE
-----
    python scan_xaml_surface.py <path-to-xaml-folder> [more-folders...]

Discovers all *.xaml recursively under each folder and aggregates across all.
"""

import argparse
import collections
import csv
import os
import re
import sys

# ----------------------------------------------------------------------------
# Namespace classification
# ----------------------------------------------------------------------------
CORE_NS_PREFIXES = [
    # Standard WPF xmlns (default 'presentation' and 'x' directive)
    "http://schemas.microsoft.com/winfx/2006/xaml/presentation",
    "http://schemas.microsoft.com/winfx/2007/xaml/presentation",
    "http://schemas.microsoft.com/winfx/2006/xaml",
    # .NET / netfx presentation
    "http://schemas.microsoft.com/netfx/2007/xaml/presentation",
    # clr-namespace mappings into the BCL / PresentationFramework (approximate)
    "clr-namespace:System.Windows",
    "clr-namespace:System.Windows.Controls",
    "clr-namespace:System.Windows.Controls.Primitives",
    "clr-namespace:System.Windows.Shapes",
    "clr-namespace:System.Windows.Media",
    "clr-namespace:System.Windows.Data",
]
DESIGN_NS_PREFIXES = [
    "http://schemas.microsoft.com/expression/blend/2008",           # d:
    "http://schemas.openxmlformats.org/markup-compatibility/2006",  # mc:
    "http://schemas.microsoft.com/xaml/behaviors",                  # i: behaviors (attach-time)
    "http://schemas.microsoft.com/expression/2010/interactivity",
]


def classify_ns(uri: str) -> str:
    if any(uri.startswith(u) for u in DESIGN_NS_PREFIXES):
        return "DESIGN"
    if any(uri.startswith(u) for u in CORE_NS_PREFIXES) or uri.startswith("clr-namespace:System."):
        return "CORE"
    return "THIRD"


# ----------------------------------------------------------------------------
# Regexes
# ----------------------------------------------------------------------------
# Namespace decl: xmlns:prefix="uri" or xmlns="uri"
NS_RE = re.compile(r'xmlns(?::([A-Za-z_][\w.-]*))?\s*=\s*"([^"]*)"')
# Open tag, capturing name + inner-attributes remainder. Handles <Button>,
# <Button.Content>, <local:VM>, <c1:FlexGrid />, and multi-line.
TAG_RE = re.compile(r"<(\s*/?\s*)([A-Za-z_][\w.:-]*)([^>]*?)(/?)>", re.S)
# Attribute: name="value"
ATTR_RE = re.compile(r'([A-Za-z_][\w.:-]*)\s*=\s*"([^"]*)"')
# Markup extension head token inside a value: {Binding ..., {StaticResource ...}
MARKUP_RE = re.compile(r"\{([A-Za-z_][\w.]*)\s*[,\}]")
IGNORE_ATTRS = {"xmlns"}


def scan_file(path: str) -> dict | None:
    nsmap = {}  # prefix -> uri
    out = {
        "elements": collections.Counter(),
        "attrs_by_elem": collections.defaultdict(collections.Counter),
        "attrs_all": collections.Counter(),
        "markup": collections.Counter(),
        "attached": collections.Counter(),
    }
    try:
        with open(path, "r", encoding="utf-8-sig", errors="replace") as f:
            text = f.read()
    except Exception as e:  # noqa: BLE001
        print(f"  !! cannot read {path}: {e}", file=sys.stderr)
        return None

    # Namespace declarations
    for m in NS_RE.finditer(text):
        nsmap[m.group(1) or ""] = m.group(2)

    # A per-file default-namespace classification so we can resolve later.
    default_ns = nsmap.get("", "")
    default_cls = classify_ns(default_ns) if default_ns else "CORE"

    def cls_of_prefix(pfx: str) -> str:
        if pfx == "":
            return default_cls
        uri = nsmap.get(pfx, "")
        if not uri:
            return "CORE"  # no mapped prefix -> treat as core (often un-prefixed local)
        return classify_ns(uri)

    for m in TAG_RE.finditer(text):
        head = m.group(1).strip()
        raw_name = m.group(2)
        inner = m.group(3) or ""
        self_close = "/" in m.group(4)
        if head.startswith("/"):
            continue  # closing tag

        # Split into prefix / local-name.
        # Element names like "Button.Content" have a '.'; prefix may be "c1:", "local:", etc.
        prefix = ""
        lname = raw_name
        if ":" in raw_name:
            prefix, lname = raw_name.split(":", 1)

        cls = cls_of_prefix(prefix)
        # Flag name for clarity in report where useful (CORE/DESIGN/THIRD)
        key = lname
        out["elements"][(lname, cls)] += 1

        for am in ATTR_RE.finditer(inner):
            aname = am.group(1)
            aval = am.group(2)
            if aname in IGNORE_ATTRS or aname.startswith("xmlns"):
                continue
            out["attrs_by_elem"][(lname, cls)][aname] += 1
            out["attrs_all"][aname] += 1
            for mm in MARKUP_RE.finditer(aval):
                out["markup"][mm.group(1)] += 1
            # attached-property form (dotted attrs that aren't x:/d: meta)
            if "." in aname and not aname.startswith(("x:", "d:", "mc:", "i:")):
                out["attached"][aname] += 1

    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("paths", nargs="+")
    args = ap.parse_args()

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

    print(f"Scanning {len(files)} .xaml files ...")
    agg = {
        "elements": collections.Counter(),
        "attrs_by_elem": collections.defaultdict(collections.Counter),
        "attrs_all": collections.Counter(),
        "markup": collections.Counter(),
        "attached": collections.Counter(),
    }
    fails = 0
    for f in files:
        r = scan_file(f)
        if r is None:
            fails += 1
            continue
        agg["elements"].update(r["elements"])
        agg["attrs_all"].update(r["attrs_all"])
        agg["markup"].update(r["markup"])
        agg["attached"].update(r["attached"])
        for k, c in r["attrs_by_elem"].items():
            agg["attrs_by_elem"][k].update(c)

    outdir = os.path.abspath(args.paths[0])
    if not os.path.isdir(outdir):
        outdir = os.path.dirname(outdir)
    rep = os.path.join(outdir, "xaml_surface_report.txt")
    csvp = os.path.join(outdir, "xaml_surface.csv")

    L = []
    L.append("=" * 78)
    L.append("XAML SURFACE REPORT")
    L.append(f"files scanned : {len(files)}  (unreadable: {fails})")
    L.append("=" * 78)

    L.append("\n--- ELEMENTS (local-name, by class then frequency) ---")
    for cls in ("CORE", "THIRD", "DESIGN"):
        L.append(f"  [{cls}]")
        for (lname, c2), c in sorted(agg["elements"].items(), key=lambda kv: -kv[1]):
            if c2 == cls:
                L.append(f"    {c:6d}  {lname}")

    L.append("\n--- MARKUP EXTENSIONS (head token) ---")
    for mk, c in agg["markup"].most_common():
        L.append(f"    {c:6d}  {{{mk}")

    L.append("\n--- ATTACHED PROPERTIES (dotted attrs) ---")
    for ap_, c in agg["attached"].most_common():
        L.append(f"    {c:6d}  {ap_}")

    L.append("\n--- ATTRIBUTES (overall, top 150) ---")
    for an, c in agg["attrs_all"].most_common(150):
        L.append(f"    {c:6d}  {an}")

    L.append("\n--- PER-ELEMENT ATTRIBUTES (CORE elements, top by attr-volume, <=30 each) ---")
    for (lname, cls), ac in sorted(agg["attrs_by_elem"].items(), key=lambda kv: -sum(kv[1].values())):
        if cls != "CORE":
            continue
        total = sum(ac.values())
        L.append(f"  [{lname}]  (total: {total})")
        for an, c in ac.most_common(30):
            L.append(f"      {c:6d}  {an}")

    with open(rep, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")
    print(f"  wrote {rep}")

    with open(csvp, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["class", "element", "attribute", "count"])
        for (lname, cls), ac in sorted(agg["attrs_by_elem"].items()):
            for an, c in ac.most_common():
                w.writerow([cls, lname, an, c])
    print(f"  wrote {csvp}")

    print("\nCORE elements by frequency:")
    for (lname, cls), c in agg["elements"].most_common():
        if cls == "CORE":
            print(f"    {c:6d}  {lname}")
    print("\nTHIRD-party elements (out of scope) by frequency:")
    for (lname, cls), c in agg["elements"].most_common():
        if cls == "THIRD":
            print(f"    {c:6d}  {lname}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
