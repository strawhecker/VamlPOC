# src/tools — vaml-tools library (compiler tooling; core layer, no Vulkan; ADR-011).
# S003: target instantiated via vaml_add_library() under the artifact model, with a
# MINIMAL REAL version marker (see docs/roadmap.md "Guard Rails"). The real vamlcc
# tooling lands at Sprint 7 (S287) which RESOLVES this marker.
#
# S280 (dep:S255): the vamlcc compiler skeleton lands HERE (the S287-resolution
# content arrives at S287; the library's MINIMAL REAL marker is kept INTACT and
# resolved at S287 — NO-GAP, no premature tag removal). The compiler is:
#   Vamlc.h/.cpp            — the compile entry points + shared compiled form
#   BindingValidator.h/.cpp — S280_1 static binding validation (ADR-016)
#   vbaml/VbamlSchema.*     — S281 versioned binary record-set schema
#   vbaml/VbamlSerializer.* — S282 byte-identical serializer/deserializer
# see the individual file headers for the per-sprint scope.
vaml_add_library(tools
    src/version.cpp
    src/Vamlc.cpp
    src/BindingValidator.cpp
    src/vbaml/VbamlSchema.cpp
    src/vbaml/VbamlSerializer.cpp
)

# vamlcc links vaml-core (the S2 scanner + factory/value/property tables it
# parses and resolves against). It is CORE-layer (no Vulkan — the ADR-011 guard
# validates this link stays Vulkan-free).
target_link_libraries(vaml-tools PRIVATE vaml-core)

# The vamlcc executable: a thin CLI shell over vaml::tools::run_vamlcc (S280).
# Target name "vamlcc", output vamlcc[.exe].
add_executable(vamlcc vamlcc_main.cpp)
target_link_libraries(vamlcc PRIVATE vaml-tools)
# The executable shares the tools public include (vaml/tools/Vamlc.h).
target_include_directories(vamlcc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
# Hold the executable to the same strict bar as the libraries.
vaml_apply_compiler_policy(vamlcc)

