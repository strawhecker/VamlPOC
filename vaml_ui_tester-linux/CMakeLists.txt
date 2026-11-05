# ============================================================================
# vaml_ui_tester-linux — Linux UI-compliance tester (the Sprint 11r DEDICATED
# tester; S641's scanner lives here). S640 scaffold: mirrors the viewer's
# consumer structure (a SEPARATE CMake project, not pulled in via
# add_subdirectory(src)); it consumes ONLY the already-built static libraries
# through their PUBLIC include roots (CONSUMER-APP PATTERN, ADR-021).
#
# The S640 scaffold takes an authored `.xaml` argument, registers the built-in
# element/converter tables, LOADS it through the real file->tree seam (proving
# the BARE `Source` DP resolution), and exits 0=PASS / 1=FAIL / 2=SKIP. The
# scanner's pull/render/scan/compare power is S641.
#
# Requires (Ubuntu): the clang-debug libs built (vaml-core / vaml-vulkan /
# vaml-presentation / vaml-tools static archives). No GLFW/Vulkan needed for
# the S640 scaffold (load only); S641's offscreen render links the GPU stack
# exactly as vaml_viewer-linux does.
# ============================================================================

cmake_minimum_required(VERSION 3.24)

project(vaml-ui-tester-linux
    VERSION 0.1.0
    DESCRIPTION "vaml_ui_tester - Linux VAML UI-compliance tester"
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
    target_compile_options(vaml-ui-tester-linux PRIVATE /W4 /permissive- /EHsc /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    # The lib's own public headers carry long ASCII-art doc comments ending in a
    # backslash (`\`), which clang's `-Wcomment` flags under -Wall. These are
    # INCLUDE-only (consumer apps cannot edit the lib headers), so the
    # standalone consumer apps silence the benign comment warning — the same
    # reason the test targets pass `-Wno-#warnings`. (vaml_viewer-linux does the
    # same for the headers it consumes.)
    add_compile_options(-Wno-comment)
endif()

# --- Consume the STATIC libraries from the clang-debug build (ADR-021) --------
set(VAML_CORE_INCLUDE          "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/include")
set(VAML_CORE_LIB              "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/core/libvaml-core.a")
set(VAML_VULKAN_INCLUDE        "${CMAKE_CURRENT_SOURCE_DIR}/../src/vulkan/include")
set(VAML_VULKAN_LIB            "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/vulkan/libvaml-vulkan.a")
set(VAML_PRESENTATION_INCLUDE  "${CMAKE_CURRENT_SOURCE_DIR}/../src/presentation/include")
set(VAML_PRESENTATION_LIB      "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/presentation/libvaml-presentation.a")
set(VAML_TOOLS_INCLUDE         "${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/include")
set(VAML_TOOLS_LIB             "${CMAKE_CURRENT_SOURCE_DIR}/../build/clang-debug/src/tools/libvaml-tools.a")

add_executable(vaml-ui-tester-linux
    src/main.cpp
    ../vaml_viewer/load_file.cpp   # S221: the shared headless-ctestable file->tree seam (single source of truth)
)

target_include_directories(vaml-ui-tester-linux PRIVATE
    ${VAML_CORE_INCLUDE}
    ${VAML_VULKAN_INCLUDE}
    ${VAML_PRESENTATION_INCLUDE}
    ${VAML_TOOLS_INCLUDE}
    ${CMAKE_CURRENT_SOURCE_DIR}/../vaml_viewer   # load_file.h (the shared seam header)
)

target_link_libraries(vaml-ui-tester-linux PRIVATE
    ${VAML_PRESENTATION_LIB}
    ${VAML_VULKAN_LIB}
    ${VAML_TOOLS_LIB}
    ${VAML_CORE_LIB}
)
