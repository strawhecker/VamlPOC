# ============================================================================
# VAML — FindVulkan.cmake  (S161)
#
# Locates the Vulkan dependency and exposes it as the interface target
# `vaml::vulkan`. Deferred: it does NOT create vaml-vulkan; it makes Vulkan
# available so S161 can link it and S162+ compiles the real surface against it.
#
# STRATEGY (no-drift, all-vendor, SDK-less): the compile-time Vulkan interface
# is the ALREADY-VENDORED header set (vulkan/vulkan.h + friends), which every
# host builds against identically — NO SDK version pinning, so a machine-local
# SDK (e.g. D:\SoftwareLibraries\vulkan) can never introduce header drift. The
# Vulkan RUNTIME is the per-OS stable loader ABI: runtime-resolved vulkan-1.dll
# on Windows (the repo's standing VK_NO_PROTOTYPES design — no import lib), and
# the system libvulkan.so on Linux (pkg-config `vulkan`, supplied by
# libvulkan-dev). The Linux package is used for the LOADER LIBRARY ONLY, never
# as the API-header authority.
#
# Candidate header roots, in priority order. Only the first that actually
# contains vulkan/vulkan.h is used:
#   1. ${VAML_DEPS_DIR}/vulkan-headers/include — the vendored authority (the
#      deps root lives OUTSIDE VamlPOC: local sibling vaml-deps during dev, the
#      gitignored fetch folder third_party/deps at CI — repo stays bloat-free).
#   2. ${VULKAN_SDK}/Include — opportunistic LOCAL convenience ONLY; does NOT
#      count as the dependency (host-local drift); skipped when absent.
# ADR-011: Vulkan is only ever reachable via vaml-vulkan.
# ============================================================================

# Defensive: allow a re-include without duplicate targets.
if(TARGET vaml::vulkan)
    return()
endif()

find_package(PkgConfig QUIET)

# --- Header root selection --------------------------------------------------
set(_vml_vk_include "")
foreach(_cand
        "${VAML_DEPS_DIR}/vulkan-headers/include"
        "$ENV{VULKAN_SDK}/Include")
    if(EXISTS "${_cand}/vulkan/vulkan.h")
        set(_vml_vk_include "${_cand}")
        break()
    endif()
endforeach()

if(NOT _vml_vk_include)
    message(FATAL_ERROR
        "Vulkan headers not found. Expected the vendored header set under "
        "VAML_DEPS_DIR/vulkan-headers/include (or VULKAN_SDK/Include). Vendor "
        "vulkan-headers in the deps root to keep the build drift-free.")
endif()

# --- Loader link ------------------------------------------------------------
set(_vml_vk_link "")
if(WIN32)
    # Runtime loader only (VK_NO_PROTOTYPES + LoadLibrary("vulkan-1.dll")); no
    # import library and no SDK dependency — the repo's standing design.
    set(_vml_vk_runtime on)
else()
    # Linux/unix: link the stable loader ABI via pkg-config (libvulkan.so),
    # supplied by libvulkan-dev. Headers stay the vendored set above.
    if(PkgConfig_FOUND)
        pkg_check_modules(VULKAN_LOADER QUIET IMPORTED_TARGET vulkan)
        if(VULKAN_LOADER_FOUND)
            set(_vml_vk_link PkgConfig::VULKAN_LOADER)
        endif()
    endif()
endif()

add_library(vaml::vulkan INTERFACE IMPORTED)
target_include_directories(vaml::vulkan INTERFACE "${_vml_vk_include}")
if(_vml_vk_link)
    target_link_libraries(vaml::vulkan INTERFACE "${_vml_vk_link}")
    target_compile_definitions(vaml::vulkan INTERFACE VAML_VULKAN_LINKED_LOADER=1)
endif()
set(VAML_VULKAN_FOUND TRUE CACHE INTERNAL "" FORCE)
