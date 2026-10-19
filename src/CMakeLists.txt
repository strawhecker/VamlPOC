# src/ — the reusable VAML framework + tools (the IP). Each subdir is a lib/tool.
# S001: resolve the hierarchy. S003: all three targets instantiated under the
# artifact model (vaml_add_library) — vaml-vulkan now carries the real Vulkan
# surface (S161 resolved its MINIMAL REAL version marker); vaml-tools still
# carries its version marker, RESOLVED BY S287. See docs/roadmap.md.
add_subdirectory(core)
# S003: vulkan is now a real surface (S160 PlatformWindow + S161 Vulkan/VMA
# linkage + marker resolution); tools is a live target (MINIMAL REAL marker),
# its real content arrives at S287.
add_subdirectory(vulkan)
add_subdirectory(tools)
