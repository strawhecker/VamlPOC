# ============================================================================
# vaml-compile.cmake — the SHARED vaml_compile_vaml() custom-command module.
# S444 (dep:S443) — moved OUT of src/tools/CMakeLists.txt into this COMMON
# cmake folder so BOTH build systems consume ONE definition (NO duplication,
# NO GAP):
#
#   * the top-level library build (src/tools/CMakeLists.txt includes it — the
#     S287 vamlcc demo + the S444 MainView compile both call vaml_compile_vaml),
#   * the STANDALONE Linux consumer app (vaml_viewer-linux/CMakeLists.txt
#     includes it — the whole point of the "common folder" directive: a
#     function the standalone project previously could NOT call because it was
#     buried in the top-level library CMake).
#
# The function wires a build-time vamlcc custom command per .vaml → a .vbaml
# sidecar (and, optionally, an EMBEDDED C++ twin via --embed). It is the S287
# integration repeated at every consumer that needs a compiled/embedded VAML
# resource, sharing ONE implementation.
#
# The vamlcc executable is referenced via VAMLCC_EXE (a generated-expression
# or a path). When omitted it defaults to `$<TARGET_FILE:vamlcc>` (the library
# build's own target); the standalone apps pass their prebuilt vamlcc path.
# ============================================================================

if(COMMAND vaml_compile_vaml)
    # Already defined (a parent/grandparent scope included us) — never
    # re-define. This makes the module idempotent when a tree is configured
    # through overlapping includes.
    return()
endif()

# ----------------------------------------------------------------------------
# vaml_compile_vaml(<target> ...)
#   NAME      <symbol>       the embed symbol fragment (default = target name)
#   VAML      <in.vaml>      the VAML source
#   OUTPUT    <out.vbaml>    the side-car .vbaml path
#   VAMLCC_EXE <exe>         the vamlcc executable (a path or a generator
#                            expression). Default: $<TARGET_FILE:vamlcc>.
#   EMBED_ON  <h> <cpp>      (optional) also generate the embedded C++ pair
#   DEPS      <extra deps>   (optional) extra DEPENDS for the .vaml input
#   ADD_EMBED_DEF            (optional) define VAML_<NAME>_EMBED with the span
#
# Issues an add_custom_command(OUTPUT <out.vbaml> COMMAND vamlcc <in.vaml>
# <out.vbaml> DEPENDS in.vaml vamlcc) so the .vbaml sidecar is built on demand
# and re-runs when either the source or the tool changes. If EMBED_ON is given,
# a SECOND add_custom_command(OUTPUT <h> <cpp> COMMAND vamlcc --embed
# <out.vbaml> <h> <cpp> <symbol> DEPENDS out.vbaml vamlcc) produces the
# embedded twin. The <target> gains a custom target that owns these outputs and
# is a build-order dependency of the caller.
#
# The embedded + sidecar artifacts are the SAME bytes (tests prove the embed
# span == the sidecar file) — dev uses the sidecar (reload/live tooling), the
# release app uses the embedded span (single-exe path).
# ============================================================================
function(vaml_compile_vaml TGT)
    cmake_parse_arguments(ARG "" "NAME;VAML;OUTPUT;VAMLCC_EXE" "EMBED_ON;DEPS;ADD_EMBED_DEF" ${ARGN})

    if(NOT ARG_VAML OR NOT ARG_OUTPUT)
        message(FATAL_ERROR "vaml_compile_vaml: VAML and OUTPUT are required")
    endif()
    set(SYM "${ARG_NAME}")
    if(NOT SYM)
        string(MAKE_C_IDENTIFIER "${TGT}" SYM)
    endif()

    # Resolve the vamlcc executable. When the caller passes VAMLCC_EXE it wins
    # (the standalone consumer apps pass their prebuilt tool). Otherwise the
    # library build's own vamlcc target is referenced via a generator expr.
    if(ARG_VAMLCC_EXE)
        set(VAMLCC_EXE "${ARG_VAMLCC_EXE}")
    else()
        set(VAMLCC_EXE "$<TARGET_FILE:vamlcc>")
    endif()

    add_custom_command(
        OUTPUT "${ARG_OUTPUT}"
        COMMAND "${VAMLCC_EXE}" "${ARG_VAML}" "${ARG_OUTPUT}"
        DEPENDS "${ARG_VAML}" ${ARG_DEPS}
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
            DEPENDS "${ARG_OUTPUT}" ${ARG_DEPS}
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
