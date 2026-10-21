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
)

# Only vaml-vulkan links the GPU stack (ADR-011): GLFW (S160) + Vulkan + VMA
# (S161), all resolved as interface targets by the top-level find modules.
target_link_libraries(vaml-vulkan
    PUBLIC vaml::glfw
    PUBLIC vaml::vulkan
    PUBLIC vaml::vma
)
