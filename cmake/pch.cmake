# ============================================================================
# VAML — shared MSVC precompiled-header module  (S440, Sprint 11)
# Build-speed optimization for the msvc-debug lane. Centralizes the MSVC PCH
# wiring so every vaml_add_library() target + the test executables share ONE
# generated PCH instead of each re-parsing the stable core headers.
#
# Semantics-neutral: PCH is parse-amortization ONLY; every TU is still compiled
# from its own .cpp under the same strict /W4 /WX policy. The shared PCH is
# built once per directory into CMake's per-target PCH slot and reused.
#
# Applied to MSVC (MSVC toolset) targets ONLY. clang-cl preset is NOT a CI lane
# and /Zc:preprocessor stays off there, so this module guards on MSVC (the
# Windows CI lane). GCC/Clang use the classic (slower) path — no behavior change.
# ============================================================================

# Define after vaml_apply_compiler_policy() has run so the /W4 /WX flags are in
# place; target_precompile_headers adds /Yc.../Yu... and keeps the existing flags.
function(vaml_apply_msvc_pch target)
    if(NOT MSVC)
        return()
    endif()
    # The shared PCH header root (this module's directory — CMAKE_CURRENT_
    # FUNCTION_LIST_DIR = the dir of pch.cmake, NOT the caller's subdirectory).
    set(_pch "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/vaml_pch.hpp")
    if(NOT EXISTS "${_pch}")
        message(FATAL_ERROR "vaml_apply_msvc_pch: shared PCH header not found: ${_pch}")
    endif()
    target_precompile_headers(${target} PRIVATE "${_pch}")
endfunction()
