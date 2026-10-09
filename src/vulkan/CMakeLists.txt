# src/vulkan — vaml-vulkan library (the ONLY layer that may touch Vulkan/GLFW; ADR-011).
# S003: target instantiated via vaml_add_library() under the artifact model, with a
# MINIMAL REAL version marker (see docs/roadmap.md "Guard Rails"). The real Vulkan
# surface lands at Sprint 4 (S161) which RESOLVES this marker.
vaml_add_library(vulkan
    src/version.cpp
)
