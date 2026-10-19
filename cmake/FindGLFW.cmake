# ============================================================================
# VAML — FindGLFW.cmake  (S160)
#
# Locates the GLFW dependency and exposes it as the interface target `vaml::glfw`.
# Deferred/transparent on the caller: it does NOT create vaml-vulkan; it only
# makes GLFW available so S160's PlatformWindow.cpp can compile+link.
#
# STRATEGY (no-drift, all-vendor): GLFW source is vendored in the unified deps
# root (${VAML_DEPS_DIR}/glfw, pinned to the 3.4 tag) and built IN-TREE via
# add_subdirectory, so every host (GH windows-latest / ubuntu-latest, the VM,
# the laptop) compiles the IDENTICAL source — no per-host package-manager drift.
# Win32 + X11 (+Wayland) come from the one vendored tree. The deps root lives
# OUTSIDE the VamlPOC repo (local sibling vaml-deps during dev, the gitignored
# fetch folder third_party/deps at CI) — so VamlPOC itself stays free of
# dependency bloat. ADR-011: GLFW is only ever reachable via vaml-vulkan.
#
# A VAML_USE_SYSTEM_GLFW escape hatch is provided for consumers who prefer their
# platform's package, but the default is the vendored build.
# ============================================================================

# --- 1) Vendored in-tree build (default) ----------------------------------
# Guard on the TARGET EXISTING (not a cache var) so a partial configure aborted
# before target creation cannot leave a stale short-circuit on the next run.
if(TARGET vaml::glfw)
    return()
endif()

if(NOT VAML_USE_SYSTEM_GLFW)
    set(_vml_glfw_src "${VAML_DEPS_DIR}/glfw")
    if(NOT EXISTS "${_vml_glfw_src}/CMakeLists.txt")
        message(FATAL_ERROR
            "GLFW vendor tree not found at ${_vml_glfw_src}. "
            "Expected the vendored GLFW 3.4 source under VAML_DEPS_DIR/glfw. "
            "Clone the deps or set VAML_USE_SYSTEM_GLFW=ON to use the system package.")
    endif()

    # GLFW's own CMake exposes the target `glfw`. Select only what we need and
    # disable its examples/tests/docs/install to keep the build minimal and
    # warning-free. Backend selection: S160's contract is "Win32 + X11", so on
    # unix we force X11 ON and Wayland OFF — deterministic across hosts, and it
    # keeps the Linux dependency surface to the X11 dev packages only. Save/
    # restore the caller's options so a consumer's project does not leak GLFW's
    # defaults.
    set(_save_glfw_build_examples ${GLFW_BUILD_EXAMPLES})
    set(_save_glfw_build_tests ${GLFW_BUILD_TESTS})
    set(_save_glfw_build_docs ${GLFW_BUILD_DOCS})
    set(_save_glfw_install ${GLFW_INSTALL})
    set(_save_glfw_vulkan ${GLFW_VULKAN_STATIC})
    set(_save_glfw_build_wayland ${GLFW_BUILD_WAYLAND})
    set(_save_glfw_build_x11 ${GLFW_BUILD_X11})
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS     OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS      OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL         OFF CACHE BOOL "" FORCE)
    # On UNIX (non-APPLE), force X11 ON and Wayland OFF — S160's contract is
    # "Win32 + X11", so the Linux backend surface is deterministic and limited
    # to the X11 dev packages. On Windows, GLFW's own cmake_dependent_option
    # already selects Win32 (X11 is auto-OFF there).
    if(UNIX AND NOT APPLE)
        set(GLFW_BUILD_X11   ON  CACHE BOOL "" FORCE)
        set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
    endif()
    # GLFW's GLFW_VULKAN_STATIC is only meaningful for apps that want to bundle
    # the Vulkan loader; we use the runtime loader, so leave it OFF.
    set(GLFW_VULKAN_STATIC   OFF CACHE BOOL "" FORCE)
    set(_vml_glfw_prev_src ${CMAKE_CURRENT_SOURCE_DIR})
    add_subdirectory("${_vml_glfw_src}" "${CMAKE_CURRENT_BINARY_DIR}/glfw")
    set(_vml_glfw_prev_glfw_src ${CMAKE_CURRENT_SOURCE_DIR})
    # Restore the caller's option values (best effort; only reset the ones we
    # force-set so defaults are not clobbered when the consumer ran first).
    if(DEFINED _save_glfw_build_examples)
        set(GLFW_BUILD_EXAMPLES ${_save_glfw_build_examples} CACHE BOOL "" FORCE)
    endif()
    if(DEFINED _save_glfw_build_tests)
        set(GLFW_BUILD_TESTS ${_save_glfw_build_tests} CACHE BOOL "" FORCE)
    endif()
    if(DEFINED _save_glfw_build_docs)
        set(GLFW_BUILD_DOCS ${_save_glfw_build_docs} CACHE BOOL "" FORCE)
    endif()
    if(DEFINED _save_glfw_install)
        set(GLFW_INSTALL ${_save_glfw_install} CACHE BOOL "" FORCE)
    endif()
    if(DEFINED _save_glfw_vulkan)
        set(GLFW_VULKAN_STATIC ${_save_glfw_vulkan} CACHE BOOL "" FORCE)
    endif()
    if(DEFINED _save_glfw_build_wayland)
        set(GLFW_BUILD_WAYLAND ${_save_glfw_build_wayland} CACHE BOOL "" FORCE)
    endif()
    if(DEFINED _save_glfw_build_x11)
        set(GLFW_BUILD_X11 ${_save_glfw_build_x11} CACHE BOOL "" FORCE)
    endif()

    # Snapshot the vendored build's compile/link into a clean interface target
    # so vaml-vulkan links a stable name regardless of GLFW's own option state.
    add_library(vaml::glfw INTERFACE IMPORTED)
    target_link_libraries(vaml::glfw INTERFACE glfw)
    set(VAML_GLFW_FOUND TRUE CACHE INTERNAL "" FORCE)
    return()
endif()

# --- 2) System package fallback (VAML_USE_SYSTEM_GLFW=ON) ------------------
# Pkg-config path for Linux/other unix hosts with glfw3 installed.
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(GLFW3 QUIET IMPORTED_TARGET glfw3)
    if(GLFW3_FOUND)
        add_library(vaml::glfw INTERFACE IMPORTED)
        target_link_libraries(vaml::glfw INTERFACE PkgConfig::GLFW3)
        set(VAML_GLFW_FOUND TRUE CACHE INTERNAL "" FORCE)
        return()
    endif()
endif()

message(FATAL_ERROR
    "GLFW not found. Either vendor GLFW under VAML_DEPS_DIR/glfw (default) or "
    "install the system glfw3 and set VAML_USE_SYSTEM_GLFW=ON.")
