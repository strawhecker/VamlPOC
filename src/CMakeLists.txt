# src/ — the reusable VAML framework + tools (the IP). Each subdir is a lib/tool.
# S001: resolve the hierarchy. S003: all three targets instantiated under the
# artifact model (vaml_add_library) — vulkan/tools carry MINIMAL REAL version
# markers, RESOLVED BY S161 (vulkan) and S287 (tools). See docs/roadmap.md.
add_subdirectory(core)
# S003: vulkan + tools are now live targets (MINIMAL REAL markers), no longer
# deferred. Their real content arrives at S161 / S287 respectively.
add_subdirectory(vulkan)
add_subdirectory(tools)
