# VAML — SESSION-CONTEXT (per-run state)

- last_command    : implementit (S003)
- plan_digest     : S003 — Static-library artifact model (Plan B + guard rails): VAML_BUILD_SHARED
                    (default OFF); vaml-core/vaml-vulkan/vaml-tools via vaml_add_library() helper;
                    generated export headers; ADR-011 CMake-enforced; MINIMAL REAL markers for
                    vulkan/tools (resolved S161/S287); guard-rail terms codified in md.
- files_touched   :
    CREATED  cmake/artifact-model.cmake
    CREATED  src/vulkan/CMakeLists.txt
    CREATED  src/vulkan/include/vaml/vulkan/version.h
    CREATED  src/vulkan/src/version.cpp
    CREATED  src/tools/CMakeLists.txt
    CREATED  src/tools/include/vaml/tools/version.h
    CREATED  src/tools/src/version.cpp
    CREATED  src/{core,vulkan,tools}/include/vaml/{core,vulkan,tools}/export.h  (generated)
    MODIFIED CMakeLists.txt                 (include artifact-model + vaml_validate_layer_rules())
    MODIFIED src/CMakeLists.txt             (un-commented add_subdirectory(vulkan)+add_subdirectory(tools))
    MODIFIED src/core/CMakeLists.txt        (refactored to vaml_add_library(core))
    MODIFIED cmake/presets/configure.json   (added msvc-debug-shared)
    MODIFIED cmake/presets/build.json       (added msvc-debug-shared)
    MODIFIED cmake/presets/test.json        (added msvc-debug-shared)
    MODIFIED docs/roadmap.md   (Guard Rails section + S003/S161/S287 resolution contracts)
    MODIFIED progress.md       (CHECKPOINT S002->S003 + GUARD RAILS block + STEP LOG entry)
- build_status    : STATIC (build/msvc-debug): vaml-core.lib + vaml-vulkan.lib + vaml-tools.lib GREEN,
                    /WX active. SHARED (build/msvc-debug-shared): vaml-core.dll + vaml-vulkan.dll +
                    vaml-tools.dll (with import .libs) GREEN. ADR-011 guard negative-tested -> configure
                    FATAL "ADR-011 VIOLATION", reverted to green. Linux + windows-clang = no-build-on-host.
- next_step       : S004 (CI matrices)
- notes           :
    - No git commands, no commits, no new non-plan md files (per protocol).
    - GUARD RAILS now codified: MINIMAL REAL, NO-GAP GUARANTEE, NO LAZY LLM SYNDROME (in roadmap.md
      + progress.md). Active MINIMAL REAL markers: vulkan -> RESOLVED BY S161; tools -> RESOLVED BY S287.
    - GOTCHA: layer guard introspects LINK_LIBRARIES (direct edges) — sufficient because GLFW/Vulkan
      only reachable via vaml-vulkan in this project.
    - GOTCHA: export.h generated into the PUBLIC include tree (absolute EXPORT_FILE_NAME) so consumers
      include vaml/{lib}/export.h.
