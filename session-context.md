# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit (S004)
- plan_digest     : S004 — CI matrices (.github/workflows): single ci.yml, two live jobs
                    (windows-msvc -> msvc-debug, ubuntu-clang -> clang-debug), rolling -latest
                    runners (windows-latest = VS 2026 cl.exe = local dev), reusing S002/S003
                    presets verbatim, configure+build+ctest green. No windows-clang lane in any
                    form. Non-goals: no artifacts/coverage/packaging.
- files_touched   :
    CREATED  .github/workflows/ci.yml   (sole deliverable of S004)
    MODIFIED docs/roadmap.md   (S004 line marked [DONE] + CI detail)
    MODIFIED progress.md       (CHECKPOINT S003->S004 + STEP LOG entry)
    MODIFIED session-context.md (this file)
- build_status    : No build-system/feature code changed in S004 (YAML-only + md tracking). MSVC
                    lane (preset msvc-debug) re-validated locally green — any preset-name typo in
                    the workflow is caught pre-push. Ubuntu lane = no-build-on-host; acceptance is
                    GH Actions on push/PR (free for public repos). CI green is the acceptance test.
- next_step       : S005 (vendor doctest single-header + test harness + smoke test)
- notes           :
    - No git commands, no commits, no new non-plan md files (per protocol).
    - Workflow presets cross-checked against cmake/presets/{configure,build,test}.json: msvc-debug
      and clang-debug are named identically across all three JSON files (required — CI calls only
      --preset <name>, never CMake directly).
    - MSVC lane needs cl.exe on PATH -> microsoft/setup-msbuild must precede cmake --preset;
      base.json sets Ninja generator -> choco install ninja -y precedes configure.
    - Rolling -latest, no pinned labels: keeps MSVC lane on same VS as local dev and follows future
      migrations (explicitly decided; windows-2022/vs2026 pins rejected).
    - NO windows-clang lane in the workflow (toolchain seed stays unbuilt in cmake/toolchains/);
      no dead/commented lanes per the workflow's header comment.
    - GUARD RAILS unchanged (MINIMAL REAL, NO-GAP, NO LAZY LLM) — still active: vulkan -> S161, tools -> S287.
    - LOCAL VERIFICATION: drove the real msvc-debug preset via build/verify_s004.cmd (create -> VS
      dev shell -> cmake --preset/build/test) — configure + build green; ctest reports no tests yet
      (doctest harness is S005); ctest returns exit 0 for zero matches so the CI lane stays green.
      Ubuntu lane remains no-build-on-host (acceptance in GH Actions).
