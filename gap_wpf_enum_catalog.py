#!/usr/bin/env python3
"""gap_wpf_enum_catalog.py — catalog every enum in the WPF source + diff ordinals against VAML.

The WPF-corpus enum-drift-reduction tool (Sprint 11p-adjacent / the "catalog all
enums" directive). Reads the REAL WPF C# source at E:\\GitRepos\\wpf, extracts every
enum declaration (name, base type, [Flags], member list, and each member's
WPF-true ordinal), computes the ordinals (sequential, explicit =N, Flags
bit-shift, shifted continuation), and emits TWO artifacts:

  1. docs/WPF_ENUM_CATALOG.md  — the committed engine contract: the full list of
     WPF enums and their ordinals (a living reference a VAML author diffs against).
  2. A DRIFT section in that same doc: for every enum domain VAML currently
     registers (EnumDomainRegistry.cpp), compare VAML's registered ordinals against
     the WPF ground truth and flag ANY mismatch (ordinal drift) or missing enum.

This is a GENERATOR (the gap_dp_daggen.py convention). Re-run whenever
E:\\GitRepos\\wpf advances; the OUTPUT (docs/WPF_ENUM_CATALOG.md) is committed,
NOT this script's stdout.
"""

import os
import re
import sys

WPF_SRC = r"E:\GitRepos\wpf\src\Microsoft.DotNet.Wpf\src"
VAML_ROOT = r"E:\GitRepos\VamlPOC"
OUT_DOC = os.path.join(VAML_ROOT, "docs", "WPF_ENUM_CATALOG.md")

# The PresentationCore/PresentationFramework/WindowsBase are the VAML-facing
# assemblies (the controls + the types they author). Extensions/UIElement etc.
# are out of scope; these three are the UI surface.
SCAN_ROOTS = [
    os.path.join(WPF_SRC, "PresentationCore"),
    os.path.join(WPF_SRC, "PresentationFramework"),
    os.path.join(WPF_SRC, "WindowsBase"),
]


def strip_cs_artifacts(text):
    """Remove comments, attributes, and C# preprocessor directives so the enum
    body is clean (attributes may contain braces? no — attributes use [ ] and
    parens; guard both). The preprocessor (#if/#endif/#else/#region/#endregion/
    #pragma/#define...) MUST be removed FIRST: a '#if SYMBOL' region can split
    an enum member's ordinal assignment across a non-comma boundary, which would
    otherwise corrupt member parsing (e.g. WindowState's THEATRE_FULLSCREEN block
    after 'Maximized = 2' with no trailing comma)."""
    # preprocessor directives: remove entire lines starting with '#'
    text = re.sub(r'(?m)^\s*#[^\n]*\n?', '', text)
    # remove /* */ comments
    text = re.sub(r'/\*.*?\*/', '', text, flags=re.S)
    # remove // comments
    text = re.sub(r'//[^\n]*', '', text)
    # attributes: [ Flags ], [ ... ] possibly multiline — but never contain { }
    text = re.sub(r'\[[^\[\]{}]*\]', '', text, flags=re.S)
    return text


# An enum decl: `[Flags]? enum Name [: Base] { ... }` with a balanced body.
# Use a brace matcher rather than a naive regex so nested braces (e.g. in
# attribute/expression edge cases) don't break us.
ENUM_HEAD_RE = re.compile(
    r'\benum\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?::\s*([A-Za-z0-9_]+))?\s*\{')

# A single member: "Name" or "Name = expr". The expr may contain commas (e.g.
# `Name = (int)Some(x, y)`), so we cannot split purely on commas at the top
# level. We instead parse member-by-member with brace/paren balance + split on
# the top-level comma.
MEMBER_RE = re.compile(r'^\s*([A-Za-z_][A-Za-z0-9_]*)\s*(?:=\s*(.*?))?\s*$')


def find_enum_bodies(text):
    """Yield (name, base, body) for each enum, using a brace matcher."""
    idx = 0
    while True:
        m = ENUM_HEAD_RE.search(text, idx)
        if not m:
            break
        name = m.group(1)
        base = m.group(2) or 'int'
        open_brace = m.end() - 1  # index of '{'
        # match to closing brace
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


def split_top_level(body):
    """Split on top-level commas AND newlines (respecting (), [], {} nesting +
    string). Newline-splitting handles the C# `#if SYMBOL` pattern where a member
    with NO trailing comma is directly followed by another member on the next
    line (e.g. WindowState `Maximized = 2` then `#if THEATRE_FULLSCREEN` ...
    `Theatre = 3,`). With #if lines already stripped, without the newline split
    the two members fuse into one comma-piece that the member regex then drops —
    producing a FALSE "EXTRA" + "MISSING" pair. WPF enum value expressions are
    single-line integer literals, so a depth-0 newline is always a member
    boundary here."""
    # normalize to spaces but keep newlines for the boundary split
    parts = []
    depth = 0
    cur = []
    instr = None
    i = 0
    while i < len(body):
        ch = body[i]
        if instr:
            cur.append(ch)
            if ch == instr and body[i - 1] != '\\':
                instr = None
            i += 1
            continue
        if ch in ('"', "'"):
            instr = ch
            cur.append(ch)
            i += 1
            continue
        if ch in '([{':
            depth += 1
        elif ch in ')]}':
            depth -= 1
        if depth == 0 and ch in (',', '\n'):
            if ''.join(cur).strip():
                parts.append(''.join(cur))
            cur = []
        else:
            cur.append(ch)
        i += 1
    if ''.join(cur).strip():
        parts.append(''.join(cur))
    return parts


def parse_enum_members(body, flags):
    """Return ordered list of (name, ordinal). Handles sequential default,
    explicit =expr (constant int / hex / enum member), Flags shift, and
    shifted-continuation (a following member with no = continues after the
    previous explicit value +1)."""
    members = []
    next_ordinal = 0
    flag_bit = 1
    explicit_last = None  # if the previous member had explicit value

    for piece in split_top_level(body):
        piece = piece.strip()
        if not piece:
            continue
        # The member may itself be `Name\n{...}` no; enum members are scalars here.
        mm = MEMBER_RE.match(piece)
        if not mm:
            # e.g. a stray doc leftover — skip
            continue
        name = mm.group(1)
        expr = (mm.group(2) or '').strip()

        if expr:
            val = eval_const(expr, members, next_ordinal)
            members.append((name, val))
            next_ordinal = val + 1
            explicit_last = True
        else:
            if flags:
                # In a [Flags] enum, a member with no explicit value takes the
                # next power-of-two beyond the previous explicit value.
                val = flag_bit
                flag_bit <<= 1
                if explicit_last is not None and explicit_last is False:
                    pass
            else:
                val = next_ordinal
            members.append((name, val))
            next_ordinal = val + 1
            explicit_last = False
    return members


def eval_const(expr, members, fallback):
    """Evaluate a constant integer expression. Handles:
       - decimal/hex/binary
       - unary +/- 
       - simple enum-member references (resolved from earlier members)
       - parenthesized single values, (int) casts, shifts (yield literal flags)
    Returns an int. On anything else, returns fallback (best-effort)."""
    e = expr.strip()
    # strip leading casts like (int), (uint), (long)
    cast_re = re.compile(r'^\([A-Za-z0-9_]+\)')
    e = cast_re.sub('', e).strip()
    e = e.replace('\t', ' ')
    # (int)Foo.Member / refs — resolve known member names
    member_vals = {n: v for n, v in members}

    def substitute(mo):
        nm = mo.group(1)
        if nm in member_vals:
            return str(member_vals[nm])
        # hex/decimal/binary fallback handled by int()
        return nm
    e = re.sub(r'\b([A-Za-z_][A-Za-z0-9_]*)\b', substitute, e)
    try:
        # handle simple arithmetic of ints via python eval on sanitized input
        # (tokens already substituted to ints; only digits/operators remain)
        val = int(eval(e, {'__builtins__': {}}, {}))
        return val
    except Exception:
        normalized = e.replace('0x', '').replace('X', '')
        try:
            return int(normalized, 16) if normalized else fallback
        except Exception:
            return fallback


def scan_all_enums():
    """Return {enum_name: {base, flags, members:[(name,ordinal)...], file}} for
    all enums across the scan roots. Later (higher-priority) file wins on name
    clash."""
    result = {}
    files = 0
    for root in SCAN_ROOTS:
        if not os.path.isdir(root):
            continue
        for dirpath, _, filenames in os.walk(root):
            for fn in filenames:
                if not fn.endswith('.cs'):
                    continue
                path = os.path.join(dirpath, fn)
                try:
                    raw = open(path, encoding='utf-8', errors='replace').read()
                except Exception:
                    continue
                text = strip_cs_artifacts(raw)
                # flags detection: the enum def (or preceding attribute) is Flags
                flags = bool(re.search(r'\[[^\]]*Flags', raw))
                for name, base, body in find_enum_bodies(text):
                    members = parse_enum_members(body, flags)
                    result[name] = {
                        'base': base,
                        'flags': flags,
                        'members': members,
                        'file': os.path.relpath(path, WPF_SRC),
                    }
                files += 1
    return result, files


# ---- VAML side: parse EnumDomainRegistry.cpp ----
VAML_REG = os.path.join(VAML_ROOT, "src", "core", "src", "EnumDomainRegistry.cpp")
VAML_DOMAIN_RE = re.compile(
    r'registry\.register_domain\(\s*"([^"]+)"\s*,\s*\n\s*make_domain\(\s*"([^"]+)"\s*,\s*\{'
    r'(.*?)\}\s*\)\s*\)', re.S)
VAML_MEMBER_RE = re.compile(r'\{"([^"]+)",\s*(-?\d+)\}')


def parse_vaml_domains():
    """Return {domainKey: {wpfType, members:[(name,ordinal)...]}}."""
    try:
        text = open(VAML_REG, encoding='utf-8', errors='replace').read()
    except Exception as e:
        return {}
    domains = {}
    for m in VAML_DOMAIN_RE.finditer(text):
        key = m.group(1)
        wpf_type = m.group(2)
        body = m.group(3)
        members = [(n, int(v)) for n, v in VAML_MEMBER_RE.findall(body)]
        domains[key] = {'wpf_type': wpf_type, 'members': members}
    return domains


def main():
    enums, files = scan_all_enums()
    vaml = parse_vaml_domains()

    lines = []
    lines.append("# WPF_ENUM_CATALOG.md — full enum inventory + VAML drift diff (engine contract)")
    lines.append("")
    lines.append("> **gap_wpf_enum_catalog.py (generated).** The complete catalog of every `enum` declared in")
    lines.append("> the REAL WPF source (`E:\\GitRepos\\wpf` — PresentationCore, PresentationFramework, WindowsBase —")
    lines.append("> the VAML-facing UI assemblies), with each member's **WPF-true ordinal**. This is the living")
    lines.append("> reference a VAML author diffs against to reduce enum-ordinal drift (the exact class of bug")
    lines.append("> Sprint 11p / `EnumDomainRegistry` was built to kill — e.g. `Dock` (Left=0) vs")
    lines.append("> `HorizontalAlignment` (Left=1)).")
    lines.append(">")
    lines.append("> Generated by re-running `python gap_wpf_enum_catalog.py` whenever WPF advances; the OUTPUT")
    lines.append("> is committed, NOT the stdout. Do NOT hand-edit generated rows; re-run the generator (or add")
    lines.append("> under the documented closure rule).")
    lines.append(">")
    lines.append(f"> **Scope:** `{files}` .cs files scanned; **{len(enums)} enum declarations** cataloged.")
    lines.append("")
    lines.append("## DRIFT DIFF — VAML EnumDomainRegistry vs WPF ground truth")
    lines.append("")
    lines.append("For each domain VAML registers, this compares VAML's ordinal for every member against the")
    lines.append("WPF enum of the same name. **ORDINAL DRIFT** = VAML's ordinal differs from WPF's; **MISSING** = ")
    lines.append("a member WPF has that VAML's registration table lacks. A member WPF does NOT have (VAML-invented)")
    lines.append("is flagged **EXTRA**.")
    lines.append("")
    lines.append("| VAML key | WPF enum | Verdict |")
    lines.append("|----------|----------|---------|")

    drift_rows = []
    for key, v in sorted(vaml.items()):
        wpf_type = v['wpf_type']
        wpf = enums.get(wpf_type)
        if wpf is None:
            drift_rows.append(
                f"| `{key}` | `{wpf_type}` | **WPF ENUM NOT FOUND** — no `enum {wpf_type}` in scan roots |")
            continue
        wpf_map = dict(wpf['members'])
        issues = []
        for nm, val in v['members']:
            wv = wpf_map.get(nm)
            if wv is None:
                issues.append(f"EXTRA `{nm}={val}` (not a WPF member)")
            elif wv != val:
                issues.append(f"ORDINAL DRIFT `{nm}`: VAML={val} WPF={wv}")
        for nm, wv in wpf['members']:
            vv = dict(v['members']).get(nm)
            if vv is None:
                issues.append(f"MISSING `{nm}` (WPF={wv})")
        if issues:
            drift_rows.append(
                f"| `{key}` | `{wpf_type}` | **⚠ {len(issues)}** — " + "; ".join(issues) + " |")
        else:
            drift_rows.append(
                f"| `{key}` | `{wpf_type}` | OK ({len(v['members'])} members match WPF) |")
    lines.extend(drift_rows)
    lines.append("")

    lines.append("### Reading the drift table (caveats)")
    lines.append("")
    lines.append("- **Resolved: `DockPanel.Dock` no longer registers `Fill=4`.** WPF's `Dock` enum is exactly")
    lines.append("  `{Left=0, Top=1, Right=2, Bottom=3}` — there is NO `Fill`. VAML previously registered")
    lines.append("  `Fill` as both a member and the DP's default; that drift was removed so `Dock` now matches")
    lines.append("  WPF member-for-member (the last-child fill is the positional rule + the `LastChildFill`")
    lines.append("  bool, never a Dock value).")
    lines.append("- **`IconStretch` is a VAML ALIAS, not a WPF enum.** WPF has no `IconStretch` type — the")
    lines.append("  members `{None=0, Fill=1, Uniform=2, UniformToFill=3}` exactly mirror WPF's `Stretch`")
    lines.append("  enum (`PresentationCore\\System\\Windows\\Media\\Generated\\Stretch.cs`). Not a defect, but it")
    lines.append("  is an invented name: the WPF-faithful type is `Stretch`.")
    lines.append("- **Members inside a `#if THEATRE_FULLSCREEN` / `#if DEBUG` region** (e.g. `WindowState.Theatre=3` /")
    lines.append("  `FullScreen=4`) are listed as MISSING because they are not part of a normal build. They are")
    lines.append("  not shipped drift — a VAML author should only add them if VAML also supports the symbol, and")
    lines.append("  NOTE these regions carry no trailing comma, which is why they parse as separate members.")
    lines.append("")
    lines.append("## Full enum catalog")
    lines.append("")
    lines.append("Non-Flags enums. `[Flags]` enums are marked with `🎏` and their members are the bit values.")
    lines.append("The ordinal shown is the WPF-true value. * = ordinal computed by the generator (sequential /")
    lines.append("explicit-expression best-effort); a non-numeric expression that could not be resolved is")
    lines.append("shown as the sequential fallback.")
    lines.append("")
    for name in sorted(enums):
        e = enums[name]
        base = e['base']
        flag_mark = " 🎏[Flags]" if e['flags'] else ""
        lines.append(f"### `{name}`  `:{base}`{flag_mark}  ({e['file']})")
        lines.append("")
        lines.append("| Member | Ordinal |")
        lines.append("|--------|---------|")
        for mn, mv in e['members']:
            lines.append(f"| `{mn}` | `{mv}` |")
        lines.append("")

    os.makedirs(os.path.dirname(OUT_DOC), exist_ok=True)
    with open(OUT_DOC, 'w', encoding='utf-8') as fh:
        fh.write("\n".join(lines))

    print(f"scanned {files} files -> {len(enums)} enums; {len(vaml)} VAML domains diffed")
    print(f"wrote {OUT_DOC}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
