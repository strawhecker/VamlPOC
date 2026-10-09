# src/ — the reusable VAML framework + tools (the IP). Each subdir is a lib/tool.
# S001: resolve the hierarchy. Artifact model (shared/static, ABI facade) = S003.
add_subdirectory(core)
# add_subdirectory(vulkan)  # S16x (Sprint 4) — Vulkan backend. Not in S001 (core has no Vulkan deps).
# add_subdirectory(tools)   # vamlcc — S28x (Sprint 7). Not in S001.
