# src/vulkan — vaml-vulkan library (the ONLY layer that may touch Vulkan/GLFW; ADR-011).
# S003: target instantiated via vaml_add_library() under the artifact model,
# with a MINIMAL REAL version marker (see docs/roadmap.md "Guard Rails").
# RESOLVED at S161 (Sprint 4): the marker is replaced by the real Vulkan surface —
# PlatformWindow (S160, GLFW Win32/X11) + the VMA implementation TU; the real
# version() symbol stays header-only (see version.h). S162+ adds the Vulkan
# instance/device/swapchain surface. version.cpp's empty-TU anchor was removed
# here (the library now has real compiled symbols).
vaml_add_library(vulkan
    src/PlatformWindow.cpp
    src/VmaImpl.cpp
    src/VulkanLoader.cpp
    src/VulkanInstance.cpp
    src/VulkanSurface.cpp
    src/QueueFamilySelection.cpp
    src/BestGpuSelection.cpp
    src/Device.cpp
    src/Swapchain.cpp
    src/SwapchainState.cpp
    src/MsaaTarget.cpp
    src/RenderPass.cpp
    src/ShaderModule.cpp
    src/Pipeline.cpp
    src/Framebuffer.cpp
    src/CommandBuffer.cpp
    src/FrameSync.cpp
    src/quad/QuadVertex.cpp
    src/quad/QuadPipeline.cpp
    src/quad/QuadTexture.cpp
    src/quad/QuadScene.cpp
    src/quad/TextureAtlas.cpp
    # S202_1 — the icon asset pipeline (single quad engine; per ADR-015): PNG +
    # SVG are BOTH source formats, each rasterized ONCE into the S202 atlas —
    # no vector-on-screen, no second renderer. Output tagged with its texel
    # count for S213_1 best-fit.
    src/icon/IconPipeline.cpp
    src/icon/IconAtlas.cpp
    # The single-TU implementation defines (STB_IMAGE_IMPLEMENTATION,
    # NANOSVG_IMPLEMENTATION + NANOSVGRAST_IMPLEMENTATION) live in these files,
    # exactly once per process (the VMA single-TU precedent).
    src/icon/StbImageImpl.cpp
    src/icon/NanosvgImpl.cpp
    # S203 — the embedded bitmap-font loader + ASCII 32–126 glyph atlas build
    # (dep:S202): parses the embedded default TrueType with stb_truetype and
    # rasterizes each glyph's coverage ONCE into the S202 atlas via sub-region
    # upload. S204 (dep:S203) converts those coverage tiles to SDF for soft-edge
    # sampling. The STB_TRUETYPE_IMPLEMENTATION single-TU define lives in
    # TruetypeImpl.cpp, exactly once (the VMA single-TU discipline).
    src/font/FontAtlas.cpp
    src/font/SdfAtlas.cpp
    src/font/TruetypeImpl.cpp
)

# Only vaml-vulkan links the GPU stack (ADR-011): GLFW (S160) + Vulkan + VMA
# (S161), all resolved as interface targets by the top-level find modules.
# S202_1 adds vaml::image (stb_image + nanosvg, the decode/raster headers),
# also GPU-adjacent CPU rasterization reachable only via vaml-vulkan (ADR-011).
target_link_libraries(vaml-vulkan
    PUBLIC vaml::glfw
    PUBLIC vaml::vulkan
    PUBLIC vaml::vma
    PUBLIC vaml::image
)

# The vendored rasterizer headers are exposed via SYSTEM include dirs by
# vaml::image; on MSVC that routing only takes effect when /external:W0 is on,
# so the strict bar (/W4 /WX) governs our own sources and stays quiet on the
# vendor noise compiled inside the implementation TUs (StbImageImpl/NanosvgImpl).
# Clang/GCC treat SYSTEM as -isystem and need no extra flag.
if(MSVC)
    target_compile_options(vaml-vulkan PRIVATE /external:W0)
endif()
