# VAML — SESSION-CONTEXT (per-run state)

- last_command    : **DEP-MIGRATION COMPLETE (working-tree; no commits) — vendored deps OUT of the repo
                    into a gitignored uniform fetch folder + sibling staging.** ALL FOUR deps (glfw 3.4,
                    vma v3.1.0, doctest v2.4.11, vulkan-headers) MOVED OUT of VamlPOC: repo-root
                    third_party/{glfw,vma} (were UNTRACKED) + tracked third_party/doctest + tracked
                    VamlPOC/third_party/vulkan-headers -> staged in sibling E:\GitRepos\vaml-deps AND
                    populated into the gitignored fetch folder VamlPOC/third_party/deps. Root .gitignore
                    gained `third_party/deps/` -> the deps are now INVISIBLE to git staging/VS "Changes"
                    (git check-ignore confirms; git status shows zero deps entries — the no-bloat goal).
                    VAML_DEPS_DIR (top-level CMakeLists, default = ${CMAKE_SOURCE_DIR}/third_party/deps,
                    overridable) is the uniform source of truth; FindGLFW/FindVMA/FindVulkan resolve from
                    ${VAML_DEPS_DIR}/<dep>. test/unit/CMakeLists SYSTEM-include roots -> ${VAML_DEPS_DIR}.
                    Windows POC vcxproj + Linux POC CMakeLists updated to third_party/deps (Linux POC
                    keeps SYSTEM pkg-config Vulkan; the vendored-vulkan include I added was REVERTED —
                    it shadowed system headers and hit the StdVideo pre-include requirement; POC is a
                    system-stack consumer). VERIFIED GREEN from first principles on the FINAL migrated
                    state: windows-msvc (msvc-debug configure/build warning-clean + ctest 2/2 PASS) +
                    ubuntu-clang (/mnt/hgfs/VamlPOC clang-debug configure/build warning-clean + ctest 2/2
                    PASS; VM sees deps via the shared-folder mirror /mnt/hgfs/VamlPOC/third_party/deps —
                    NO topology change needed) + Windows POC msbuild EXIT=0 + Linux POC build EXIT=0.
                    All prior S160/S161 deliverables unchanged. No commits.
- plan_repo_state : deps-migration in WORKING TREE only (nothing committed). Deletions pending (the
                    tracked doctest + vulkan-headers show as `D`); the sibling E:\GitRepos\vaml-deps is
                    the FUTURE public `vaml-deps` repo source. COMMIT PLAN (to get it right the first
                    time, per user): (1) make vaml-deps PUBLIC on GitHub + push its 4 dep trees + their
                    upstream LICENSE/COPYING files (GLFW zlib, VMA MIT, doctest MIT, vulkan-headers
                    Apache-2.0 — all permit public redistribution); (2) VamlPOC CI/runners/VM/laptop
                    bootstrap = clone the public vaml-deps into third_party/deps (the accepted
                    marker-file / gitignored-fetch design, NOT a submodule — submodule would need a
                    committed gitlink and conflicts with the no-commit stance + .gitignore semantics);
                    (3) commit the removal (doctest + vulkan-headers deletions) + CMake/Find*/test/POC
                    rewires + .gitignore + CI bootstrap — the "removal requires a commit" truth;
                    (4) push vaml-deps BEFORE VamlPOC so GH runners fetch a reachable source. POC
                    removal (folders VamlPOC/ + VamlPOC-linux/) is independent, user may start it when
                    ready.
- plan_repo_state : **S160 + S161 COMPLETE — Sprint 4 window + Vulkan/VMA linkage + S003 vulkan marker
                    resolved.** CHECKPOINT (progress.md) still reads last = S147 / next = S160 — progress.md
                    was NOT updated this sprint (roadmap + session-context only; progress.md SURVIVOR is
                    the next wrap step's job, matching the S139/S147 docs-only-wrap precedent). Roadmap:
                    S160 [DONE] + S161 [DONE]; GUARD RAILS active-minimal-real list updated (vulkan ->
                    RESOLVED at S161); S003 line + NO-GAP forward-resolutions updated; S162 is the next
                    unimplemented token. No commits.
- plan_digest     : S160 (dep:S147) = MOD CMakeLists.txt (VAML_DEPS_DIR + include FindGLFW/Vulkan/VMA) +
                    ADDED cmake/FindGLFW.cmake + ADDED src/vulkan/include/vaml/vulkan/PlatformWindow.h +
                    ADDED src/vulkan/src/PlatformWindow.cpp + ADDED test/unit/PlatformWindow_test.cpp +
                    MOD src/vulkan/CMakeLists.txt + MOD test/unit/CMakeLists.txt (+vaml-vulkan-tests) +
                    ADDED build/verify_s160.cmd + MOD docs/roadmap.md. S161 (dep:S160) = ADDED
                    cmake/FindVulkan.cmake + cmake/FindVMA.cmake + ADDED src/vulkan/src/VmaImpl.cpp +
                    MOD src/vulkan/CMakeLists.txt + MOD version.h + DEL version.cpp + ADDED
                    build/verify_s161.cmd + MOD docs/roadmap.md. DEP-MIGRATION = MOVED all 4 deps OUT of
                    VamlPOC -> sibling vaml-deps + gitignored third_party/deps; MOD CMakeLists.txt
                    (VAML_DEPS_DIR default third_party/deps) + Find*/test/POC-POC rewire to VAML_DEPS_DIR;
                    MOD .gitignore (third_party/deps/). Both POCs route window through PlatformWindow.
- files_touched   :
    MODIFIED CMakeLists.txt                             (VAML_DEPS_DIR default ${CMAKE_SOURCE_DIR}/third_party/deps + include FindGLFW/Vulkan/VMA)
    ADDED     cmake/FindGLFW.cmake                      (vendored VAML_DEPS_DIR/glfw build -> vaml::glfw; VAML_USE_SYSTEM_GLFW hatch)
    ADDED     cmake/FindVulkan.cmake                    (VAML_DEPS_DIR/vulkan-headers authority + runtime loader -> vaml::vulkan)
    ADDED     cmake/FindVMA.cmake                       (VAML_DEPS_DIR/vma/vk_mem_alloc.h -> vaml::vma)
    ADDED     src/vulkan/include/vaml/vulkan/PlatformWindow.h
    ADDED     src/vulkan/src/PlatformWindow.cpp
    ADDED     src/vulkan/src/VmaImpl.cpp
    MODIFIED  src/vulkan/CMakeLists.txt                 (src list: PlatformWindow.cpp + VmaImpl.cpp; link vaml::glfw/vulkan/vma)
    MODIFIED  src/vulkan/include/vaml/vulkan/version.h  (S003 sentinel REMOVED; real version() kept)
    DELETED   src/vulkan/src/version.cpp                 (empty-TU anchor removed; real compiled symbols now)
    MODIFIED  src/CMakeLists.txt                          (comments: vulkan surface real, tools marker -> S287)
    ADDED     test/unit/PlatformWindow_test.cpp           (S160 headless callback-surface tests)
    MODIFIED  test/unit/CMakeLists.txt                    (+vaml-vulkan-tests exec; SYSTEM-include roots -> ${VAML_DEPS_DIR})
    MODIFIED  VamlPOC/VamlPOC.cpp                         (window via PlatformWindow; S160 callback echo)
    MODIFIED  VamlPOC/VamlPOC.vcxproj                     (link vaml-vulkan.lib + glfw3.lib; vulkan+glfw includes -> third_party/deps; /MT[d] CRT)
    MODIFIED  VamlPOC-linux/src/main.cpp                  (window via PlatformWindow; S160 callback echo)
    MODIFIED  VamlPOC-linux/CMakeLists.txt                (link libvaml-vulkan.a; glfw include -> third_party/deps; SYSTEM pkg-config Vulkan)
    MOVED     third_party/{glfw,vma}                       (UNTRACKED deps -> sibling vaml-deps staging + gitignored third_party/deps)
    MOVED     third_party/doctest                          (tracked dep -> same; deletion pending commit)
    MOVED     VamlPOC/third_party/vulkan-headers           (tracked dep -> same; deletions pending commit)
    ADDED     third_party/deps/                            (gitignored uniform fetch folder — NOT tracked via .gitignore)
    MODIFIED  .github/workflows/ci.yml                     (ubuntu: libglfw3-dev + libvulkan-dev + xorg-dev)
    MODIFIED  .gitignore                                   (third_party/deps/ + build-*/ catch)
    MODIFIED  docs/roadmap.md                              (S160/S161 DONE; vulkan MINIMAL REAL -> RESOLVED)
    MODIFIED  session-context.md (this)
- build_status    : **GREEN — BOTH CI LANES VERIFIED FROM FIRST PRINCIPLES.** windows-msvc
                    (build/verify_s161.cmd, msvc-debug): configure warning-clean + build warning-as-error
                    clean (vaml-core + vaml-vulkan + vaml-tools + glfw + both test exes) + ctest 1/1 PASS
                    (vaml-core-tests + vaml-vulkan-tests both pass). ubuntu-clang (/mnt/hgfs/VamlPOC
                    clang-debug on the VM after sudo apt libglfw3-dev libvulkan-dev + X11 dev): configure
                    clean + build warning-clean (only pre-existing upstream <ciso646> doctest deprecation,
                    non-error) + ctest 1/1 PASS. windows-clang-debug is NOT a lane (pre-existing
                    /Zc:preprocessor failure; not built/tested/flagged per standing rule).
- next_step       : **COMMIT PLAN** (get it right the first time — per user, no commits yet): (1) create +
                    push PUBLIC `vaml-deps` on GitHub (E:\GitRepos\vaml-deps holds the 4 dep trees; keep
                    each upstream's LICENSE/COPYING — GLFW zlib, VMA MIT, doctest MIT, vulkan-headers
                    Apache-2.0); (2) wire bootstrap/CI fetch so each host clones vaml-deps into
                    third_party/deps (accepted gitignored-fetch design — NOT a submodule); (3) commit the
                    dep removal + all rewires + .gitignore + CI bootstrap; (4) push order: vaml-deps
                    BEFORE VamlPOC so GH runners fetch a reachable source. THEN next roadmap token:
                    **S162 (dep:S161)** "Vulkan instance + surface + physical device + queue family
                    selection; validation layers debug-only" — consumes vaml::vulkan headers + runtime
                    loader + PlatformWindow::native_handle(). POC removal is independent (user may start).
                    NO-GAP to carry: WindowCallbacks->routed events S323/S324; resize-safe swapchain S167;
                    VMA consumed S163/S202; live-window run needs a desktop host.
- notes           :
    - Dependency layout FINALIZED (this run): deps live OUTSIDE the repo in a gitignored uniform
      folder VamlPOC/third_party/deps (root-level .gitignore `third_party/deps/`; git check-ignore
      confirms — VS "Changes" no longer lists them; NO repo bloat, and historically-committed doctest
      + vulkan-headers show only as pending `D` for the commit-plan removal). Sibling E:\GitRepos\
      vaml-deps is the staging/future-public source. VAML_DEPS_DIR (default third_party/deps,
      overridable -DVAML_DEPS_DIR) is the single source of truth consumed by Find*/test/POCs — uniform
      on ALL FOUR hosts (GH runners, VM via /mnt/hgfs/VamlPOC mirror, laptop, Windows).
    - The user's accepted model: gitignored fetch folder + per-host clone of public vaml-deps (with a
      tracked marker/URL in a future commit) — NOT a submodule. Submodule was rejected (needs a tracked
      gitlink + conflicts with .gitignore semantics + the no-bloat/no-commit stance). "removal requires
      a commit" is accepted; "no repo bloat" + "git gives the fetch for free but submodule isn't free"
      resolved in favor of the gitignore+clone design.
    - All-vendor/no-drift stands: GLFW 3.4 + VMA v3.1.0 + Vulkan headers vendored (identical source on
      every host); Vulkan runtime is the per-OS loader (vulkan-1.dll runtime on Win / libvulkan.so via
      pkg-config on Linux). The vendored vulkan-headers version needs the vk_video std pre-includes
      before vulkan.h (VmaImpl.cpp + VulkanLoader.h do this; the Linux POC uses SYSTEM pkg-config Vulkan
      instead and was REVERTED from the vendored include to avoid that conflict).
    - Live window "run" proven manually via POCs (desktop host needed; headless VM has no DISPLAY —
      documented). Windows POC msbuild EXIT=0, Linux POC build EXIT=0, both lanes ctest 2/2 green.
    - COMMITIT STATUS: cumulative work through S161 + the dep-migration is outstanding (S147 + S160 +
      S161 + migration). No commits made (per command). The commit-plan ordering (vaml-deps public +
      push-before-VamlPOC) is recorded to avoid CI fetch-timing failures.
