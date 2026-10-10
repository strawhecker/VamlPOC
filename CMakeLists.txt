cmake_minimum_required(VERSION 3.24)

# ============================================================================
# VAML — top-level build
# S001: project(), C++ language level. The warning/error policy shared by every
# target now lives in cmake/compiler-policy.cmake (S002). Target/artifact
# structure lands in S003 (VAML_BUILD_SHARED etc.).
# Governing docs: docs/ROADMAP.md (S001), docs/adr/ADR-LOG.md (ADR-004, ADR-007).
# ============================================================================

project(VAML
    VERSION 0.1.0
    DESCRIPTION "VAML - XAML-faithful, Vulkan-native, pure C++ UI framework"
    LANGUAGES CXX
)

# --- Language level: C++ latest (C++20 baseline; allows /std:c++latest on MSVC) ---
if(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    # MSVC 18.0+/VS 2026 supports /std:c++latest; opt in via preset if available.
else()
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
endif()

# --- Build type default ---
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# ============================================================================
# Compiler / warning policy  (ADR-004: no exceptions in release hot paths is a
# per-target concern; the POLICY here is: warnings-as-errors + strict flags).
# S002: policy moved to cmake/compiler-policy.cmake so every target (and future
# sub-builds) applies identical strict flags from a single module home.
# ============================================================================
include(cmake/compiler-policy.cmake)
# S003: artifact model (VAML_BUILD_SHARED, vaml_add_library, export headers,
# ADR-011 layer validation). All VAML libraries are created through this module.
include(cmake/artifact-model.cmake)

# ============================================================================
# Directory aggregation — library/tool sources live under src/ (see ROADMAP).
# vaml_add_library() (S003) is the only way targets are produced; the layer
# rules (ADR-011) are validated here AFTER every subdirectory is configured.
# ============================================================================
add_subdirectory(src)
vaml_validate_layer_rules()

# ============================================================================
# Tests (S005)
# enable_testing() makes `ctest --preset <x>` meaningful (test discovery +
# run under CTest). The test subtree contributes no symbols to any VAML library.
# ============================================================================
enable_testing()
add_subdirectory(test)

# The POC application is a separate tree (VamlPOC/) with its own VS project;
# wiring it into CMake arrives with S003/S004. At S001 we intentionally do NOT
# add_subdirectory(VamlPOC) — keep S001 = language + policy + library baseline.
