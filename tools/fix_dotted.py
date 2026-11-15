#!/usr/bin/env python3
"""S803 helper: convert bare synthetic-DP attribute authoring to the dotted
`Owner.Prop` composite form in reader test documents.

WPF-faithful S803 resolution: a BARE attribute key resolves by walking the
element's DECLARED owner chain (most-derived first). A synthetic DP registered
under a test-only owner token (e.g. "S095_Owner") is NOT in any element's chain,
so a bare key never resolves -> UnknownProperty. The composite-faithful authoring
form is the dotted `Owner.Prop` key, which the reader resolves directly via
TryGet(tail, owner) (exactly how WPF does `Grid.Row` / `DockPanel.Dock`).

This script rewrites occurrences of `Prop="` (a bare attribute authoring site)
into `Owner.Prop="` for each (ownerToken, [prop,...]) mapping. It deliberately
only matches the `Name="` authoring pattern so registration lines
(`Register("Name", ...)`) and read lines (`PropertyId::of("Name","Owner")`)
are untouched.

Usage:
  python tools/fix_dotted.py <file> <ownerToken> <prop1> <prop2> ...
"""
import io
import re
import sys


def main() -> int:
    path, owner = sys.argv[1], sys.argv[2]
    props = sys.argv[3:]
    with io.open(path, "r", encoding="utf-8", newline="") as f:
        src = f.read()
    n = 0
    for prop in props:
        pattern = prop + '="'
        repl = owner + "." + prop + '="'
        c = src.count(pattern)
        src = src.replace(pattern, repl)
        n += c
    with io.open(path, "w", encoding="utf-8", newline="") as f:
        f.write(src)

    # Verify we did not corrupt reads (PropertyId::of("Prop","Owner") must stay
    # two separate args — the prop arg must NOT have gained a dot).
    double = re.findall(r'of\("' + owner + r'\.[A-Za-z0-9]+",', src)
    if double:
        print("WARN: read sites now dotted (unexpected):", double)
    print(f"replaced {n} bare '{owner}' attr sites -> dotted in {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
