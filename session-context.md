# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit (S006) — header port done; silent MSVC compile-check green
- last_commit     : none this step (implementit;no commits). Prior commit: 979ef05 on main (S005).
- plan_repo_state : after work, only session-context.md is modified (this record). No commits made.
- plan_digest     : S006 — port user Shouldly.h -> src/core/include/vaml/assert/Shouldly.h
                    (header-only, namespace vaml::test): strip MSTest/__forceinline/Windows bits;
                    pluggable failure sink (throw TestFailureException). All predicate names preserved.
                    No-build-by-contract; silent MSVC compile-check done.
- files_touched   :
    CREATED  src/core/include/vaml/assert/Shouldly.h   (header-only port; TestFailureException +
                     FailureSink + failure_sink() + ReportFailure; all predicate names preserved;
                     factories Integral/String/Pointer/Array + macros retargeted to vaml::test)
    MODIFIED docs/roadmap.md      (S006 line marked [DONE]; path corrected to src/core/include/vaml/assert/Shouldly.h)
    MODIFIED progress.md          (CHECKPOINT S005->S006 + STEP LOG S006 survivor)
    MODIFIED session-context.md (this file)
    (throwaway build/s006_* compile-check TU + cl.bat created then deleted — not tracked)
- build_status    : S006 is NO-BUILD-BY-CONTRACT (compile gate belongs to S007). Header silently
                    compile-checked on MSVC (VS 2026 cl 19.51, /W4 /permissive- /EHsc /WX /std:c++20)
                    incl. failing-predicate throw path + custom-sink path -> W4/WX clean. No CMake
                    change; no full project build this step.
- next_step       : S007 (Shouldly.h compiles under MSVC + Clang; assertion round-trip tests)
- notes           :
    - Namespace is vaml::test (NOT vaml::assert): `assert` is a function-like macro from <cassert>, so
      `namespace assert {}` is ill-formed if <cassert> loads; `assert` as a dir name is fine.
    - Stripped all MSTest refs, Windows bits (_wcsdup/wchar_t/free, SHOULDLY_USES_AWESOMELY toggle),
      __forceinline/__inline, dead/commented code (Actual overloads, DynString, is_str, AwesomelyException).
    - Kept portable #pragma push_macro("max")/#undef max guard.
    - The Shouldly factory helpers REQUIRE the `line` arg (macros supply it via #stuff) — call the
      *_macros or pass a line explicitly.
    - On Windows, `cl` needs the VS dev shell (vcvars64.bat) inside the SAME cmd that runs cl.
    - No new non-plan/operational md files; no commits (per protocol).
