# VAML — PROGRESS / LLM CONTINUATION FILE  (SINGLE SOURCE OF TRUTH)

> ## HOW TO RESUME (an LLM must read this first, with no prior context)
> 1. Read ONLY this header block + the two `## CHECKPOINT` lines below.
> 2. That tells you: repo state, last completed step token, next step token, and the
>    surviving-state fingerprint. Do NOT re-read the history section to resume.
> 3. If a step is partially done (PENDING), read its SURVIVOR block directly.
> 4. Generated artifacts are never trusted; regenerate any derived file from the
>    surviving source files. Trust this file for WHERE things stand, trust source
>    files for WHAT the code is.

---

## HEADER — VERSION 1
- Framework: VAML (XAML-faithful, Vulkan-native, pure C++)
- Language/stack: C++ latest only. No C#, no Python, no managed code (includes build tooling).
- Distribution: consumer chooses shared (dll/so) or static (lib/.a); default = static for IP-hiding. [ADR-003]
- Repos: `VamlPOC` = proof-of-concept (this). Production `vaml` repo opens after the POC proves the pipeline.
- Build: MSVC (VS 2026) on Windows and Clang on Linux; CMake is the canonical driver. [ADR-007]
- Tests: doctest framework + adapted Shouldly-style fluent asserts (ported, MSTest-decoupled). [ADR-009]
- Async: Task/Executor continuation model, NOT compiler coroutine keywords. [ADR-010]
- MVVM: VAML ships its own lightweight MVVM layer (ObservableObject, RelayCommand, Messenger,
  static CompositionRoot, RoutedEvent). No Prism-style runtime modularity. [ADR-008]

## CHECKPOINT — DO NOT EDIT BY HAND (agents regenerate)
- Last completed step : S000 (repo skeleton + ALL tracking/docs files) [token: S000]
- Next step to do     : S001 (top-level CMake: project(VAML), C++ latest, /EHsc /permissive-
                       MSVC, -Wall -Wextra -Wpedantic Clang, Werror)  [token: S001]
- PENDING work        : none
- Surviving-state digest: (computed when first build artifact exists; place SHA-256 of
  `build/state/surviving-nodes.ser` here once that pipeline is live)
- Key invariants intact: [ADR-001..010] [layer-rule core↛vulkan] [no-managed] [no-stub rule]

## SURVIVING STATE (what a fresh LLM must assume is true)
- S000 DONE: directory skeleton created — docs/ (adr, design), src/ (core, vulkan, tools, cmake),
  test/ (unit, integration), VamlPOC/ with SoC subfolders {hmi, core, infra, viewmodels}/ for the
  POC APPLICATION (distinct from the library source tree in src/); tracking files present:
  progress.md (ROOT), docs/ROADMAP.md (S000–S999 tokenized), docs/architecture.md (seed),
  docs/adr/ADR-LOG.md (ADR-001..011), docs/design/DESIGN-SEEDS.md.
- Build system: NOT started (S001 is the first code step).
- Property system, parser, Vulkan, binding, etc.: NOT started. See ROADMAP.md for the full S000–S999
  tokenized step list. The "no-stub" rule means a step is only ✅ when its behavior is real and tested.

---

## STEP LOG  (append-only; new steps land at the BOTTOM)

### Sprint 0 — Repo, toolchain, CI, test harness   (S000–S019)
- [x] S000 repo skeleton + tracking files (this file, ROADMAP.md, ADR seeds, architecture.md) — ✅
  SURVIVOR: layout is docs/{adr,design}/, src/{core,vulkan,tools,cmake}/, test/{unit,integration}/,
  VamlPOC/{app+hmi,core,infra,viewmodels}/. progress.md lives at repo ROOT (lowercase). Next = S001.

### (remaining S001–S999 steps live in docs/ROADMAP.md, the authoritative tokenized list)
