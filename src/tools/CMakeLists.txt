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
# S287 (dep:S286) — vamlcc integrated in CMake: a custom command per .vaml →
# a .vbaml sidecar (and, optionally, an EMBEDDED C++ twin via --embed).
#
# vaml_compile_vaml(<target> ...) — reusable function that wires the build:
#   NAME      <symbol>       the embed symbol fragment (default = target name)
#   VAML      <in.vaml>      the VAML source
#   OUTPUT    <out.vbaml>    the side-car .vbaml path
#   EMBED_ON  <h> <cpp>      (optional) also generate the embedded C++ pair
#   DEPS      <extra deps>   (optional) extra DEPENDS for the .vaml input
#   ADD_EMBED_DEF            (optional) define VAML_<NAME>_EMBED with the span
#
# The function issues an add_custom_command(OUTPUT <out.vbaml> COMMAND vamlcc
# <in.vaml> <out.vbaml> DEPENDS in.vaml vamlcc) so the .vbaml sidecar is built
# on demand and re-runs when either the source or the tool changes. If
# EMBED_ON is given, a SECOND add_custom_command(OUTPUT <h> <cpp> COMMAND
# vamlcc --embed <out.vbaml> <h> <cpp> <symbol> DEPENDS out.vbaml vamlcc)
# produces the embedded twin. Both commands depend on the vamlcc EXECUTABLE so
# a tool rebuild re-runs them. The <target> gains a custom target that owns
# these outputs and is a build-order dependency of the caller.
#
# The embedded + sidecar artifacts are the SAME bytes (a test proves the embed
# span == the sidecar file) — dev uses the sidecar (reload/live tooling), the
# release app uses the embedded span (single-exe path). Both lanes test it.
# ============================================================================
function(vaml_compile_vaml TGT)
    cmake_parse_arguments(ARG "" "NAME;VAML;OUTPUT" "EMBED_ON;DEPS;ADD_EMBED_DEF" ${ARGN})

    if(NOT ARG_VAML OR NOT ARG_OUTPUT)
        message(FATAL_ERROR "vaml_compile_vaml: VAML and OUTPUT are required")
    endif()
    set(SYM "${ARG_NAME}")
    if(NOT SYM)
        string(MAKE_C_IDENTIFIER "${TGT}" SYM)
    endif()

    # The full path to the built vamlcc executable ($<TARGET_FILE:vamlcc>).
    # Generator expressions ARE expanded in add_custom_command COMMAND lines, so
    # the custom command runs the ACTUAL build artifact on every platform — NOT
    # a bare `vamlcc` that only resolves when the exe dir is on PATH (Windows
    # vcvars) but FAILS on Linux ("vamlcc: not found").
    set(VAMLCC_EXE "$<TARGET_FILE:vamlcc>")

    add_custom_command(
        OUTPUT "${ARG_OUTPUT}"
        COMMAND "${VAMLCC_EXE}" "${ARG_VAML}" "${ARG_OUTPUT}"
        DEPENDS "${ARG_VAML}" vamlcc ${ARG_DEPS}
        COMMENT "vamlcc: ${ARG_VAML} -> ${ARG_OUTPUT}"
        VERBATIM
    )

    set(OUTPUTS "${ARG_OUTPUT}")

    if(ARG_EMBED_ON)
        list(LENGTH ARG_EMBED_ON N_EMBED)
        if(NOT N_EMBED EQUAL 2)
            message(FATAL_ERROR "vaml_compile_vaml: EMBED_ON needs <h> <cpp>")
        endif()
        list(GET ARG_EMBED_ON 0 H)
        list(GET ARG_EMBED_ON 1 CPP)
        add_custom_command(
            OUTPUT "${H}" "${CPP}"
            COMMAND "${VAMLCC_EXE}" --embed "${ARG_OUTPUT}" "${H}" "${CPP}" "${SYM}"
            DEPENDS "${ARG_OUTPUT}" vamlcc
            COMMENT "vamlcc --embed: ${ARG_OUTPUT} -> ${CPP}"
            VERBATIM
        )
        list(APPEND OUTPUTS "${H}" "${CPP}")
    endif()

    add_custom_target("${TGT}" ALL DEPENDS ${OUTPUTS})

    if(ARG_ADD_EMBED_DEF AND ARG_EMBED_ON)
        list(GET ARG_EMBED_ON 0 H)
        list(GET ARG_EMBED_ON 1 CPP)
        add_library("${TGT}_embed" INTERFACE)
        target_sources("${TGT}_embed" INTERFACE "${CPP}")
        target_include_directories("${TGT}_embed" INTERFACE
            "${CMAKE_CURRENT_BINARY_DIR}")
        # The consumer may request a macro; expose the span accessor declarably.
        target_compile_definitions("${TGT}_embed" INTERFACE
            "VAML_${SYM}_HEADER=\"${H}\"")
        set("${TGT}_EMBED_LIB" "${TGT}_embed" PARENT_SCOPE)
    endif()
endfunction()

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

