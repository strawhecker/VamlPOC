# src/core — vaml-core library (no Vulkan deps; ADR-011 layer rule).
# S001 scope: a buildable library baseline with the version API + a smoke symbol,
# under the shared compiler/warning policy. Property system begins at S030 (Sprint 1).
add_library(vaml-core STATIC)

target_include_directories(vaml-core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_sources(vaml-core PRIVATE
    src/version.cpp
)

# Compiler/warning policy from the top-level function.
vaml_apply_compiler_policy(vaml-core)

# Core must never pull in Vulkan/GLFW (ADR-011). Keep the include path tight.
