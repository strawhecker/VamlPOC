# VAML — SESSION-CONTEXT (per-run state)

- last_command    : commitit (S005) — committed + pushed on Windows repo
- last_commit     : 979ef05 on main, pushed to origin (9343df6..979ef05). Message = active S005 plan
                    text verbatim (option B). 11 files, +7263/-48. Includes the three pre-existing
                    uncommitted export.h changes (VAML_*_DEPRECATED -> __declspec(deprecated) so
                    generated export headers are MSVC-acceptable), which S005's MSVC-green depends on.
- plan_repo_state : after push, only session-context.md is modified (this commitit record). Verify/CI
                    scripts build/verify_s005.cmd + verify_s005_shared.cmd live under build/ (gitignored
                    like the S004 script) — present on disk, not tracked.
- plan_digest     : S005 — vendor doctest single-header into third_party (v2.4.11, verbatim);
                    test target vaml-core-tests links vaml-core + doctest smoke test (0-feature);
                    CTest PASS on both MSVC (VS 2026) and Clang (Ubuntu VM). No product symbols.
- files_touched   :
    CREATED  third_party/doctest/doctest.h   (doctest v2.4.11 single header, vendored verbatim)
    CREATED  test/CMakeLists.txt             (add_subdirectory(unit))
    CREATED  test/unit/CMakeLists.txt        (vaml-core-tests exec; SYSTEM third_party; /external:W0 /
                                              -Wno-error=#warnings for vendored headers; add_test)
    CREATED  test/unit/smoke_test.cpp        (DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN; TEST_CASE("smoke")
                                              3 CHECKs incl. vaml::version().major==0)
    CREATED  build/verify_s005.cmd           (PATH reset + vcvars64 + msvc-debug configure/build/ctest)
    CREATED  build/verify_s005_shared.cmd    (PATH reset + msvc-debug-shared configure regression)
    MODIFIED CMakeLists.txt                  (enable_testing() + add_subdirectory(test))
    MODIFIED docs/roadmap.md                 (S005 line marked [DONE])
    MODIFIED progress.md                     (CHECKPOINT S004->S005 + STEP LOG S005 survivor)
    MODIFIED session-context.md (this file)
- build_status    : GREEN BOTH COMPILERS. msvc-debug (VS 2026 cl 19.51) configure+build+ctest:
                    vaml-core/vulkan/tools libs + vaml-core-tests.exe build warnings-/WX-clean;
                    CTest 1/1 PASS (doctest 2.4.11, 3/3 assertions). msvc-debug-shared configures
                    (no S003 regression). clang-debug REAL LINUX (Ubuntu VM, clang 21.1.8, cmake
                    4.2.3 via /mnt/hgfs/VamlPOC shared folder): configure+build+ctest green, 1/1 PASS.
- next_step       : S006 (port Shouldly.h -> src/core/assert/Shouldly.h on the doctest harness)
- notes           :
    - doctest pinned http://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h;
      SHA-256 44FAA038E9C3F9728EFBDA143748D01124EA0A27F4BF78F35A15D8FAB2E039FB recorded in progress.md.
    - Vendored/3rd-party headers must be excluded from the strict /WX bar on BOTH compilers:
      MSVC -> third_party as SYSTEM include + /external:W0 (doctest C5285 std::tuple false-positive);
      Clang -> -isystem silences normal system-header warnings but `#warning` is ALWAYS reported, so
      add -Wno-error=#warnings (doctest includes <ciso646> carrying a benign C++20 `#warning`).
    - CMake arg containing `#` must be QUOTED ("-Wno-error=#warnings") or CMake treats it as a comment
      -> "Function missing ending )" parse error.
    - Build must run from a clean PATH + vcvars64 so VS-bundled cmake/ninja win over the Cygwin PATH;
      a stale msvc-debug CMakeCache created under a Cygwin PATH must be deleted before reconfigure.
    - SSH key is at C:\Users\straw\.ssh\vaml_ci_key (NOT E:\GitRepos\VamlPOC\vaml_ci_key as a stale
      S004 note said). Shared-folder path on VM is /mnt/hgfs/VamlPOC (live mirror of E:\GitRepos\VamlPOC).
    - No git commands, no commits, no new non-plan/operational md files (per protocol). This step
      introduced no STUB / MINIMAL REAL marker (doctest = permanent test infra, 0 product symbols).
