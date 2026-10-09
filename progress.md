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
- Last completed step : S004 (CI matrices) [token: S004]
- Next step to do     : S005 (vendor doctest + test harness) [token: S005]
- PENDING work        : none
- Surviving-state digest: (computed when first build artifact exists; place SHA-256 of
  `build/state/surviving-nodes.ser` here once that pipeline is live)
- Key invariants intact: [ADR-001..012] [layer-rule: core has NO Vulkan/GLFW deps; only vulkan backend touches GPU]
  [no-managed] [no-stub rule] [MINIMAL REAL: vulkan→S161, tools→S287] [NO-GAP GUARANTEE] [NO LAZY LLM]

## GUARD RAILS (codified from S003 — mirror of docs/roadmap.md; survives roadmap trimming)
- **MINIMAL REAL**: permanent, linkable, forward-resolvable seed (kept+extended, NOT a stub);
  tagged with searchable sentinel `// [MINIMAL REAL] <comp> — RESOLVED BY <token>`; removable only
  by its declaring step. Active: vulkan→S161, tools→S287.
- **NO-GAP GUARANTEE**: no feature/symbol left for silent backfill; every MINIMAL REAL has an
  explicit resolution authored at the same step that delivers real content.
- **NO LAZY LLM SYNDROME**: default posture is to reason the opposite of lazy; MINIMAL REAL is
  explicitly NOT lazy — it is strategic, documented-by-design.

## SURVIVING STATE (what a fresh LLM must assume is true)
- S000 DONE: directory skeleton created — docs/ (adr, design), src/ (core, vulkan, tools, cmake),
  test/ (unit, integration), VamlPOC/ with SoC subfolders {hmi, core, infra, viewmodels}/ for the
  POC APPLICATION (distinct from the library source tree in src/); tracking files present:
  progress.md (ROOT), docs/ROADMAP.md (S000–S999 tokenized), docs/architecture.md (seed),
  docs/adr/ADR-LOG.md (ADR-001..012), docs/design/GPU-DRIVEN-PRINCIPLE.md, docs/design/DESIGN-SEEDS.md.
- S001 DONE: top-level CMake (project(VAML) 0.1.0, C++20 CXX), vaml_apply_compiler_policy(),
  src/ + src/core CMake, vaml-core STATIC lib with version API (non-stub), CMakePresets.json,
  .gitignore, build/verify_msvc.cmd. VERIFIED: MSVC 19.51, warnings-as-errors ON, vaml-core.lib
  builds+links green. NOTE: no Vulkan/GLFW/vcpkg at S001 (correct — core has zero Vulkan deps,
  ADR-011); Next = S002.
- Build system: NOT started (S001 is the first code step).
- Property system, parser, Vulkan, binding, etc.: NOT started. See ROADMAP.md for the full S000–S999
  tokenized step list. The "no-stub" rule means a step is only ✅ when its behavior is real and tested.

---

## STEP LOG  (append-only; new steps land at the BOTTOM)

### Sprint 0 — Repo, toolchain, CI, test harness   (S000–S019)
- [x] S000 repo skeleton + tracking files (this file, ROADMAP.md, ADR seeds, architecture.md) — ✅
  SURVIVOR: layout is docs/{adr,design}/, src/{core,vulkan,tools,cmake}/, test/{unit,integration}/,
  VamlPOC/{app+hmi,core,infra,viewmodels}/. progress.md lives at repo ROOT (lowercase). Next = S001.
- [x] S001 top-level CMake + compiler policy + vaml-core baseline — ✅
  SURVIVOR: CMakeLists.txt (project(VAML) 0.1.0, C++20, /W4 /permissive- /EHsc /WX MSVC;
  -Wall -Wextra -Wpedantic -Werror Clang), vaml_apply_compiler_policy() helper, src/CMakeLists.txt,
  src/core/CMakeLists.txt (vaml-core STATIC), include/vaml/version.h (Version + version() function),
  src/version.cpp, CMakePresets.json (msvc/clang), .gitignore, build/verify_msvc.cmd. Build verified
  MSVC 19.51 warnings-as-errors → vaml-core.lib links. No Vulkan at S001 (correct, ADR-011).
  GOTCHA: use vcvars64.bat before cl; use VS-bundled cmake.exe + ninja.exe (not on base PATH).
- [x] S002 cmake/ toolchain files + compiler presets (MSVC VS2026, Linux Clang) — ✅
  SURVIVOR: policy extracted to cmake/compiler-policy.cmake (vaml_apply_compiler_policy, flags
  byte-identical to S001: MSVC /W4 /permissive- /EHsc /Zc:* /WX + /wd4100; Clang -Wall -Wextra
  -Wpedantic -Werror). Toolchains: cmake/toolchains/{msvc-vs2026,linux-clang,windows-clang}.cmake.
  Presets split into cmake/presets/{base,configure,build,test}.json (Alt B) with a nested include
  CHAIN (base<-configure<-build<-test, root includes cmake/presets/test.json) — siblings do NOT
  cross-inherit in CMake. Root CMakePresets.json is a thin include stub. Presets anchored to
  toolchainFiles: msvc-debug/release->msvc-vs2026, clang-debug->linux-clang,
  windows-clang-debug->windows-clang. VERIFIED: clean configure --preset msvc-debug via include
  chain + toolchainFile -> MSVC 19.51, /WX active, vaml-core.lib builds green. Linux + windows-clang
  toolchains are static (no-build-on-host; review-validated only). Next = S003.
  GOTCHA: Ninja generator does NOT support CMAKE_GENERATOR_TOOLSET (MSVC toolchain file must NOT pin
  a toolset; vcvars64 selects it). GOTCHA: preset inheritance must be a nested include chain, not
  sibling includes.

### (remaining S003–S999 steps live in docs/ROADMAP.md, the authoritative tokenized list)

- [x] S003 static-library artifact model + guard rails — ✅
  SURVIVOR: cmake/artifact-model.cmake (NEW — vaml_add_library() helper, VAML_BUILD_SHARED
  default OFF, generate_export_header per lib, OUTPUT_NAME vaml-*, VAML_LAYER tag,
  vaml_validate_layer_rules() ADR-011 guard). Top-level CMakeLists includes artifact-model +
  calls vaml_validate_layer_rules() after add_subdirectory(src). src/core/CMakeLists.txt refactored
  to vaml_add_library(core). src/vulkan/CMakeLists.txt + src/tools/CMakeLists.txt NEW, un-commented
  in src/CMakeLists.txt; each builds a version-marker MINIMAL REAL lib. Libs now: vaml-core,
  vaml-vulkan, vaml-tools. Generated export headers committed at
  src/{core,vulkan,tools}/include/vaml/{core,vulkan,tools}/export.h (VAML_{CORE,VULKAN,TOOLS}_EXPORT;
  empty when static). Presets: msvc-debug-shared added (configure/build/test) to prove shared path.
  VERIFIED: static (`msvc-debug`) builds vaml-core.lib+vaml-vulkan.lib+vaml-tools.lib green, /WX ON;
  shared (`msvc-debug-shared`) builds vaml-core.dll/vulkan/tools green. ADR-011 guard negative-tested
  (vaml-core linking vaml-vulkan → configure FATAL "ADR-011 VIOLATION", then reverted to green).
  Linux + windows-clang static: review-validated only (no host build; consistent with S002).
  GUARD-RAIL CODIFICATION: MINIMAL REAL, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME terms added to both
  docs/roadmap.md (Guard Rails section) + progress.md (Guard Rails block); S003/S161/S287 lines in
  roadmap carry the forward-declared resolution contracts (vulkan→S161, tools→S287). No new md files.
  Next = S004.
  GOTCHA: layer guard introspects LINK_LIBRARIES (direct edges) — sufficient because GLFW/Vulkan are
  only reachable via vaml-vulkan in this project. GOTCHA: export.h generated into the PUBLIC include
  tree (absolute EXPORT_FILE_NAME) so targets consumers include via vaml/{lib}/export.h.
- [x] S004 CI matrices (.github/workflows) — ✅
  SURVIVOR: .github/workflows/ci.yml (NEW, sole deliverable of S004). Single matrix-driven file, two
  live jobs exactly matching ADR-007 core matrix: windows-msvc (windows-latest, VS 2026 cl.exe) ->
  preset msvc-debug; ubuntu-clang (ubuntu-latest, clang++) -> preset clang-debug. Both jobs run the
  identical checkout -> install toolchain -> cmake --preset -> cmake --build --preset -> ctest
  --preset --output-on-failure sequence, reusing S002/S003 presets verbatim (msvc-debug/clang-debug
  are named identically across configure/build/test). MSVC lane: microsoft/setup-msbuild (cl.exe +
  vcvars on PATH) + choco install ninja -y; Linux: apt clang + ninja-build. Guards: per-job
  timeout-minutes=30, warnings-as-errors ON via presets, no pinned runner labels (rolling -latest,
  keeps MSVC lane on same VS as local dev + follows platform). NO windows-clang lane in any form;
  NOT commented into YAML (toolchain seed stays unbuilt in cmake/toolchains/). Non-goals: no
  artifacts, no coverage, no packaging (future sprints own those). VERIFICATION: MSVC lane
  reproducible locally against the same msvc-debug preset (VS 2026) — re-ran clean, catches preset
  name typos pre-push. Ubuntu lane = no-build-on-host; acceptance is GH Actions on push/PR (free for
  public repos). All preset names in the workflow cross-checked against configure/build/test.json.
  Roadmap S004 line marked [DONE]. No build-system/feature code changed. Next = S005.
  GOTCHA: a workflow depends on preset NAMES being identical across configure/build/test JSON —
  CI calls only --preset <name>, never CMake directly, so a rename in presets silently breaks a lane
  (checked: msvc-debug + clang-debug are consistent). GOTCHA: MSVC lane needs the VS dev shell env
  (cl.exe), so setup-msbuild MUST precede cmake --preset; base.json sets Ninja generator so Ninja
  must be installed (choco) before configure.
