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
    # S213_1 — the DPI-aware icon best-fit SELECTION (per ADR-015): the scaling
    # policy { none | down-only | both }, default none, computed ONCE per
    # (IconKey, logical size) against framebufferScale then cached. Consumes the
    # S202_1 size-family candidates + the S213 dpiScale input.
    src/icon/IconBestFit.cpp
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
    # S205 — the UI pipeline family (SDF text + solid color + border) on ONE
    # SHARED descriptor-set layout + pipeline layout (the four real UI shaders
    # are embedded in ShaderModule. The shared 128-byte UiPushConstants + the
    # shared set-0-binding-0 sampler generalize the S200-local descriptor seam.
    src/ui/UiPipeline.cpp
    # S209 (dep:S208) — the fence-aware retired-buffer pool: owns the vertex/index
    # buffer GENERATIONS so a UiScene grow RETIRES the old buffer instead of freeing
    # it, and recycle() frees it once the S166 in-flight ring's fence signals
    # (the "retired-buffer lifetime via fence" guarantee — no UAF).
    src/ui/RetainedBufferPool.cpp
    # S206 — the UI primitive draw batcher (dep:S205): rects + text glyphs from
    # the SHARED descriptor layout, batching contiguous same-shader draws into
    # ONE vkCmdDrawIndexed each (the S205 shared-layout premise — kind switch =
    # pipeline rebind + push update, never a descriptor-set rebind). UiScene is
    # the named consumer of S205's shared layout. See UiScene.h for the NO-GAP.
    src/ui/UiScene.cpp
    # S210 (dep:S209) — the single-thread retained-scene owner: encodes a logical
    # scene ONCE into the S209 fence-aware retained buffers and RECORDS them every
    # frame (no re-encode) + recycles retired generations via the in-flight fence —
    # the render-thread loop end-to-end. Marshalling to a dual-thread model is S211.
    src/ui/SceneRenderer.cpp
    # S211 (dep:S210) — the lock-free scene-change postbox (MarshalledScene): the
    # UI thread mutates + publishes a CPU-encoded DrawItem snapshot ONCE per frame;
    # the render thread consumes the LATEST once per frame (has_newer/consume_latest,
    # release/acquire atomics, ZERO new locks per ADR-018). SceneRenderer::consume_changes
    # drains it ahead of EncodeIfDirty in the S211 loop-plan. Consumed by S212.
    src/ui/MarshalledScene.cpp
    # S207 (dep:S206) — the Scene→command-buffer encoder: walks the REAL core
    # visual tree (vaml::core::UIElement) and produces the S206 DrawItem list —
    # draw list by material, z-order via tree order (the CORE→vulkan feed edge).
    src/ui/UiSceneEncoder.cpp
    # S400 (dep:S207) — the RENDER-side Text seam: maps the CORE text_layout_plan
    # (already word-wrapped) into S206 DrawItem glyph quads with the REAL S202
    # exact-grid atlas UVs. Pure + headless-testable (no Vulkan runtime). Consumed
    # by the S400 Text material + S409_1 viewer growth.
    src/ui/TextGlyphPlan.cpp
    # S400_1 (dep:S400) — the RENDER-side Icon seam: turns the Icon control's
    # DP-driven inputs into the S206 draw item via the S213_1 best-fit + the
    # cached icon provider, with the deliberate fallback glyph + structured
    # diagnostic for an unresolved asset (ADR-015 NO-GAP). Pure + headless.
    src/icon/IconGlyphPlan.cpp
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
    # S207: the UiSceneEncoder walks the REAL core visual tree (vaml::core::UIElement),
    # so vaml-vulkan gains a PUBLIC vaml-core edge. This is a legitimate
    # Vulkan→core dependency under ADR-011 (core still never reaches Vulkan/GLFW —
    # vaml_validate_layer_rules enforces that direction). PUBLIC mirrors the
    # glfw/vulkan/vma/image edges: consumers + the test target see core too.
    PUBLIC vaml-core
)

# The vendored rasterizer headers are exposed via SYSTEM include dirs by
# vaml::image; on MSVC that routing only takes effect when /external:W0 is on,
# so the strict bar (/W4 /WX) governs our own sources and stays quiet on the
# vendor noise compiled inside the implementation TUs (StbImageImpl/NanosvgImpl).
# Clang/GCC treat SYSTEM as -isystem and need no extra flag.
if(MSVC)
    target_compile_options(vaml-vulkan PRIVATE /external:W0)
endif()
