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

# --- S444: the SHARED vaml_compile_vaml() module (cmake/vaml-compile.cmake) ----
# The whole point of the "common folder" directive: vaml_compile_vaml() lives in
# the shared cmake/vaml-compile.cmake (included by BOTH the top-level library
# build AND this STANDALONE Linux consumer project), so the Linux app can call it
# here without duplicating the function. It compiles the authored MainView.vaml
# → a .vbaml sidecar + an EMBEDDED C++ twin at build time (the S444 compile
# step; the S287 integration, reused).
include(${CMAKE_CURRENT_SOURCE_DIR}/../cmake/vaml-compile.cmake)

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
set(VAML_TOOLS_INCLUDE         "${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/include")
set(VAML_TOOLS_LIB             "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/tools/libvaml-tools.a")

add_executable(vaml-viewer-linux
    src/main.cpp
    ../vaml_viewer/load_file.cpp   # S221: the shared HEADLESS-ctestable file->tree seam (single source of truth)
    ../vaml_viewer/app.cpp         # S222: the shared "soup-to-nuts" render host (single source of truth)
    ../vaml_viewer/file_watch.cpp  # S222_1: the shared file-watch mtime-poll + notify seam
    ../vaml_viewer/live_reload.cpp # S222_1: the shared live-reload coordinator
    ../vaml_viewer/app_shell_vm.cpp # S441/S442: the app-shell VM + service impls (single source of truth)
    # S224 cli.h + view_transform.h are HEADER-ONLY seams (included from the
    # vaml_viewer include dir below) — no separate TU needed.
)

target_include_directories(vaml-viewer-linux PRIVATE
    ${VAML_CORE_INCLUDE}
    ${VAML_VULKAN_INCLUDE}
    ${VAML_PRESENTATION_INCLUDE}
    ${VAML_TOOLS_INCLUDE}      # S293: <vaml/tools/VBamlReader.h> + <vaml/tools/Vamlc.h> (the compiled surface)
    ${CMAKE_CURRENT_SOURCE_DIR}/../vaml_viewer   # load_file.h + app.h (the shared hosts' headers)
)

target_link_libraries(vaml-viewer-linux PRIVATE
    ${VAML_PRESENTATION_LIB}
    ${VAML_VULKAN_LIB}
    ${VAML_TOOLS_LIB}          # S293: vaml-tools (VBamlReader + vamlcc compile_to_bytes)
    ${VAML_CORE_LIB}
    PkgConfig::GLFW3
    PkgConfig::VULKAN
    dl
)

# ----------------------------------------------------------------------------
# S444 (dep:S443) — the vamlcc compile step for the app-shell's MainView.vaml.
# At build time the prebuilt clang-debug vamlcc executable compiles
# ../vaml_viewer/MainView.vaml → a .vbaml SIDE-CAR + an EMBEDDED C++ twin in
# THIS project's binary dir. The `--compiled --app` launch opens the sidecar via
# the S444 seam; the embed twin is the single-exe path. Uses the SHARED
# vaml_compile_vaml() (cmake/vaml-compile.cmake) — ONE definition, NO
# duplication across the two build systems.
# ----------------------------------------------------------------------------
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../vaml_viewer/MainView.vaml")
    set(S444_LINUX_VBAML "${CMAKE_CURRENT_BINARY_DIR}/MainView.vbaml")
    set(S444_LINUX_EMBED_H "${CMAKE_CURRENT_BINARY_DIR}/app_shell_MainView_vbaml.h")
    set(S444_LINUX_EMBED_CPP "${CMAKE_CURRENT_BINARY_DIR}/app_shell_MainView_vbaml.cpp")
    # The prebuilt clang-debug vamlcc (the standalone app consumes prebuilt libs
    # per ADR-021; there is no vamlcc TARGET in this project to reference).
    set(S444_LINUX_VAMLCC "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/tools/vamlcc")
    vaml_compile_vaml(vamlcc_app_shell_linux
        VAML       "${CMAKE_CURRENT_SOURCE_DIR}/../vaml_viewer/MainView.vaml"
        OUTPUT     "${S444_LINUX_VBAML}"
        VAMLCC_EXE "${S444_LINUX_VAMLCC}"
        EMBED_ON   "${S444_LINUX_EMBED_H}" "${S444_LINUX_EMBED_CPP}"
        NAME       "app_shell_MainView"
        DEPS       "${S444_LINUX_VAMLCC}"
    )
    # Feed the sidecar path to the app so `--compiled --app` can open it.
    target_compile_definitions(vaml-viewer-linux PRIVATE
        VAML_APPSHELL_SIDECAR="${S444_LINUX_VBAML}")
    # The app target must build AFTER the compile step produces the sidecar.
    add_dependencies(vaml-viewer-linux vamlcc_app_shell_linux)
endif()
