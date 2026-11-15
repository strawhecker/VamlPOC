#!/usr/bin/env python3
import io, sys
# Attempt an in-place byte-identical write; catches the IDE user-mapped-section
# lock that a read-only 'r+' probe misses.
for path in sys.argv[1:]:
    try:
        with io.open(path, "r", encoding="utf-8", newline="") as f:
            data = f.read()
        with io.open(path, "w", encoding="utf-8", newline="") as f:
            f.write(data)
        print("WRITABLE", path)
    except Exception as e:
        print("LOCKED  ", path)
