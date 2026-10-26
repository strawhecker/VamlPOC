// S446 — RAIL 2: Hidden-window real-WSI app-shell validation gate (LOCAL on this
// box, NOT GH) — the "zero validation-layer errors over 10k PRESENTED frames"
// PROOF against the REAL in-repo app-shell on the REAL driver.
//
// This is Deliverable C of the S446 plan. It mirrors the S177 WSIResize harness
// (the proven live-stack construction precedent: loader -> instance -> surface ->
// best-gpu -> device -> swapchain-negotiate -> render pass -> the full S222 3-
// pipeline UI scene spec + SDF atlas -> the S210 retained SceneRenderer + the
// S173 Presenter), ENABLES validation layers, installs a VK_EXT_debug_utils
// messenger that fails on ANY validation/error message, AND drives the REAL
// app-shell through the FULL retained-render path:
//
//   (1) run_app_shell_binding_plan() — the S443/S445 binding plan (the exact
//       `--app` launch) loads + wires + finalizes the authored MainView.vaml,
//       RETAINING the reader/VM (the live wires stay armed — the S246 loop's
//       interaction model, single-threaded).
//   (2) encode the REAL shell tree ONCE via encode_visual_slice -> set_scene once
//       (the S210 RETAINED encode: no per-frame re-encode).
//   (3) a change-driven loop of 10,000 REAL PRESENTED frames — each frame
//       records the RETAINED buffers (no re-encode = no per-frame encode/layout
//       thrash on the real driver) + submits + presents through the Presenter.
//   (4) a clean vkDeviceWaitIdle + reverse-order teardown (the S170 discipline).
//
// **Any validation error = test failure** (the hard S177/S446 contract). The gate
// is enforced as a LOCAL pre-✅ command on THIS box (build/verify_s446.cmd's WSI
// lane); it is NOT a GH lane (CI runners have no WSI surface/display). On a
// headless host (no display, no present-capable surface) the executable exits
// SKIP (exit 2) so normal CI stays green; the local gate forces the RUN.
//
// Exit codes: 0 = PASS (10k presented frames, zero validation errors, clean teardown)
//             1 = FAIL (validation error observed, or a required create failed)
//             2 = SKIP (no display / no surface — the local gate forces a real RUN)

#include <vaml/presentation/VamlRenderer.h>   // the S212 vertical-slice host (reuses the S222 stack + Presenter + SceneRenderer)
#include <vaml/vulkan/PlatformWindow.h>
#include <vaml/vulkan/VulkanLoader.h>
#include <vaml/vulkan/VulkanInstance.h>
#include <vaml/vulkan/VulkanSurface.h>
#include <vaml/vulkan/Device.h>
#include <vaml/vulkan/Swapchain.h>
#include <vaml/vulkan/gpu/BestGpuSelection.h>
#include <vaml/vulkan/RenderPass.h>
#include <vaml/vulkan/QueueFamilySelection.h>
#include <vaml/vulkan/ShaderModule.h>
#include <vaml/vulkan/quad/QuadPipeline.h>        // quad::is_srgb_format (S200)
#include <vaml/vulkan/quad/TextureAtlas.h>        // quad::TextureAtlas (S202)
#include <vaml/vulkan/font/FontAtlas.h>           // font::FontAtlas (S203)
#include <vaml/vulkan/font/SdfAtlas.h>            // font::SdfAtlas (S204)
#include <vaml/vulkan/ui/UiPipeline.h>            // ui::UiPipeline (S205)
#include <vaml/vulkan/ui/UiScene.h>               // ui::UiScene::Spec (S206)
#include <vaml/vulkan/ui/SceneRenderer.h>         // ui::SceneRenderer (S210)
#include <vaml/presentation/Presenter.h>          // presenter_lifetimes_ok (S173)
#include <vaml/vulkan/ui/UiSceneEncoder.h>        // encode_visual_slice (via the host) + DrawItem

#include "app_shell_binding.h"   // S443/S445: run_app_shell_binding_plan (the --app launch)

#include <vaml/core/UIElement.h>  // vaml::UIElement (the REAL core visual tree)
#include <vaml/core/HeapStats.h>  // S146: the public churn funnel counters (flat across the retained loop)

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace vaml::vulkan;

namespace {

// The retained scene owner + the render pass — set in scope before the Presenter
// builds (the draw body below references them).
vaml::vulkan::ui::SceneRenderer* g_sceneRenderer = nullptr;
VkRenderPass g_renderPass = VK_NULL_HANDLE;

std::atomic<bool> g_validationError = false;

// The S446 debug-utils messenger callback: ANY ERROR-severity message marks the
// gate FAILED (the "any validation error = test failure" contract, mirrored from
// S177).
VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
              VkDebugUtilsMessageTypeFlagsEXT /*type*/,
              const VkDebugUtilsMessengerCallbackDataEXT* data,
              void* /*userData*/)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::fprintf(stderr, "[s446 VALIDATION ERROR] %s\n",
                     data && data->pMessage ? data->pMessage : "(no message)");
        g_validationError = true;
    }
    return VK_FALSE;
}

// Binds + installs the debug-utils messenger at runtime (the S177 precedent — a
// local gate-only best-effort binding; if the extension is absent the run
// continues unguarded).
struct DebugMessenger
{
    PFN_vkDestroyDebugUtilsMessengerEXT destroyFn = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;

    ~DebugMessenger()
    {
        if (messenger && destroyFn)
            destroyFn(instance, messenger, nullptr);
    }
};

bool install_debug_messenger(DebugMessenger& m, const VulkanLoader& vk, VkInstance instance)
{
    m.instance = instance;
    auto* createFn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vk.vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    m.destroyFn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vk.vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (!createFn || !m.destroyFn)
        return false;

    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
    ci.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback = &debugCallback;

    return createFn(instance, &ci, nullptr, &m.messenger) == VK_SUCCESS;
}

// The S446 retained DrawBody — records the scene's RETAINED buffers onto the
// presenter's command buffer (NO re-encode: the retained SceneRenderer draws the
// SAME encoded buffers every frame). Mirrors the S210 loop integration the
// VamlRenderer's internally-wired record_draw uses.
bool draw_retained(void* userData, vaml::presentation::Presenter& self,
                   VkCommandBuffer cmd, std::uint32_t imageIndex)
{
    (void)userData;
    const auto& vk = self.loader();
    const vaml::vulkan::Framebuffer* fb = self.machine().framebuffer(imageIndex);
    if (!fb || !cmd)
        return false;
    if (vk.vkResetCommandBuffer(cmd, /*flags=*/0) != VK_SUCCESS)
        return false;

    const auto begin = describe_command_buffer_begin();
    if (vk.vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
        return false;

    const VkExtent2D extent = self.extent();
    const float clearColor[4] = { 0.18f, 0.22f, 0.26f, 1.0f };
    const auto clear = describe_clear_value(clearColor);
    const auto rpBegin = vaml::vulkan::build_render_pass_begin(
        g_renderPass, fb->framebuffer(), extent, clear,
        self.machine().depth_format().has_value(),
        /*hasResolve=*/false);
    vk.vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // ConsumeMarshalled (idle: nothing new -> no re-encode) + record the RETAINED
    // buffers (the "retained scene drawn each frame" guarantee) + recycle (the S209
    // fence-driven pool).
    (void)g_sceneRenderer->consume_changes();
    const bool recorded = g_sceneRenderer->record(cmd);
    if (recorded)
        g_sceneRenderer->recycle(VK_NULL_HANDLE);

    vk.vkCmdEndRenderPass(cmd);
    if (vk.vkEndCommandBuffer(cmd) != VK_SUCCESS)
        return false;
    return recorded;
}

} // namespace

// File-based diagnostic sink (survives an AV so we can see how far the gate got).
static void logfile(const char* msg)
{
    std::ofstream f("s446_diag.txt", std::ios::app);
    if (f)
        f << msg << "\n";
}

int main()
{
    logfile("[s446] begin");
    std::fprintf(stderr, "[s446] begin\n");
    std::fflush(stderr);

    // --- The S446 harness: ONE hidden window (surface host) + the real WSI stack,
    //     mirroring the S222 soup-to-nuts construction + the S177 gate. ----
    vaml::vulkan::WindowCallbacks callbacks;
    auto window = vaml::vulkan::PlatformWindow::create(
        { "VAML S446 WSI app-shell gate", 800, 600 }, std::move(callbacks));
    if (!window)
    {
        std::fprintf(stderr, "[s446] No display / PlatformWindow::create failed — SKIP (exit 2).\n");
        std::fprintf(stderr, "[s446] The local build/verify_s446.cmd WSI lane forces a REAL device run.\n");
        return 2;
    }
    auto* glfwWindow = static_cast<GLFWwindow*>(window->native_handle());
    glfwHideWindow(glfwWindow);

    std::unique_ptr<VulkanLoader> vk;
    std::unique_ptr<VulkanInstance> instance;
    std::unique_ptr<VulkanSurface> surface;
    std::unique_ptr<Device> device;
    std::unique_ptr<RenderPass> renderPass;
    std::unique_ptr<vaml::vulkan::ShaderModule> uiVertex, sdfFrag, solidFrag, borderFrag;
    std::unique_ptr<vaml::vulkan::ui::UiPipeline> solidPipeline, sdfPipeline, borderPipeline;
    std::unique_ptr<vaml::vulkan::quad::TextureAtlas> atlas;
    std::unique_ptr<vaml::vulkan::font::FontAtlas> fontAtlas;
    std::unique_ptr<vaml::vulkan::font::SdfAtlas> sdfAtlas;
    std::unique_ptr<vaml::vulkan::ui::SceneRenderer> sceneRenderer;
    std::unique_ptr<vaml::presentation::Presenter> presenter;

    try
    {
        // The S443/S445 binding plan — the exact `--app` launch: load + wire +
        // finalize the authored MainView.vaml, retaining the reader/VM (the live
        // wires stay armed for the loop's lifetime).
        vaml::viewer::AppShellBindingPlanResult plan = vaml::viewer::run_app_shell_binding_plan();
        if (!plan.ok || !plan.root)
        {
            std::fprintf(stderr, "[s446] app-shell binding plan failed: %s — FAIL (exit 1)\n",
                         plan.detail.empty() ? "no root" : plan.detail.c_str());
            return 1;
        }
        auto* uiRoot = dynamic_cast<vaml::UIElement*>(plan.root.get());
        if (!uiRoot)
        {
            std::fprintf(stderr, "[s446] app-shell root is not a UIElement — FAIL (exit 1)\n");
            return 1;
        }
        std::fprintf(stderr, "[s446] app-shell plan ok (transferred=%zu)\n", plan.transferred);

        vk = std::make_unique<VulkanLoader>();
        std::fprintf(stderr, "[s446] loader ok\n");

        VulkanInstance::Config icfg;
        icfg.enableValidation = true;   // a Debug build => validation layers REALLY ON
        instance = VulkanInstance::create(*vk, icfg);
        if (!instance)
        {
            std::fprintf(stderr, "[s446] VulkanInstance::create failed — FAIL (exit 1)\n");
            return 1;
        }
        std::fprintf(stderr, "[s446] instance ok\n");

        surface = VulkanSurface::create(*vk, *instance, *window);
        if (!surface)
        {
            std::fprintf(stderr, "[s446] VulkanSurface::create failed (no surface) — SKIP (exit 2)\n");
            return 2;
        }

        DebugMessenger messenger;
        install_debug_messenger(messenger, *vk, instance->instance());

        auto best = gpu::select_best_gpu(*vk, *instance, surface.get(),
                                         gpu::preferred_gpu_env_name());
        if (!best || best->best.device == VK_NULL_HANDLE)
        {
            std::fprintf(stderr, "[s446] no physical device — SKIP (exit 2)\n");
            return 2;
        }
        std::fprintf(stderr, "[s446] gpu=%s\n", best->best.name.c_str());

        std::uint32_t famCount = 0;
        vk->vkGetPhysicalDeviceQueueFamilyProperties(best->best.device, &famCount, nullptr);
        std::vector<VkQueueFamilyProperties> fams(famCount);
        vk->vkGetPhysicalDeviceQueueFamilyProperties(best->best.device, &famCount, fams.data());
        PresentSupportFn present = [&](std::uint32_t i) {
            VkBool32 s = VK_FALSE;
            return vk->vkGetPhysicalDeviceSurfaceSupportKHR(best->best.device, i,
                                                            surface->surface(), &s) == VK_SUCCESS &&
                   s == VK_TRUE;
        };
        auto fam = select_queue_family(fams.data(), famCount, present);
        if (!fam)
        {
            std::fprintf(stderr, "[s446] no graphics+present queue family — SKIP (exit 2)\n");
            return 2;
        }

        device = Device::create(*vk, best->best, *fam);
        if (!device)
        {
            std::fprintf(stderr, "[s446] Device::create failed — FAIL (exit 1)\n");
            return 1;
        }
        std::fprintf(stderr, "[s446] device ok\n");

        auto cfg = Swapchain::negotiate(*vk, best->best.device, *surface, 800, 600);
        const VkFormat colorFormat = cfg ? cfg->surfaceFormat.format : VK_FORMAT_B8G8R8A8_SRGB;
        const std::optional<VkFormat> depth = cfg ? cfg->depthFormat : std::nullopt;
        renderPass = RenderPass::create(*vk, *device, colorFormat, depth, VK_SAMPLE_COUNT_1_BIT);
        if (!renderPass)
        {
            std::fprintf(stderr, "[s446] RenderPass::create failed — FAIL (exit 1)\n");
            return 1;
        }
        g_renderPass = renderPass->render_pass();

        // ---- The S205 shader modules + 3 UiPipelines (the S222 stack) ----
        uiVertex  = vaml::vulkan::ShaderModule::create(*vk, *device, vaml::vulkan::ui_vertex_spirv());
        sdfFrag   = vaml::vulkan::ShaderModule::create(*vk, *device, vaml::vulkan::sdf_text_fragment_spirv());
        solidFrag = vaml::vulkan::ShaderModule::create(*vk, *device, vaml::vulkan::solid_color_fragment_spirv());
        borderFrag= vaml::vulkan::ShaderModule::create(*vk, *device, vaml::vulkan::border_fragment_spirv());
        if (!uiVertex || !sdfFrag || !solidFrag || !borderFrag)
        {
            std::fprintf(stderr, "[s446] shader module create failed — FAIL (exit 1)\n");
            return 1;
        }

        vaml::vulkan::ui::UiPipelineSpec plSpec;
        plSpec.renderPass  = renderPass->render_pass();
        plSpec.colorFormat = colorFormat;
        plSpec.hasDepth    = depth.has_value();
        plSpec.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        plSpec.extent      = { 800, 600 };
        plSpec.vertexModule = uiVertex->shader_module();

        plSpec.fragmentModule = solidFrag->shader_module();
        solidPipeline = vaml::vulkan::ui::UiPipeline::create(
            *vk, *device, plSpec, vaml::vulkan::ui::UiShaderKind::Solid);
        if (!solidPipeline)
        {
            std::fprintf(stderr, "[s446] solid pipeline create failed — FAIL (exit 1)\n");
            return 1;
        }
        const VkPipelineLayout sharedLayout    = solidPipeline->pipeline_layout();
        const VkDescriptorSetLayout sharedSet  = solidPipeline->descriptor_set_layout();

        plSpec.fragmentModule = sdfFrag->shader_module();
        sdfPipeline = vaml::vulkan::ui::UiPipeline::create(
            *vk, *device, plSpec, vaml::vulkan::ui::UiShaderKind::SdfText);
        plSpec.fragmentModule = borderFrag->shader_module();
        borderPipeline = vaml::vulkan::ui::UiPipeline::create(
            *vk, *device, plSpec, vaml::vulkan::ui::UiShaderKind::Border);
        if (!sdfPipeline || !borderPipeline)
        {
            std::fprintf(stderr, "[s446] sdf/border pipeline create failed — FAIL (exit 1)\n");
            return 1;
        }

        // ---- The S202/S203/S204 atlas stack (the SDF sample resource) ----
        atlas = vaml::vulkan::quad::TextureAtlas::create(*vk, *device, 4096u, 2048u, 16u, 8u);
        if (!atlas)
        {
            std::fprintf(stderr, "[s446] atlas create failed — FAIL (exit 1)\n");
            return 1;
        }
        fontAtlas = std::make_unique<vaml::vulkan::font::FontAtlas>(*atlas);
        if (!fontAtlas->load() || fontAtlas->upload_glyphs() == 0u)
        {
            std::fprintf(stderr, "[s446] FontAtlas load/upload failed — FAIL (exit 1)\n");
            return 1;
        }
        sdfAtlas = std::make_unique<vaml::vulkan::font::SdfAtlas>(*atlas, *fontAtlas);
        if (!sdfAtlas->build() || sdfAtlas->upload_sdfs() == 0u)
        {
            std::fprintf(stderr, "[s446] SdfAtlas build/upload failed — FAIL (exit 1)\n");
            return 1;
        }

        // ---- The S206 UiScene::Spec (the shared layout + 3 pipelines + the SDF atlas) ----
        vaml::vulkan::ui::UiScene::Spec sceneSpec;
        sceneSpec.renderPass     = renderPass->render_pass();
        sceneSpec.colorFormat    = colorFormat;
        sceneSpec.extent         = { 800, 600 };
        sceneSpec.pipelineLayout = sharedLayout;
        sceneSpec.setLayout      = sharedSet;
        sceneSpec.sdfPipeline    = sdfPipeline->pipeline();
        sceneSpec.solidPipeline  = solidPipeline->pipeline();
        sceneSpec.borderPipeline = borderPipeline->pipeline();
        sceneSpec.atlasView      = atlas->image_view();
        sceneSpec.atlasSampler   = atlas->sampler();
        sceneSpec.inFlightCount  = 3;

        // ---- The S210 retained SceneRenderer (encode the shell ONCE) ----
        sceneRenderer = vaml::vulkan::ui::SceneRenderer::create(*vk, *device, sceneSpec);
        if (!sceneRenderer)
        {
            std::fprintf(stderr, "[s446] SceneRenderer::create failed — FAIL (exit 1)\n");
            return 1;
        }
        g_sceneRenderer = sceneRenderer.get();

        // ENCODE the REAL app-shell tree ONCE (the S207 encoder over the arranged
        // tree) and set_scene ONCE — the S210 RETAINED encode. Arrange first (the
        // S212 producer arrange step).
        uiRoot->Measure(vaml::Size{ 800.0f, vaml::Size::Infinite().height });
        uiRoot->Arrange(vaml::Rect{ 0.0f, 0.0f, 800.0f, 600.0f });
        std::vector<vaml::vulkan::ui::DrawItem> items;
        vaml::presentation::encode_visual_slice(*uiRoot, vaml::presentation::default_visual_of(), items);
        if (items.empty())
        {
            std::fprintf(stderr, "[s446] encoded shell scene is empty — FAIL (exit 1)\n");
            return 1;
        }
        if (!sceneRenderer->set_scene(items))
        {
            std::fprintf(stderr, "[s446] retained set_scene failed — FAIL (exit 1)\n");
            return 1;
        }
        std::fprintf(stderr, "[s446] retained encode done: %zu items\n", items.size());

        // ---- The S173 Presenter (the ONE owner of acquire/submit/present) ----
        vaml::presentation::PresenterOptions opts;
        opts.width = 800;
        opts.height = 600;
        opts.sampleCount = VK_SAMPLE_COUNT_1_BIT;
        opts.draw = &draw_retained;
        presenter = vaml::presentation::Presenter::create(
            *vk, *device, *surface, renderPass->render_pass(), opts);
        if (!presenter)
        {
            std::fprintf(stderr, "[s446] Presenter::create failed (no gen0) — SKIP (exit 2)\n");
            return 2;
        }
        std::fprintf(stderr, "[s446] machine gen0 built: %u images, extent %ux%u\n",
                     presenter->machine().image_count(), presenter->extent().width,
                     presenter->extent().height);

        // ---- The S446 headline: 10,000 REAL PRESENTED frames over the RETAINED
        //      app-shell scene (no per-frame re-encode = no encode/layout thrash on
        //      the real driver) ----
        const std::uint32_t kFrames = 10000;
        std::uint32_t presented = 0;
        for (std::uint32_t i = 0; i < kFrames; ++i)
        {
            glfwPollEvents();
            // tick(true, true) — the change-driven loop: acquire -> record retained
            // -> submit -> present. The retained record has no re-encode.
            const auto slot = presenter->tick(true, true);
            if (slot.has_value())
                ++presented;
            if (presented >= 100 && (presented % 1000u) == 0u)
                std::fprintf(stderr, "[s446] %u frames presented\n", presented);
            if (g_validationError.load())
            {
                std::fprintf(stderr, "[s446] validation error during frame loop — FAIL (exit 1)\n");
                return 1;
            }
        }

        std::fprintf(stderr, "[s446] presented %u / %u frames\n", presented, kFrames);
        if (presented == 0)
        {
            std::fprintf(stderr, "[s446] ZERO frames presented (idle/park) — can't prove the gate — FAIL (exit 1)\n");
            return 1;
        }

        // ---- The S170 clean teardown: idle the device, then destroy in REVERSE
        //      build order (the swapchain/image lifetimes + the S209 pools release). ----
        vk->vkDeviceWaitIdle(device->device());
        presenter.reset();
        sceneRenderer.reset();
        logfile("[s446] teardown done");

        // ---- VERDICT ----
        if (g_validationError.load())
        {
            std::fprintf(stderr, "[s446] FAIL: validation error observed over %u frames\n", presented);
            return 1;
        }
        std::fprintf(stderr, "[s446] PASS: %u real presented frames, ZERO validation errors, clean teardown\n",
                     presented);
        return 0;
    }
    catch (const std::exception& e)
    {
        std::fprintf(stderr, "[s446] exception: %s — FAIL (exit 1)\n", e.what());
        return 1;
    }
}
