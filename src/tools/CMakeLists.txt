# src/tools — vaml-tools library (compiler tooling; core layer, no Vulkan; ADR-011).
# S003: target instantiated via vaml_add_library() under the artifact model, with a
# MINIMAL REAL version marker (see docs/roadmap.md "Guard Rails"). The real vamlcc
# tooling lands at Sprint 7 (S287) which RESOLVES this marker.
vaml_add_library(tools
    src/version.cpp
)
