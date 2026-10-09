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

# Convenience: every target must call vaml_apply_compiler_policy.
# Enforced by convention; S002 keeps the policy centralized in the module.

# ============================================================================
# Directory aggregation — library/tool sources live under src/ (see ROADMAP).
# The actual static/shared artifact model is S003; here we just add subdirs so
# the hierarchy resolves in the IDE and the build is green from the first step.
# ============================================================================
add_subdirectory(src)

# The POC application is a separate tree (VamlPOC/) with its own VS project;
# wiring it into CMake arrives with S003/S004. At S001 we intentionally do NOT
# add_subdirectory(VamlPOC) — keep S001 = language + policy + library baseline.
