# src/tools — vaml-tools library (compiler tooling; core layer, no Vulkan; ADR-011).
# S003: target instantiated via vaml_add_library() under the artifact model, with a
# MINIMAL REAL version marker (see docs/roadmap.md "Guard Rails"). The real vamlcc
# tooling lands at Sprint 7 (S287) which RESOLVES this marker.
#
# S280 (dep:S255): the vamlcc compiler skeleton lands HERE (the S287-resolution
# content arrives at S287; the library's MINIMAL REAL marker is kept INTACT and
# resolved at S287 — NO-GAP, no premature tag removal). The compiler is:
#   Vamlc.h/.cpp            — the compile entry points + shared compiled form
#   BindingValidator.h/.cpp — S280_1 static binding validation (ADR-016)
#   vbaml/VbamlSchema.*     — S281 versioned binary record-set schema
#   vbaml/VbamlSerializer.* — S282 byte-identical serializer/deserializer
# see the individual file headers for the per-sprint scope.
vaml_add_library(tools
    src/version.cpp
    src/Vamlc.cpp
    src/BindingValidator.cpp
    src/vbaml/VbamlSchema.cpp
    src/vbaml/VbamlSerializer.cpp
    src/VBamlReader.cpp   # S285 (dep:S284) — the compiled-path runtime reader (bytes -> tree)
    src/parity/DualPathParity.cpp   # S288 (dep:S287) — dual-path parity comparator (test-support)
    src/Pack.cpp          # S289 (dep:S288) — pack/identity: PackUri + ResourcePack + resolve_pack_uri
                          #                       (the embedded-resource lookup + the x:Class owner release seam)
)

# vamlcc links vaml-core (the S2 scanner + factory/value/property tables it
# parses and resolves against). It is CORE-layer (no Vulkan — the ADR-011 guard
# validates this link stays Vulkan-free).
target_link_libraries(vaml-tools PRIVATE vaml-core)

# The vamlcc executable: a thin CLI shell over vaml::tools::run_vamlcc (S280).
# Target name "vamlcc", output vamlcc[.exe].
add_executable(vamlcc vamlcc_main.cpp)
target_link_libraries(vamlcc PRIVATE vaml-tools)
# The executable shares the tools public include (vaml/tools/Vamlc.h).
target_include_directories(vamlcc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Hold the executable to the same strict bar as the libraries.
vaml_apply_compiler_policy(vamlcc)

# ============================================================================
# S287 (dep:S286) + S444 (dep:S443) — vamlcc integrated in CMake: a custom
# command per .vaml → a .vbaml sidecar (and, optionally, an EMBEDDED C++ twin
# via --embed). The reusable `vaml_compile_vaml()` function lives in the COMMON
# cmake folder (cmake/vaml-compile.cmake) so BOTH the top-level library build
# AND the standalone Linux viewer app consume ONE definition (NO duplication —
# the S444 "common folder" directive). See that module for the full contract.
# ============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/vaml-compile.cmake)

# ----------------------------------------------------------------------------
# S287 demo + CI test: a small consumer target that compiles a sample .vaml → a
# .vbaml sidecar at build time AND generates the embedded C++ pair. The S287
# unit test (Vamlc_s287_test.cpp) loads the sidecar via the S285 reader and
# asserts its span == the embedded bytes (both artifacts produced by the CMake
# custom command, cross-platform on both CI lanes).
# ----------------------------------------------------------------------------
set(S287_DEMO_VAML "${CMAKE_CURRENT_SOURCE_DIR}/samples/demo_main.vaml")
set(S287_DEMO_VBAML "${CMAKE_CURRENT_BINARY_DIR}/demo_main.vbaml")
set(S287_DEMO_EMBED_H "${CMAKE_CURRENT_BINARY_DIR}/demo_main_vbaml.h")
set(S287_DEMO_EMBED_CPP "${CMAKE_CURRENT_BINARY_DIR}/demo_main_vbaml.cpp")

# Only wire the demo + its embed when the sample file exists (it ships with the
# tree; the custom commands need a real input).
if(EXISTS "${S287_DEMO_VAML}")
    vaml_compile_vaml(vamlcc_demo_main
        VAML    "${S287_DEMO_VAML}"
        OUTPUT  "${S287_DEMO_VBAML}"
        EMBED_ON "${S287_DEMO_EMBED_H}" "${S287_DEMO_EMBED_CPP}"
        NAME    "demo_main"
    )
    # An interface target the S287 test links so it sees the generated embed
    # header + the generated source (ADR-021: compile the ACTUAL generated code,
    # not a copy).
    add_library(vamlcc_demo_main_embed INTERFACE)
    target_sources(vamlcc_demo_main_embed INTERFACE "${S287_DEMO_EMBED_CPP}")
    target_include_directories(vamlcc_demo_main_embed INTERFACE
        "${CMAKE_CURRENT_BINARY_DIR}")
    target_link_libraries(vamlcc_demo_main_embed INTERFACE vaml-tools)
    # Expose the generated sidecar path + symbol to the test via compile defs.
    target_compile_definitions(vamlcc_demo_main_embed INTERFACE
        S287_DEMO_VBAML="${S287_DEMO_VBAML}"
        S287_DEMO_EMBED_H="${S287_DEMO_EMBED_H}"
    )
    add_dependencies(vamlcc_demo_main_embed vamlcc_demo_main)

    # Export the generated embed source + header + sidecar path + include dir to
    # global scope so test/unit/vaml-tools-tests can compile the ACTUAL generated
    # .cpp as one of its sources (ADR-021 single source of truth).
    set(VAML_S287_EMBED_CPP "${S287_DEMO_EMBED_CPP}" CACHE INTERNAL "")
    set(VAML_S287_EMBED_HDR "${S287_DEMO_EMBED_H}" CACHE INTERNAL "")
    set(VAML_S287_EMBED_INC "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")
    set(VAML_S287_EMBED_SIDECAR "${S287_DEMO_VBAML}" CACHE INTERNAL "")
    set(VAML_S287_EMBED_LIB "vamlcc_demo_main_embed" CACHE INTERNAL "")
endif()

# ----------------------------------------------------------------------------
# S444 (dep:S443) — the vamlcc compile step for the app-shell's authored
# MainView.vaml. At build time vamlcc compiles vaml_viewer/MainView.vaml → a
# .vbaml SIDE-CAR + an EMBEDDED C++ twin (--embed). This is the RELEASE-path
# `.vbaml` the `--compiled --app` app-shell launch loads (the S444 compiled
# seam + the S289 InitializeComponentFrom embedded path), while the plain
# `--app` launch keeps loading the authored TEXT via the S440/S443 reader path.
# The S444 test (AppShellCompile_test.cpp) proves the embed span == the sidecar
# bytes AND that the .vbaml decodes through the S288 dual-path comparator to the
# same live tree the text path builds.
# ----------------------------------------------------------------------------
set(S444_APP_SHELL_VAML "${CMAKE_CURRENT_SOURCE_DIR}/../../vaml_viewer/MainView.vaml")
set(S444_APP_SHELL_VBAML "${CMAKE_CURRENT_BINARY_DIR}/MainView.vbaml")
set(S444_APP_SHELL_EMBED_H "${CMAKE_CURRENT_BINARY_DIR}/app_shell_MainView_vbaml.h")
set(S444_APP_SHELL_EMBED_CPP "${CMAKE_CURRENT_BINARY_DIR}/app_shell_MainView_vbaml.cpp")

if(EXISTS "${S444_APP_SHELL_VAML}")
    vaml_compile_vaml(vamlcc_app_shell
        VAML    "${S444_APP_SHELL_VAML}"
        OUTPUT  "${S444_APP_SHELL_VBAML}"
        EMBED_ON "${S444_APP_SHELL_EMBED_H}" "${S444_APP_SHELL_EMBED_CPP}"
        NAME    "app_shell_MainView"
        DEPS    "${CMAKE_CURRENT_SOURCE_DIR}/../../vaml_viewer/app_shell.h"
    )
    # The embedded twin the test + the embedded app-shell path consume (ADR-021:
    # compile the ACTUAL generated code, not a copy). Exported to global scope so
    # test/unit/vaml-tools-tests can compile the generated .cpp + resolve the
    # generated header + assert the sidecar == the span.
    set(VAML_S444_EMBED_CPP "${S444_APP_SHELL_EMBED_CPP}" CACHE INTERNAL "")
    set(VAML_S444_EMBED_HDR "${S444_APP_SHELL_EMBED_H}" CACHE INTERNAL "")
    set(VAML_S444_EMBED_INC "${CMAKE_CURRENT_BINARY_DIR}" CACHE INTERNAL "")
    set(VAML_S444_EMBED_SIDECAR "${S444_APP_SHELL_VBAML}" CACHE INTERNAL "")
    set(VAML_S444_EMBED_INPUT_VAML "${S444_APP_SHELL_VAML}" CACHE INTERNAL "")
endif()

