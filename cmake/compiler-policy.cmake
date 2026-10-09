# ============================================================================
# VAML — shared compiler / warning policy module  (S002)
# Extracted verbatim from the S001 top-level definition so every target (core,
# vulkan, tools, app) applies identical strict flags. No flag semantics changed.
#
# Governing docs: docs/adr/ADR-LOG.md (ADR-004 warning policy, ADR-007 CMake).
# The "policy centralized here" note from S001 continues to hold — this module
# IS that single home.
# ============================================================================

# Warning policy is read at policy-application time so a preset can still
# override it. Default: warnings-as-errors ON (same default as S001).
if(NOT DEFINED VAML_WARNINGS_AS_ERRORS)
    set(VAML_WARNINGS_AS_ERRORS ON CACHE BOOL "Treat warnings as errors")
endif()

function(vaml_apply_compiler_policy target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4                      # Level 4 warnings (project-wide standard)
            /permissive-             # strict standards conformance
            /EHsc                    # C++ exceptions, extern "C" nothrow
            /Zc:preprocessor         # conformant preprocessor
            /Zc:__cplusplus          # correct __cplusplus value
        )
        if(VAML_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
        # Suppress benign MSVC noise that is not real conformance issues.
        target_compile_options(${target} PRIVATE /wd4100)   # unreferenced formal (managed per-site)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic
        )
        if(VAML_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
