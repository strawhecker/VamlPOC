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
- Last completed step : S031 (Value round-trip + type-mismatch-throws tests) [token: S031]
- Next step to do     : S032 [token: S032]
- PENDING work        : none
- Surviving-state digest: (computed when first build artifact exists; place SHA-256 of
  `build/state/surviving-nodes.ser` here once that pipeline is live)
- Key invariants intact: [ADR-001..012] [layer-rule: core has NO Vulkan/GLFW deps; only vulkan backend touches GPU]
  [no-managed] [no-stub rule] [MINIMAL REAL: vulkan→S161, tools→S287, Color→S089, Thickness→S089] [NO-GAP GUARANTEE] [NO LAZY LLM]

## GUARD RAILS (codified from S003 — mirror of docs/roadmap.md; survives roadmap trimming)
- **MINIMAL REAL**: permanent, linkable, forward-resolvable seed (kept+extended, NOT a stub);
  tagged with searchable sentinel `// [MINIMAL REAL] <comp> — RESOLVED BY <token>`; removable only
  by its declaring step. Active: vulkan→S161, tools→S287, Color→S089, Thickness→S089.
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

---

## SPRINT 0 — SURVIVOR  (the consolidated surviving state a fresh LLM must assume — S009 wrap)
- **Goal met (S000–S008): green skeleton, zero feature code.** Repo, toolchain, CI, and test
  harness are real and verified; no product feature (property system, parser, renderer, etc.) is
  started. All Sprint-0 gates are ✅.
- **Artifacts / libraries:** three STATIC libs — `vaml-core`, `vaml-vulkan`, `vaml-tools`
  (`VAML_BUILD_SHARED` default OFF; shared path proven but not the default). Artifact model +
  ADR-011 layer rule enforced in `cmake/artifact-model.cmake` (`vaml_add_library`, generate_export_header,
  `VAML_LAYER`, `vaml_validate_layer_rules`). Include-surface convention: internal impl headers under
  `src/<lib>/src`, ONLY public API under `src/<lib>/include/vaml/<lib>`.
- **Test harness:** vendored doctest single-header (v2.4.11) + ported `Shouldly.h`
  (`vaml::test` facade) with pluggable failure sink. `vaml-core-tests` EXECUTABLE =
  `smoke_test.cpp` + `shouldly_test.cpp`; `ctest` green 1/1.
- **Build lanes (both verified live):** MSVC VS 2026 (`msvc-debug` / `msvc-debug-shared`,
  warnings-as-errors ON) and real Ubuntu VM Clang 21.1.8 (`clang-debug`) via shared folder
  `/mnt/hgfs/VamlPOC` (cmake 4.2.3, ninja). Presets: `cmake/presets/{base,configure,build,test}.json`
  nested include chain; toolchains under `cmake/toolchains/`. MSVC lane driven via `build/verify_s*.cmd`
  (clean PATH + vcvars64). CI: `.github/workflows/ci.yml` (windows-msvc + ubuntu-clang).
- **MINIMAL REAL seeds carried forward UNRESOLVED (never backfilled in S0):**
  `vaml-vulkan` version marker → **S161**; `vaml-tools` version marker → **S287**. No other
  MINIMAL REAL active. Both carry searchable sentinel comments.
- **Invariants intact:** [ADR-001..012], layer-rule (core has NO Vulkan/GLFW deps), no-managed,
  no-stub, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME.
- **Next = S030** (Sprint 1, Property system core — first token of the real feature work).

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
- [x] S005 vendor doctest + test harness + 0-feature smoke test — ✅
  SURVIVOR: third_party/doctest/doctest.h (NEW — official doctest SINGLE HEADER, pinned v2.4.11,
  vendored VERBATIM, no upstream edits; SHA-256
  44FAA038E9C3F9728EFBDA143748D01124EA0A27F4BF78F35A15D8FAB2E039FB — regenerate from the
  https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h tag, never hand-edit).
  test/CMakeLists.txt + test/unit/CMakeLists.txt (+NEW): single EXECUTABLE vaml-core-tests
  from smoke_test.cpp, links vaml-core (proves consumer-link), vaml_apply_compiler_policy(),
  registered via add_test(NAME vaml-core-tests). test/unit/smoke_test.cpp (NEW):
  DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN + TEST_CASE("smoke") with 3 CHECKs (true, version().major==0,
  engine_name()) — exercises doctest + the vaml-core version symbol; 0-feature by design.
  Top-level CMakeLists.txt (MOD): enable_testing() + add_subdirectory(test) after
  vaml_validate_layer_rules(); test subtree contributes no library symbols. verify_s005.cmd +
  verify_s005_shared.cmd (NEW in build/, mirror S004 pattern; resets PATH so VS-bundled cmake/ninja
  win over Cygwin PATH). VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) configure+build+ctest green
  (1/1 PASS, doctest 2.4.11, 3/3 assertions), and REAL LINUX clang-debug on the Ubuntu VM
  (clang 21.1.8, cmake 4.2.3) via shared folder /mnt/hgfs/VamlPOC green (1/1 PASS). msvc-debug-shared
  still configures (no S003 regression). NOT a STUB / NOT MINIMAL REAL: doctest is permanent test
  infra, 0 product symbols introduced, so no sentinel + no resolution token (recorded here so it is
  not mistaken for a silent backfill). Per-case discovery via doctest_discover_tests deferred as a
  documented follow-up, not a silent omission. Next = S006.
  GOTCHA: vendored/3rd-party headers must be EXCLUDED from the strict /WX bar on both compilers.
  MSVC: mark third_party SYSTEM + /external:W0 (doctest triggers benign C5285 std::tuple
  specialization false-positive on new MSVC). Clang: -isystem already silences normal warnings but
  `#warning` directives are ALWAYS reported — upstream doctest includes compiler <ciso646> (carries
  a benign C++20 `#warning`; doctest uses it only for libc++ detection, we use libstdc++) so add
  -Wno-error=#warnings to the test target. GOTCHA: in CMakeLists, quote CMake arguments containing
  `#` (`"-Wno-error=#warnings"`) or CMake parses them as comments → "Function missing ending )".
  GOTCHA: build must be run from a clean PATH + vcvars64 (VS-bundled cmake/ninja); a stale
  msvc-debug cache created under a Cygwin PATH must be deleted before reconfigure (cygdrive path
  mismatch in CMakeCache.txt).
- [x] S006 port user Shouldly.h -> src/core/include/vaml/assert/Shouldly.h (namespace vaml::test) — ✅
  SURVIVOR: src/core/include/vaml/assert/Shouldly.h (NEW — header-only port of the user's
  PiuGlobalsCpp::NsShouldly Shouldly file; the spec's "src/core/assert/Shouldly.h" lands at
  src/core/include/vaml/assert/Shouldly.h to match the project's public-include convention).
  Namespace vaml::test (NOT vaml::assert — `assert` is a function-like macro from <cassert>, so
  `namespace assert {}` is ill-formed if <cassert> loads; `assert` as a dir name is fine).
  STRIPPED: MSTest (Microsoft::...::Assert::Fail + wide-string AssertStdString path), Windows bits
  (_wcsdup/wchar_t/free conversions + SHOULDLY_USES_AWESOMELY toggle -> failure now unconditional),
  __forceinline/__inline hints (plain inline), dead/commented code (commented-out Actual overloads,
  unused DynString + is_str helpers, AwesomelyException superseded). KEPT: portable
  #pragma push_macro("max")/#undef max guard. ADDED (only API): TestFailureException : std::exception,
  FailureSink, DefaultFailureSink (throw TestFailureException), failure_sink() (thread_local-static-
  backed settable singleton ref), ReportFailure(string) -> routes to failure_sink()(). All predicate
  names preserved: ShouldBe, ShouldNotBe, ShouldBeGreaterThan, ShouldBeLessThan,
  ShouldBeGreaterThanOrEqualTo, ShouldBeLessThanOrEqualTo, ShouldBeInRangeTo, ShouldBeNull,
  ShouldNotBeNull, ShouldBeNullOrEmpty, ShouldNotBeNullOrEmpty, ShouldNotBeNullOrEmptyOrBe,
  ShouldNotBeNullOrEmptyButBe, ShouldNotBeNullOrEmptyButBeNearlyEqual, ShouldNotBeNullOrEmptyButMatchContents
  + Diff; factories Integral/String/Pointer/Array + macros ShouldlyIntegral/ShouldlyString/
  ShouldlyPointer/ShouldlyArray (retargeted to vaml::test). No CMake change (header lands in existing
  public include root already on vaml-core-tests' include path; header-only -> nothing links).
  S006 is a NO-BUILD-BY-CONTRACT step (compile gate belongs to S007); still SILENT-COMPILE-CHECKED on
  MSVC (VS 2026 cl 19.51, /W4 /permissive- /EHsc /WX /std:c++20, throwaway TU incl. failing-predicate
  throw path + custom-sink path) -> W4/WX clean; throwaway files deleted. Next = S007.
  GOTCHA: the Shouldly factory helper functions REQUIRE the `line` argument (the *_macros supply it via
  `#stuff`); calling Integral/String/Pointer/Array directly without a line fails to overload — use
  ShouldlyIntegral/ShouldlyString/ShouldlyPointer/ShouldlyArray or pass a line explicitly.
  GOTCHA: on Windows, `cl` requires the VS dev shell (vcvars64.bat) — call it inside the same cmd that
  runs cl (a bare `cmd /c "... && cl"` does NOT persist the vcvars env across separate cmd invocations).
- [x] S007 Shouldly.h compiles under MSVC + Clang; assertion round-trip tests — ✅
  SURVIVOR: test/unit/shouldly_test.cpp (NEW — the compile-gate + round-trip test deliverable).
  Includes <vaml/assert/Shouldly.h> + doctest (main/impl owned by smoke_test.cpp; DOCTEST_CONFIG_
  IMPLEMENT_WITH_MAIN lives ONLY there to avoid duplicate-main at link). TEST STRUCTURE: a
  thread-local capture-sink RAII helper `CaptureOn` (installs a collecting FailureSink via
  failure_sink(), restores the prior sink on scope exit, clears the thread-local buffer) -> the
  "round-trip" (predicate -> ReportFailure -> sink -> captured formatted message). 14 test cases:
  (A) pass-vectors for Integral/String/Pointer/Array asserting NOTHING is captured; (B) fail-vectors
  per predicate asserting the captured message carries the expected fragment (e.g. "should be equal",
  "greater than", "range", "[i]" for elementwise); (C) Diff helper both directions on a double array;
  (D) default-sink contract: with NO custom sink a failing predicate throws TestFailureException
  (CHECK_THROWS_AS). Covered predicates: ShouldBe, ShouldNotBe, ShouldBeGreater/LessThan,
  Greater/LessThanOrEqualTo, InRangeTo, BeNull, NotBeNull, BeNullOrEmpty, NotBeNullOrEmpty,
  NotBeNullOrEmptyOrBe/ButBe/ButBeNearlyEqual(value+pointer)/ButMatchContents, Diff.
  test/unit/CMakeLists.txt (MOD): added shouldly_test.cpp to vaml-core-tests sources (no new flags;
  doctest already SYSTEM-excluded + -Wno-error=#warnings). build/verify_s007.cmd (NEW): clone of
  verify_s005 — clean PATH -> vcvars64 -> cmake --preset msvc-debug -> build -> ctest.
  HEADER FIXES (S007 DOES touch Shouldly.h — sanctioned plan-scoped portability+bug fixes, each the
  "compile gate S006 deferred"): (1) BUG: ShouldBeNullOrEmpty required BOTH null AND len==0; intent
  is "null OR empty" — collapsed the two sequential ifs to one `if (!(actual==nullptr || len==0))`.
  (2) TestFailureException(const char*) relied on MSVC-only std::exception(char const*) ctor — now
  stores std::string message_ + overrides what() (portable to libstdc++). (3) Remove meaningless
  top-level const on `char const* const` RETURN types (Clang -Wignored-qualifiers). (4) Qualified
  std::fpclassify + std::isnormal (MSVC leaked cmath names into global scope; libstdc++ does not);
  FP_ZERO is a C MACRO (keep bare, not std::FP_ZERO — naive std:: prefix broke both compilers).
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) via verify_s007.cmd GREEN (1/1 ctest PASS, 15 test
  cases listed = 14 Shouldly + smoke, all green); REAL LINUX clang-debug on Ubuntu VM via shared
  folder /mnt/hgfs/VamlPOC (clang 21.1.8) GREEN (1/1 ctest PASS). No new product symbols; this test
  file is permanent test infra (NOT a STUB / NOT MINIMAL REAL, recorded). Next = S008.
  GOTCHA: cannot declare a zero-length array in standard C++ (`int x[0]` ill-formed) — use a non-null
  pointer with len 0 to exercise the empty/len==0 path. GOTCHA: `ShouldBeNullOrEmpty` is "null OR
  empty", NOT "null AND empty" — my first pass-case wrongly assumed AND; the OR semantics is the
  correct original intent. GOTCHA: FP_ZERO is a preprocessor macro (math.h: `#define FP_ZERO 2`), NOT
  a std:: member; `std::FP_ZERO` is ill-formed everywhere (both MSVC C2589 and Clang error) — only
  fpclassify/isnormal take the std:: qualification. GOTCHA: MSVC accepted unqualified fpclassify/
  isnormal (they leak into global scope), so /W4 /WX was blind to the non-portable call — the Linux
  Clang lane is what surfaced it; keep the Clang lane authoritative for portability.
- [x] S008 REFACTOR GATE(S0): no dead code, headers internal-only, static-link default verified, clean PR — ✅
  SURVIVOR: DELETED build/verify_msvc.cmd (superseded S001-era raw `-G Ninja` + direct-flag harness,
  fully duplicated by the preset-driven verify_s004/s005/s005_shared/s007.cmd; was gitignored/untracked).
  MODIFIED cmake/artifact-model.cmake (S008): resolve VAML_ARTIFACT_MODEL (STATIC/SHARED) + `message(
  STATUS "VAML artifact model: ..." )` so the static-by-default is provable from configure output (no
  hard FATAL); added include-surface convention comment — internal impl headers live under
  src/<lib>/src, ONLY public API under src/<lib>/include/vaml/<lib> (write-rule ONLY, NO header moves
  in S0; Shouldly.h stays the public vaml::test facade; vulkan/tools keep MINIMAL REAL version.h seeds
  until S161/S287). MODIFIED docs/roadmap.md: S008 [DONE]; S001 note now cites preset verify scripts.
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) via verify_s007.cmd GREEN — configure prints
  "VAML artifact model: STATIC", static vaml-{core,vulkan,tools}.lib, ctest 1/1 PASS; REAL LINUX
  clang-debug on Ubuntu VM (clang 21.1.8) via /mnt/hgfs/VamlPOC GREEN — STATIC .a, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> deprecation from vendored
  third_party/doctest under GCC-15 libstdc++ (not VamlPOC source, not introduced here). Tracked
  change-set = cmake/artifact-model.cmake + docs/roadmap.md only (export.h dirty flags are pre-existing
  CRLF/LF autocrlf noise — zero content diff vs HEAD). NO new md files (directive). Sprint-0
  SURVIVOR/checkpoint wrap is S009's job; this entry is a minimal non-duplicative record. Next = S009.
  GOTCHA: drive the MSVC lane through build/verify_s*.cmd (clean PATH + vcvars64 -> VS-bundled
  cmake/ninja win over any Cygwin PATH); a stale msvc-debug CMakeCache containing a /cygdrive path
  must be wiped before reconfigure.
- [x] S009 SURVIVOR wrap (Sprint 0) — ✅
  SURVIVOR: this step is documentation-only (no build, per plan). Added the consolidated
  `## SPRINT 0 — SURVIVOR` block above (distills S000–S008 into the surviving-state picture a fresh
  LLM resumes from: green skeleton, 3 STATIC libs, ADR-011 artifact model, vendored doctest+vaml::test
  harness, MSVC+Linux Clang lanes both verified, MINIMAL REAL vulkan→S161 + tools→S287 unresolved,
  invariants intact). Regenerated `## CHECKPOINT` (last completed = S009; next = S030; PENDING none).
  Marked roadmap S009 `[DONE]`. No new md files (directive); no MINIMAL REAL introduced or resolved;
  NO-GAP GUARANTEE remains satisfied (both active seeds still carry explicit resolution tokens).
  Next = S030.

### Sprint 1 — Property system core   (S030–S049)
- [x] S030 Typeless Value — ✅ (see roadmap S030 line for full detail)
  SURVIVOR: `src/core/include/vaml/core/{Color,Thickness,Value}.h` + `src/core/src/Value.cpp`.
  Color = uint8 RGBA struct; Thickness = float l,t,r,b struct — BOTH tagged
  `// [MINIMAL REAL] ... RESOLVED BY S089`. Value = `ValueType:uint8_t` enum (11 tags in roadmap
  order Null..UntypedPtr), hand-rolled tagged union (NOT std::variant), static factories for all 11
  payloads, `type()`/`isNull()`, template `get<T>()` throwing `vaml::ValueTypeMismatch` on tag/T
  mismatch, `operator==`, explicit copy/move/dtor for the std::string member.
  `ValueTypeOf<T>` trait maps payload type → accepted tags; `get<int32_t>` accepts BOTH Int32 and
  BrushId (BrushId is a tagged int32 scalar id). Test TU `test/unit/Value_test.cpp` (S030 scope:
  factory→tag, equality, copy/move). Build fixes: enum/Storage forward-ref order (MSVC); read<T>()
  noexcept mismatch from string-copy-throws (Clang) → primary+all specializations non-noexcept;
  dropped duplicate doctest main. VERIFIED msvc-debug (25/25 cases, 91/91 asserts) + clang-debug.
  NO-GAP: Color/Thickness resolution authored at same step (→S089). Next = S031.
- [x] S031 Value round-trip + type-mismatch-throws tests — ✅
  SURVIVOR: `test/unit/Value_roundtrip_test.cpp` (NEW; doctest, NO IMPLEMENT_WITH_MAIN — shares
  smoke_test's main). Complementary to S030's Value_test.cpp (left untouched). Scope:
  (A) ROUND-TRIP — every non-Null tag's payload round-trips through `get<T>()` EXACTLY and the
  re-factory `Value::X(v.get<P>()) == v` (operator== round-trip). String verified to round-trip by
  VALUE (copy), mutation of the read-back copy does not affect the stored value. BrushId/UntypedPtr
  included. (B) TYPE-MISMATCH-THROWS — Null has no payload so EVERY get<T> throws (gesture not
  corrupting: Null stays Null); each non-Null tag throws `ValueTypeMismatch` for every NON-valid
  payload type (exhaustive per-tag, incl. no cross-width/no-promotion: Int32 not int64/float/double,
  Bool not int32, Float not double, BrushId reads via int32_t ONLY but tag stays BrushId ≠ Int32 and
  BrushId(1) != Int32(1)); String mismatches every scalar and vice-versa; constraint — after a
  throw the SAME value still re-reads its valid payload and still equals its original (throw path is
  a read-only no-op on the union). CMakeLists (MOD): + Value_roundtrip_test.cpp to vaml-core-tests.
  build/verify_s031.cmd (NEW): clone of verify_s007 pattern (clean PATH + vcvars64 → msvc-debug).
  VERIFIED BOTH COMPILERS: msvc-debug (VS 2026) exe 33/33 cases, 237/237 asserts PASS (was
  25/91 at S030); REAL LINUX clang-debug (Ubuntu VM clang 21.1.8) build green, ctest 1/1 PASS.
  Only Clang warnings = pre-existing upstream <ciso646> doctest deprecation (not this TU).
  No MINIMAL REAL introduced/resolved; NO-GAP unchanged. Next = S032.
  GOTCHA: `get<T>()` is `[[nodiscard]]` (S030) — `CHECK_THROWS_AS(a.get<T>(), ...)` discards the
  return → MSVC C4834 (warnings-as-errors). Fix: wrap the expression `(void)a.get<T>()` inside
  CHECK_THROWS_AS; applied to all throw-paths.
