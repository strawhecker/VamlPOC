# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit S008 (no commits; no new md files)
- last_commit     : e761716 on main (S006 era). S007 and S008 not committed (no-commit directive).
- plan_repo_state : S008 work uncommitted (working tree dirty by design — no commit asked).
- plan_digest     : S008 — REFACTOR GATE(S0): no dead code, headers internal-only, static-link default
                    verified, clean PR.
                    - Dead code: removed superseded build/verify_msvc.cmd (S001-era raw -G Ninja harness,
                      duplicated by preset-driven verify_s004/s005/s005_shared/s007.cmd).
                    - Headers internal-only: no internal header leaks into public include roots; added an
                      include-surface convention note to cmake/artifact-model.cmake (impl under src/<lib>/src,
                      public API under include/vaml/<lib>). NO header moves in S0 (Shouldly.h kept as public
                      test facade; vulkan/tools keep MINIMAL REAL version.h seeds until S161/S287).
                    - Static-link default verified: added message(STATUS "VAML artifact model: STATIC");
                      both lanes printed STATIC and produced static .lib/.a; VAML_BUILD_SHARED stays OFF.
                    - Clean PR: tracked change-set = cmake/artifact-model.cmake + docs/roadmap.md only.
- files_touched   :
    DELETED   build/verify_msvc.cmd           (dead, superseded S001 harness — removed, was gitignored/untracked)
    MODIFIED  cmake/artifact-model.cmake      (S008: resolved VAML_ARTIFACT_MODEL + message(STATUS) log;
                                               include-surface convention comment)
    MODIFIED  docs/roadmap.md                 (S008 [DONE]; S001 note now references preset verify scripts)
    MODIFIED  session-context.md              (this file)
    UNTOUCHED src/{core,vulkan,tools}/include/vaml/*/export.h  (showed as dirty only from pre-existing
               CRLF/LF autocrlf touch — zero content diff vs HEAD)
- build_status    : msvc-debug (VS 2026) GREEN via verify_s007.cmd — configure "VAML artifact model:
                    STATIC", static vaml-{core,vulkan,tools}.lib linked, ctest 1/1 PASS. REAL LINUX
                    clang-debug on Ubuntu VM (clang 21.1.8) via /mnt/hgfs/VamlPOC GREEN — STATIC .a,
                    ctest 1/1 PASS. Only Clang warnings are pre-existing upstream <ciso646> deprecation
                    from vendored third_party/doctest under GCC-15 libstdc++, not VamlPOC source.
- next_step       : S009
- notes           :
    - S008 is a pure REFACTOR GATE; no new symbols and no MINIMAL REAL introduced. Vulkan/tools MINIMAL
      REAL seeds resolved later (S161/S287), never extended here.
    - Sprint-0 SURVIVOR/checkpoint write intentionally NOT done in S008 — deferred to S009 (Q4 decision).
    - The msvc-debug cache had a stale /cygdrive path baked in (created under a Cygwin-PATH shell);
      wiped build/msvc-debug once, then re-ran via verify_s007.cmd's clean-PATH + vcvars env. Always drive
      MSVC lane through build/verify_s*.cmd so VS-bundled cmake/ninja win over Cygwin PATH.
    - export.h files are tracked but content-identical; the dirty flag is autocrlf noise only.
    - No new non-plan/operational md files (per directive); commits pending explicit commitit directive.
