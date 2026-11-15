#!/usr/bin/env python3
import io, re, sys

# Print every PropertyId::of(...) / Get(PropertyId::of(...)) read site in a file
# with line numbers, so owner mismatches can be fixed in batch.
files = sys.argv[1:]
pat = re.compile(r'PropertyId::of\((.*?)\)')
for path in files:
    try:
        with io.open(path, "r", encoding="utf-8", newline="") as f:
            src = f.read()
    except Exception as e:
        print("ERR", path, e)
        continue
    print("=== " + path)
    for i, line in enumerate(src.splitlines(), 1):
        if "PropertyId::of" in line:
            m = pat.search(line)
            args = m.group(1) if m else "?"
            print("  %4d: %s" % (i, args.strip()))
