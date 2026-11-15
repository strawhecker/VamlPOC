#!/usr/bin/env python3
"""gap_wpf_exception_catalog.py — catalog every public exception in the WPF source.

The WPF-corpus exception-surface tool (the 11ak "VAML exception surface" directive).
Reads the REAL WPF C# source at E:\\GitRepos\\wpf, finds every *Exception.cs
declaration, extracts (namespace, public class name, base type, serializable
ctors, distinct user-facing members), tags each as VAML-facing (the classes a
VAML app maps onto VAML exception types / a compat shim must be able to catch),
and emits ONE artifact:

  docs/WPF_EXCEPTION_CATALOG.md  — the committed engine contract: the living
  reference a VAML author diffs against to decide which WPF-named exception
  types VAML must expose (off the common `vaml::exception` base) so user code
  written for WPF `catch` blocks keeps compiling.

This is a GENERATOR (the gap_dp_daggen.py / gap_wpf_enum_catalog.py convention).
Re-run whenever E:\\GitRepos\\wpf advances; the OUTPUT (docs/WPF_EXCEPTION_CATALOG.md)
is committed, NOT this script's stdout.

Note on base families (System.ArgumentException, System.InvalidOperationException,
System.NotSupportedException, ...): these are .NET framework types, not *.cs files
in this repo, so they are NOT scanned. They are recorded in the catalog as a
static KNOWN-BASE set — the catch types a VAML app is documented to see, which the
catch-all `vaml::exception` base must serve. See KNOWN_BASE_CATCH_TYPES below.
"""

import os
import re
import sys

WPF_SRC = r"E:\GitRepos\wpf\src\Microsoft.DotNet.Wpf\src"
VAML_ROOT = r"E:\GitRepos\VamlPOC"
OUT_DOC = os.path.join(VAML_ROOT, "docs", "WPF_EXCEPTION_CATALOG.md")

# The .NET framework base-type families WPF throws (or derives from) that a
# compat shim must be able to catch but which have no *.cs in the WPF repo.
# These are recorded statically, not scanned.
KNOWN_BASE_CATCH_TYPES = [
    "System.Exception",
    "System.SystemException",
    "System.ApplicationException",
    "System.ArgumentException",
    "System.ArgumentNullException",
    "System.ArgumentOutOfRangeException",
    "System.InvalidOperationException",
    "System.NotSupportedException",
    "System.NotImplementedException",
    "System.FormatException",
    "System.InvalidCastException",
    "System.IO.IOException",
    "System.Xml.XmlException",
    "System.Xaml.XamlException",
]

# Which *Exception.cs files are genuinely part of the application-facing WPF UI
# surface (PresentationCore/Framework/System.Xaml/WindowsBase + automation),
# vs compiler/internal/print/xps tooling that a VAML app will never be expected
# to catch cross-language. Kept explicit (not guessed) so the catalog's
# VAML-facing column is auditable rather than auto-classified.
NON_VAML_FACING_FILE_FRAGMENTS = [
    "ReachFramework",       # XPS packaging/serialization (document-print toolchain)
    "PrintSystemExceptions",# ReachFramework print-system interop
    "PresentationUI",       # XPS viewer
    "PresentationBuildTasks",# markup compiler task
    "PrimaryInteropAssembly",# COM interop
]

CLASS_BODY_RE = re.compile(
    r'\bpublic\s+(?:sealed\s+|abstract\s+)?class\s+([A-Za-z_][A-Za-z0-9_]*)\s*'
    r'(?::\s*([A-Za-z0-9_.]+))?\s*\{')
NAMESPACE_RE = re.compile(r'\bnamespace\s+([A-Za-z_][A-Za-z0-9_.]*)')
PUBLIC_MEMBER_RE = re.compile(
    r'\bpublic\s+(?!static|override|sealed|class|void|readonly|const)'
    r'[A-Za-z_][A-Za-z0-9_.<>\[\],\s]*\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?=[({=])')
PUBLIC_CTOR_RE = re.compile(
    r'\bpublic\s+([A-Za-z_][A-Za-z0-9_]*)\s*\(')


def strip_cs_artifacts(text):
    text = re.sub(r'(?m)^\s*#[^\n]*\n?', '', text)
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    text = re.sub(r'//[^\n]*', '', text)
    text = re.sub(r'\[[^\[\]{}]*\]', '', text, flags=re.S)
    return text


def class_bodies(text):
    """Yield (name, base, body) for each top-level public class, using brace
    matching so a multi-class file (e.g. System.Xaml\\XamlException.cs) gets each
    class's OWN body — its own members — not the whole file's."""
    idx = 0
    while True:
        m = CLASS_BODY_RE.search(text, idx)
        if not m:
            return
        name = m.group(1)
        base = m.group(2) or 'object'
        open_brace = m.end() - 1
        depth = 1
        i = open_brace + 1
        n = len(text)
        while i < n and depth > 0:
            if text[i] == '{':
                depth += 1
            elif text[i] == '}':
                depth -= 1
            i += 1
        body = text[open_brace + 1: i - 1]
        yield name, base, body
        idx = i


def is_vaml_facing(path):
    pl = path.replace('\\', '/')
    return not any(frag in pl for frag in NON_VAML_FACING_FILE_FRAGMENTS)


def scan_exceptions():
    """Return dict keyed by class name -> info {namespace, base, ctor_count,
    public_members, file, vaml_facing}."""
    result = {}
    files = 0
    for dirpath, _, filenames in os.walk(WPF_SRC):
        for fn in filenames:
            if not fn.endswith('.cs') or not fn.endswith('Exception.cs'):
                continue
            path = os.path.join(dirpath, fn)
            try:
                raw = open(path, encoding='utf-8', errors='replace').read()
            except Exception:
                continue
            files += 1
            rp = os.path.relpath(path, WPF_SRC)
            text = strip_cs_artifacts(raw)
            ns_m = NAMESPACE_RE.search(text)
            namespace = ns_m.group(1) if ns_m else ''
            for name, base, body in class_bodies(text):
                # public ctor of THIS class (same name) inside its own body
                ctor_count = len(PUBLIC_CTOR_RE.findall(body)) or 0
                # distinct public members declared in THIS class's own body only
                public_members = sorted(set(PUBLIC_MEMBER_RE.findall(body)))
                result[name] = {
                    'namespace': namespace,
                    'base': base,
                    'ctor_count': ctor_count,
                    'members': public_members,
                    'file': rp,
                    'vaml_facing': is_vaml_facing(rp),
                }
    return result, files


def member_anchor(name, namespace):
    return name  # simple; the md uses `full` col for fidelity


def main():
    excs, files = scan_exceptions()

    lines = []
    lines.append("# WPF_EXCEPTION_CATALOG.md — full exception inventory (engine contract)")
    lines.append("")
    lines.append("> **gap_wpf_exception_catalog.py (generated).** The complete catalog of every `public class")
    lines.append("> *Exception` declared in the REAL WPF source (`E:\\GitRepos\\wpf` — PresentationCore,")
    lines.append("> PresentationFramework, System.Xaml, WindowsBase, UIAutomation, and the document/print")
    lines.append("> assemblies). This is the living reference a VAML author diffs against.")
    lines.append(">")
    lines.append("> Generated by re-running `python gap_wpf_exception_catalog.py` whenever WPF advances; the")
    lines.append("> OUTPUT is committed, NOT the stdout. Do NOT hand-edit generated rows; re-run the generator.")
    lines.append(">")
    lines.append(f"> **Scope:** `{files}` `*Exception.cs` files scanned; **{len(excs)} exception classes** cataloged.")
    lines.append("")
    lines.append("## How to read this (the 11ak contract)")
    lines.append("")
    lines.append("- **VAML-facing=true** means the exception is part of the application-facing WPF UI surface")
    lines.append("  and MUST be mapped to a VAML exception type derived from the common `vaml::exception` base,")
    lines.append("  so user code written for a WPF `catch (InvalidOperationException)` / `catch (XamlParseException)`")
    lines.append("  keeps compiling against VAML.")
    lines.append("- **VAML-facing=false** (XPS/Reach/print/task-tooling) is NOT expected to be caught cross-language;")
    lines.append("  it is recorded here for completeness, not scheduled for a VAML class.")
    lines.append("- **Known base families** (the .NET framework catch types at the bottom) are recorded statically;")
    lines.append("  they are not `*.cs` in the repo. The catch-all `vaml::exception` base must serve them.")
    lines.append("")
    lines.append("## Full exception catalog")
    lines.append("")
    lines.append("| Exception | Namespace | Base | File | VAML-facing |")
    lines.append("|-----------|-----------|------|------|-------------|")
    for name in sorted(excs):
        e = excs[name]
        facing = "✅" if e['vaml_facing'] else "—"
        lines.append(
            f"| `{name}` | `{e['namespace']}` | `{e['base']}` | `{e['file']}` | {facing} |")
    lines.append("")
    lines.append("## Known .NET base catch families (recorded, not scanned)")
    lines.append("")
    lines.append("These framework base types have no `*Exception.cs` in the WPF repo; a compat shim / the")
    lines.append("`vaml::exception` base must still be catchable-as (or derivable-as) these:")
    lines.append("")
    lines.append("| Base type | Notes |")
    lines.append("|-----------|-------|")
    for b in KNOWN_BASE_CATCH_TYPES:
        lines.append(f"| `{b}` | documented catch surface |")
    lines.append("")
    lines.append("## Notable members carried by VAML-facing exceptions")
    lines.append("")
    lines.append("The observable contract a VAML exception class must honor is the **name** + the **distinct")
    lines.append("public members** (so WPF `catch` blocks / member access keep compiling). The .NET base")
    lines.append("**type identity** (e.g. deriving from `System.InvalidOperationException`) cannot be mirrored")
    lines.append("in C++ — but the catchable cross-language surface is the name + members, which C++ CAN match")
    lines.append("exactly by declaring `class InvalidOperationException : vaml::exception` and")
    lines.append("`class ResourceReferenceKeyNotFoundException : InvalidOperationException`. The base-type")
    lines.append("column above is informational (which .NET family WPF derives from), recorded so the shim knows")
    lines.append("what a user is likely to `catch`.")
    lines.append("")
    lines.append("| Exception | Public ctor overloads | Public members |")
    lines.append("|-----------|-----------------------|----------------|")
    for name in sorted(excs):
        e = excs[name]
        if not e['vaml_facing']:
            continue
        members = ", ".join(f"`{m}`" for m in e['members']) or "(none)"
        lines.append(f"| `{name}` | {e['ctor_count']} | {members} |")
    lines.append("")

    os.makedirs(os.path.dirname(OUT_DOC), exist_ok=True)
    with open(OUT_DOC, 'w', encoding='utf-8') as fh:
        fh.write("\n".join(lines))

    print(f"scanned {files} *Exception.cs files -> {len(excs)} exception classes")
    print(f"wrote {OUT_DOC}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
