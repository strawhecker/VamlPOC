# ============================================================================
# VAML POC — Linux app (standalone; NOT part of the main library build)
#
# This is the Linux counterpart to the Windows VamlPOC/ app. It is deliberately
# a SEPARATE CMake project (own root CMakeLists.txt), created "apart from the
# lib": it does NOT get pulled in via the top-level add_subdirectory(src).
#
# It proves the consumer path instead: it links the already-built vaml-core and
# vaml-vulkan static libraries (from the clang-debug build) and drives the GPU
# through GLFW3 + Vulkan — the stack the roadmap prescribes for the Linux window
# lane (S160: "Platform window via GLFW (Win32 + X11)"). The window is created
# through vaml::vulkan::PlatformWindow (S160's real surface).
#
# Requires (Ubuntu 20.04): clang, cmake, ninja, libvulkan-dev, vulkan-tools,
# libglfw3-dev, pkg-config + the X11 dev packages vaml-vulkan's vendored GLFW
# build pulls in (xorg-dev / libxinerama-dev etc).
# ============================================================================

cmake_minimum_required(VERSION 3.24)

project(VamlPOC-Linux
    VERSION 0.1.0
    DESCRIPTION "VAML POC - Linux GPU app (GLFW3 + Vulkan)"
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
    target_compile_options(vamlpoc-linux PRIVATE /W4 /permissive- /EHsc /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# --- GLFW3 + Vulkan via pkg-config (the GPU lane) ----------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(GLFW3 REQUIRED IMPORTED_TARGET glfw3)
pkg_check_modules(VULKAN REQUIRED IMPORTED_TARGET vulkan)

# --- vaml-core + vaml-vulkan: consume the STATIC libraries from the clang-debug build ----
# vaml-vulkan (S160/S161) is the library that owns the GLFW window and the
# Vulkan/VMA linkage; the POC consumes BOTH the real library surfaces.
# Public include roots (the IP-hiding facade per ADR-003) + the built archives.
set(VAML_CORE_INCLUDE      "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/include")
set(VAML_CORE_LIB          "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/core/libvaml-core.a")
set(VAML_VULKAN_INCLUDE    "${CMAKE_CURRENT_SOURCE_DIR}/../src/vulkan/include")
set(VAML_VULKAN_LIB        "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/vulkan/libvaml-vulkan.a")
# GLFW include (vendored source's public headers) — needed by main.cpp for the
# GLFW_* constants + the pass-through GLFWwindow* calls, kept minimal. Resides in
# the gitignored deps folder third_party/deps (uniform on every host — no repo
# bloat). The Vulkan headers come from the SYSTEM pkg-config `vulkan` (this POC
# drives the system stack); only the vendored GLFW include is added here.
set(VAML_DEPS_DIR           "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/deps")
set(VAML_GLFW_INCLUDE       "${VAML_DEPS_DIR}/glfw/include")

add_executable(vamlpoc-linux
    src/main.cpp
)

target_include_directories(vamlpoc-linux PRIVATE
    ${VAML_CORE_INCLUDE}
    ${VAML_VULKAN_INCLUDE}
    ${VAML_GLFW_INCLUDE}
)

# Link vaml-vulkan (which PUBLIC-links GLFW + Vulkan) + vaml-core + the GPU stack.
target_link_libraries(vamlpoc-linux PRIVATE
    ${VAML_VULKAN_LIB}
    ${VAML_CORE_LIB}
    PkgConfig::GLFW3
    PkgConfig::VULKAN
    dl
)
