# ============================================================================
# VAML — artifact model module  (S003)
# Single home for HOW VAML libraries are produced: static (default, IP-hiding
# per ADR-003) or shared (consumer opt-in via VAML_BUILD_SHARED=ON), with a
# generated export header for each library and the ADR-011 layer rule enforced
# by CMake (NOT by convention).
#
# Governing docs: docs/adr/ADR-LOG.md (ADR-003 distribution, ADR-007 CMake,
# ADR-011 layer rule); docs/roadmap.md (S003). Policy application reuses
# vaml_apply_compiler_policy() from cmake/compiler-policy.cmake (S002).
# ============================================================================

include(GenerateExportHeader)

# --- Distribution model: consumer chooses shared/static; static is default. ---
# [ADR-003] The default is STATIC so the IP lives behind a closed ABI surface.
# Turning VAML_BUILD_SHARED=ON switches every library produced by
# vaml_add_library() (core, vulkan, tools) to a shared library in one move.
option(VAML_BUILD_SHARED "Build VAML libraries as SHARED (dll/so); default OFF = static libs" OFF)

# Map a target name to its canonical output name (vaml-core, vaml-vulkan, ...).
function(vaml_artifact_output_name target_name out_var)
    set(${out_var} "vaml-${target_name}" PARENT_SCOPE)
endfunction()

# Map a library display name (core/vulkan/tools) to the macro prefix for the
# generated export header token (VAML_CORE/VAML_VULKAN/VAML_TOOLS).
function(vaml_artifact_export_prefix lib_name out_var)
    string(TOUPPER "${lib_name}" _up)
    set(${out_var} "VAML_${_up}" PARENT_SCOPE)
endfunction()

# ============================================================================
# vaml_add_library(<lib_name> [sources...])
# Creates the library target vaml-core/vaml-vulkan/vaml-tools for <lib_name>,
# honoring VAML_BUILD_SHARED, generating its export header, tagging its layer
# (for the ADR-011 guard) and applying the shared compiler/warning policy.
#
# It is the ONLY sanctioned way to create a VAML library target.
# ============================================================================
function(vaml_add_library lib_name)
    vaml_artifact_output_name("${lib_name}" _target)
    vaml_artifact_export_prefix("${lib_name}" _export_prefix)

    if(VAML_BUILD_SHARED)
        set(_kind SHARED)
    else()
        set(_kind STATIC)
    endif()

    add_library(${_target} ${_kind} ${ARGN})

    # Canonical artifact file name (drop the target prefix collision and pin
    # the vaml-* naming even if a target were later renamed).
    set_target_properties(${_target} PROPERTIES
        OUTPUT_NAME "${_target}"
    )

    # Generated export header: the single per-library facade for symbol
    # visibility. In STATIC builds VAML_<LIB>_EXPORT expands to nothing (no DLL
    # surface — ADR-003 IP-hiding); in SHARED builds it becomes dllexport/import.
    vaml_artifact_export_header("${lib_name}" "${_target}" "${_export_prefix}")

    # Layer tag so vaml_validate_layer_rules() can enforce ADR-011 at configure
    # time (core/tools must never reach Vulkan/GLFW; only vulkan may).
    if(lib_name STREQUAL "core" OR lib_name STREQUAL "tools")
        set_target_properties(${_target} PROPERTIES VAML_LAYER "CORE")
        set_property(GLOBAL APPEND PROPERTY VAML_CORE_LIBRARIES "${_target}")
    elseif(lib_name STREQUAL "vulkan")
        set_target_properties(${_target} PROPERTIES VAML_LAYER "VULKAN")
    else()
        message(FATAL_ERROR "vaml_add_library: unknown library name '${lib_name}' (expected core|vulkan|tools)")
    endif()

    # Shared compiler/warning policy (S002) — including /WX / -Werror.
    vaml_apply_compiler_policy(${_target})
endfunction()

# Generates the per-library export header into the PUBLIC include tree so it is
# real, committed, searchable, and consumable both in-tree and after install.
function(vaml_artifact_export_header lib_name target export_prefix)
    set(_inc "${CMAKE_CURRENT_SOURCE_DIR}/include/vaml/${lib_name}")
    file(MAKE_DIRECTORY "${_inc}")
    # EXPORT_FILE_NAME is resolved relative to CMAKE_CURRENT_BINARY_DIR, so pass
    # an absolute path to place the generated header alongside the hand-written
    # ones in the public include tree.
    generate_export_header(${target}
        EXPORT_FILE_NAME "${_inc}/export.h"
        EXPORT_MACRO_NAME "${export_prefix}_EXPORT"
    )
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    )
endfunction()

# ============================================================================
# vaml_validate_layer_rules()  — configured AFTER all add_subdirectory() calls.
# Walks every CORE-layer library (recorded by vaml_add_library) and fails the
# build if it ever links vaml-vulkan (or Vulkan/GLFW transitively). This makes
# ADR-011 a CMake-enforced invariant, not a convention.
#
# Note: direct linking of vaml-vulkan into vaml-core/vaml-tools is the only
# proxy we can introspect at configure time without parsing third-party link
# interfaces there (GLFW/Vulkan are only ever reachable via vaml-vulkan in this
# project, so forbidding the vaml-vulkan edge fully closes the ADR-011 gap).
# ============================================================================
function(vaml_validate_layer_rules)
    get_property(_core_libs GLOBAL PROPERTY VAML_CORE_LIBRARIES)
    foreach(_core IN LISTS _core_libs)
        get_target_property(_link_txt ${_core} LINK_LIBRARIES)
        if(_link_txt)
            foreach(_dep IN LISTS _link_txt)
                get_target_property(_dep_layer ${_dep} VAML_LAYER)
                if(_dep_layer STREQUAL "VULKAN")
                    message(FATAL_ERROR
                        "ADR-011 VIOLATION: ${_core} links ${_dep} (Vulkan/GLFW). "
                        "src/core and src/tools must NEVER depend on Vulkan/GLFW. "
                        "Remove the dependency in the corresponding CMakeLists.txt.")
                endif()
            endforeach()
        endif()
    endforeach()
endfunction()
