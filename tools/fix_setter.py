#!/usr/bin/env python3
import io, sys

# Convert setter/trigger docs authored in C++ string literals: the source bytes
# contain `Property=\"Prop\"` (backslash-escaped quotes inside a C++ literal).
path = sys.argv[1]
owner = sys.argv[2]
props = sys.argv[3:]
with io.open(path, "r", encoding="utf-8", newline="") as f:
    s = f.read()
for prop in props:
    old = 'Property=\\"%s\\"' % prop
    new = 'Property=\\"%s.%s\\"' % (owner, prop)
    c = s.count(old)
    s = s.replace(old, new)
    print("replaced", c, "of", prop)
with io.open(path, "w", encoding="utf-8", newline="") as f:
    f.write(s)
