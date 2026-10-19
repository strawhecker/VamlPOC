# ============================================================================
# VAML — FindVMA.cmake  (S161)
#
# Locates VulkanMemoryAllocator (VMA) and exposes it as the interface target
# `vaml::vma`. VMA is a single-header library (vk_mem_alloc.h). The compile-time
# interface is the vendored copy in ${VAML_DEPS_DIR}/vma/vk_mem_alloc.h (pinned
# v3.1.0) — identical on every host (no-drift). The deps root lives OUTSIDE
# VamlPOC (local sibling vaml-deps during dev, the gitignored fetch folder
# third_party/deps at CI) so VamlPOC itself stays bloat-free. The Vulkan SDK's
# own copy is an opportunistic fallback only; the vendored header is the
# authority.
# ADR-011: VMA is only ever reachable via vaml-vulkan.
# ============================================================================

if(TARGET vaml::vma)
    return()
endif()

set(_vml_vma_inc "${VAML_DEPS_DIR}/vma")
if(NOT EXISTS "${_vml_vma_inc}/vk_mem_alloc.h")
    # Opportunistic SDK fallback (host-local convenience only, not the dep).
    if(EXISTS "$ENV{VULKAN_SDK}/Include/vk_mem_alloc.h")
        set(_vml_vma_inc "$ENV{VULKAN_SDK}/Include")
    else()
        message(FATAL_ERROR
            "VMA not found. Expected the vendored single header at "
            "VAML_DEPS_DIR/vma/vk_mem_alloc.h. Vendor VMA v3.1.0 to keep the "
            "build drift-free.")
    endif()
endif()

# The implementation TU (src/vulkan/src/VmaImpl.cpp) defines VMA_IMPLEMENTATION
# and includes this header; here we expose only the header path.
add_library(vaml::vma INTERFACE IMPORTED)
target_include_directories(vaml::vma INTERFACE "${_vml_vma_inc}")
set(VAML_VMA_FOUND TRUE CACHE INTERNAL "" FORCE)
