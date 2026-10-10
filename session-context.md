# VAML — SESSION-CONTEXT (per-run state)

- last_command    : commitit — CI fix committed and pushed as e761716
- last_commit     : e761716 on main (ci fix: msvc-dev-cmd places cl.exe on PATH). Prior: 979ef05 (S005).
- plan_repo_state : e761716 (ci fix) committed + pushed to origin/main. session-context.md now records that state (this record). Working tree otherwise clean.
- plan_digest     : S006 — port user Shouldly.h -> src/core/include/vaml/assert/Shouldly.h
                    (header-only, namespace vaml::test): strip MSTest/__forceinline/Windows bits;
                    pluggable failure sink (throw TestFailureException). All predicate names preserved.
                    No-build-by-contract; silent MSVC compile-check done.
                    + AD-HOC CI FIX (outside S006): Windows MSVC configure fails on CI because
                    cl.exe is not on PATH. Diagnosis: setup-msbuild only adds MSBuild.exe, not cl.
- files_touched   :
    CREATED  src/core/include/vaml/assert/Shouldly.h   (header-only port; TestFailureException +
                     FailureSink + failure_sink() + ReportFailure; all predicate names preserved;
                     factories Integral/String/Pointer/Array + macros retargeted to vaml::test)
    MODIFIED docs/roadmap.md      (S006 line marked [DONE]; path corrected to src/core/include/vaml/assert/Shouldly.h)
    MODIFIED progress.md          (CHECKPOINT S005->S006 + STEP LOG S006 survivor)
    MODIFIED .github/workflows/ci.yml  (REPLACED microsoft/setup-msbuild@v2 with ilammy/msvc-dev-cmd@v1
                     so vcvars64 runs and cl.exe lands on PATH for Configure/Build/Test on windows job)
    MODIFIED session-context.md   (this file)
    (throwaway build/s006_* compile-check TU + cl.bat created then deleted — not tracked;
     ad-hoc diagnostics curl output + run_msvc_configure.cmd created then deleted — not tracked)
- build_status    : S006 is NO-BUILD-BY-CONTRACT (compile gate belongs to S007). Header silently
                    compile-checked on MSVC (VS 2026 cl 19.51, /W4 /permissive- /EHsc /WX /std:c++20)
                    incl. failing-predicate throw path + custom-sink path -> W4/WX clean. No CMake
                    change; no full project build this step.
                    CI FIX: ci.yml YAML validated (PyYAML safe_load OK). CI run 38056831180 had failed
                    at 'Configure (msvc-debug preset)' with "cl not found in PATH" — fix targets that.
- next_step       : commit + push ci.yml fix; monitor new CI run; then S007 (Shouldly.h compiles under
                    MSVC + Clang; assertion round-trip tests)
- notes           :
    - Namespace is vaml::test (NOT vaml::assert): `assert` is a function-like macro from <cassert>, so
      `namespace assert {}` is ill-formed if <cassert> loads; `assert` as a dir name is fine.
    - Stripped all MSTest refs, Windows bits (_wcsdup/wchar_t/free, SHOULDLY_USES_AWESOMELY toggle),
      __forceinline/__inline, dead/commented code (Actual overloads, DynString, is_str, AwesomelyException).
    - Kept portable #pragma push_macro("max")/#undef max guard.
    - The Shouldly factory helpers REQUIRE the `line` arg (macros supply it via #stuff) — call the
      *_macros or pass a line explicitly.
    - On Windows, `cl` needs the VS dev shell (vcvars64.bat) inside the SAME cmd that runs cl.
    - CI FIX ROOT CAUSE: microsoft/setup-msbuild@v2 adds only MSBuild.exe to PATH, NOT cl.exe; each
      GH Actions step is a fresh shell, so the Configure step had no cl. ilammy/msvc-dev-cmd@v1 runs
      vcvars64 and exports PATH via $GITHUB_PATH -> cl available to all later steps. Linux lane
      unaffected (clang++ on PATH from apt step).
    - No new non-plan/operational md files; commits pending explicit commitit directive.
