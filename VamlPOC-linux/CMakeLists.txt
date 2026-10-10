# ============================================================================
# VAML POC — Linux app (standalone; NOT part of the main library build)
#
# This is the Linux counterpart to the Windows VamlPOC/ app. It is deliberately
# a SEPARATE CMake project (own root CMakeLists.txt), created "apart from the
# lib": it does NOT get pulled in via the top-level add_subdirectory(src).
#
# It proves the consumer path instead: it links the already-built vaml-core
# static library (from the clang-debug build) and drives the GPU through
# GLFW3 + Vulkan — the stack the roadmap prescribes for the Linux window lane
# (S160: "Platform window via GLFW (Win32 + X11)").
#
# Requires (Ubuntu 20.04): clang, cmake, ninja, libvulkan-dev, vulkan-tools,
# libglfw3-dev, pkg-config.
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

# --- vaml-core: consume the STATIC library from the clang-debug build ---------
# Public include root (the IP-hiding facade per ADR-003) + the built archive.
set(VAML_CORE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/include")
set(VAML_CORE_LIB     "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/core/libvaml-core.a")

add_executable(vamlpoc-linux
    src/main.cpp
)

target_include_directories(vamlpoc-linux PRIVATE
    ${VAML_CORE_INCLUDE}
)

# Link vaml-core (proves the consumer path) + the GPU stack.
target_link_libraries(vamlpoc-linux PRIVATE
    ${VAML_CORE_LIB}
    PkgConfig::GLFW3
    PkgConfig::VULKAN
    dl
)
