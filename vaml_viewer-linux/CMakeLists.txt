# ============================================================================
# vaml_viewer-linux — Linux consumer app (standalone; NOT part of the library build)
#
# The Linux counterpart to the Windows vaml_viewer/ app. It is deliberately a
# SEPARATE CMake project (own root CMakeLists.txt), created "apart from the lib":
# it does NOT get pulled in via the top-level add_subdirectory(src).
#
# It proves the consumer path instead: it links the already-built vaml-core,
# vaml-vulkan and vaml-presentation STATIC libraries (from the clang-debug build)
# through ONLY their public include roots (CONSUMER-APP PATTERN, ADR-021 — never
# an internal header under src/<lib>/src). The S220 scaffold consumes only
# vaml-core's version API; later Sprint 5a steps add the vaml-vulkan/presentation
# public surfaces + the GLFW3/Vulkan stack exactly as the deleted POC did.
#
# Requires (Ubuntu): clang, cmake, ninja, libvulkan-dev, vulkan-tools,
# libglfw3-dev, pkg-config + the X11 dev packages vaml-vulkan's vendored GLFW
# build pulls in (xorg-dev / libxinerama-dev etc).
# ============================================================================

cmake_minimum_required(VERSION 3.24)

project(vaml-viewer-linux
    VERSION 0.1.0
    DESCRIPTION "vaml_viewer - Linux VAML consumer app"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# --- Compiler + warnings-as-errors policy (mirror the lib's strict bar) ------
if(MSVC)
    target_compile_options(vaml-viewer-linux PRIVATE /W4 /permissive- /EHsc /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# --- GLFW3 + Vulkan via pkg-config (added by S222+, kept declared-ready) ------
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(VULKAN REQUIRED IMPORTED_TARGET vulkan)

# --- Consume the STATIC libraries from the clang-debug build -----------------
# Public include roots (the CONSUMER-APP PATTERN per ADR-021) + the built
# archives. Link order matters for the single-pass GNU ld: vaml-presentation
# references vaml::vulkan symbols, so it comes BEFORE vaml-vulkan; vaml-vulkan
# before the raw GPU stack; vaml-core is independent.
set(VAML_CORE_INCLUDE          "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/include")
set(VAML_CORE_LIB              "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/core/libvaml-core.a")
set(VAML_VULKAN_INCLUDE        "${CMAKE_CURRENT_SOURCE_DIR}/../src/vulkan/include")
set(VAML_VULKAN_LIB            "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/vulkan/libvaml-vulkan.a")
set(VAML_PRESENTATION_INCLUDE  "${CMAKE_CURRENT_SOURCE_DIR}/../src/presentation/include")
set(VAML_PRESENTATION_LIB      "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/presentation/libvaml-presentation.a")

add_executable(vaml-viewer-linux
    src/main.cpp
)

target_include_directories(vaml-viewer-linux PRIVATE
    ${VAML_CORE_INCLUDE}
    ${VAML_VULKAN_INCLUDE}
    ${VAML_PRESENTATION_INCLUDE}
)

target_link_libraries(vaml-viewer-linux PRIVATE
    ${VAML_PRESENTATION_LIB}
    ${VAML_VULKAN_LIB}
    ${VAML_CORE_LIB}
    PkgConfig::GLFW3
    PkgConfig::VULKAN
    dl
)
