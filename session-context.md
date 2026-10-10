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
    - UBUNTU LANE VERIFIED ON REAL LINUX (new): reached the Ubuntu VM over SSH
      (ssh -i E:\GitRepos\VamlPOC\vaml_ci_key robert@192.168.198.129, VMware NAT subnet 192.168.198.0/24,
      hostname vaml, Ubuntu 26.04 resolute, x86_64, 2 CPU / 3.3G RAM). Installed clang 21.1.8,
      ninja 1.13.2, cmake 4.2.3, build-essential, git (sudo via robert; PASSWORDSHARED-NOTSTORED).
      Cloned public origin https://github.com/strawhecker/VamlPOC.git at S004 (e6d689e). Ran the real
      ubuntu-clang lane: cmake --preset clang-debug -> build -> test. CONFIGURE green (Clang 21.1.8,
      Ninja -> build/clang-debug). BUILD green: [1-6/6] compiled+linked vaml-core/vaml-tools/vaml-vulkan
      static libs, ZERO warnings/errors. ctest: no tests yet (S005), exit 0 -> lane green. Generated
      export.h present in each lib's public include tree. -> ubuntu-clang lane is no longer
      no-build-on-host; it is verified pre-push, GH Actions remains the authority.
    - SSH ACCESS MODEL: keypair at E:\GitRepos\VamlPOC\vaml_ci_key (PRIVATE stays on Windows, never
      transferred) / vaml_ci_key.pub (PUBLIC, in VM ~/.ssh/authorized_keys). VM user robert. Password
      used for sudo-gated installs only; do not log it. VM reachable on home LAN via 192.168.198.129;
      Tailscale exists on Windows (100.114.10.67) for traveling VPN later.
