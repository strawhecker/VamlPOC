#!/usr/bin/env python3
"""S643 DRIVER — the input-hash authority (single source of truth at run time).

The metadata `.json` sidecar carries `InputHash` — a SHA-256 over the PRINCIPLE
SOURCE FILES that determine the WPF-emitted output. This module is the ONE
run-time authority the driver (build/run_compare.cmd) uses to decide:

    * A (frame+metadata produce) SKIPS when the stamped hash still matches the
      current inputs — only B (the compare) runs;
    * A re-renders (expected + actual) + rewrites the metadata when the hash
      MISMATCHES (a source/xaml/catalogue change).

The hash is over SOURCE FILES ONLY (read in + appended, in deterministic order):
    1. the test's own .xaml  (content + SCAN DECLARATION)
    2. tools/capability_catalogue.json
    3. wpf_emitter/*.cs
    4. src/presentation/src/scan/*.{cpp,h}
    5. src/presentation/include/vaml/presentation/scan/*.h
It deliberately EXCLUDES the rebuilt executable (WPF builds churn binary bytes)
and any hand-bumped version constant (a forgotten bump = silent stale frame).

A missing PRINCIPLE file is a real gap and FAILS LOUDLY (exit 2 + stderr) —
an under-populated hash would silently mask drift.

Usage (exit codes — the .cmd driver branches on them):
    compare_hash.py hash <xaml>            -> write the current hash to stdout
    compare_hash.py stale <xaml> <sidecar> -> write "fresh" or "stale" to stdout;
       exit 0 = fresh (A skipped), 1 = stale (A re-renders), 2 = error/missing
"""

import hashlib
import json
import os
import sys


def _sorted_files(directory, patterns):
    """All files under `directory` matching any pattern, top-level only, sorted by name."""
    found = []
    if os.path.isdir(directory):
        for pat in patterns:
            for name in os.listdir(directory):
                if name.endswith(pat) and os.path.isfile(os.path.join(directory, name)):
                    found.append(os.path.join(directory, name))
    return sorted(set(found))


def principle_files(xaml_path):
    xaml = os.path.abspath(xaml_path)
    if not os.path.isfile(xaml):
        raise FileNotFoundError("test .xaml missing: " + xaml)
    root = os.path.dirname(os.path.dirname(xaml))  # <root>/vaml_ui_tester/<file>
    files = []
    # 1. the test's own .xaml
    files.append(xaml)
    # 2. the committed catalogue (single source of truth — a missing one is a real gap)
    cat = os.path.join(root, "tools", "capability_catalogue.json")
    if not os.path.isfile(cat):
        raise FileNotFoundError("principle file missing: " + cat)
    files.append(cat)
    # 3. the WPF composer sources
    files += _sorted_files(os.path.join(root, "wpf_emitter"), (".cs",))
    # 4. the scanner implementation sources
    files += _sorted_files(os.path.join(root, "src", "presentation", "src", "scan"), (".cpp", ".h"))
    # 5. the scanner public headers
    files += _sorted_files(
        os.path.join(root, "src", "presentation", "include", "vaml", "presentation", "scan"), (".h",))
    if len(files) < 4:
        raise RuntimeError("input-hash principle set is under-populated")
    return files


def compute(xaml_path):
    digest = hashlib.sha256()
    for f in principle_files(xaml_path):
        with open(f, "rb") as fh:
            for chunk in iter(lambda: fh.read(1 << 20), b""):
                digest.update(chunk)
    return digest.hexdigest()


def _read_stamped_hash(sidecar_path):
    with open(sidecar_path, "rb") as fh:
        data = json.load(fh)
    h = data.get("InputHash")
    if not h:
        raise KeyError("sidecar has no InputHash (older artifact — treat as STALE so A re-renders)")
    return h


def main(argv):
    if len(argv) < 3:
        sys.stderr.write("usage: compare_hash.py hash <xaml> | stale <xaml> <sidecar>\n")
        return 2
    cmd = argv[1]
    try:
        if cmd == "hash":
            print(compute(argv[2]))
            return 0
        if cmd == "stale":
            xaml, sidecar = argv[2], argv[3]
            if not os.path.isfile(sidecar):
                print("stale")   # no artifact at all -> A must re-render
                return 1
            stamped = _read_stamped_hash(sidecar)
            print("fresh" if stamped == compute(xaml) else "stale")
            return 0 if stamped == compute(xaml) else 1
        sys.stderr.write("unknown command: " + cmd + "\n")
        return 2
    except Exception as ex:  # noqa: BLE001 — the driver gets a loud, typed failure
        sys.stderr.write("compare_hash error: {}\n".format(ex))
        return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
