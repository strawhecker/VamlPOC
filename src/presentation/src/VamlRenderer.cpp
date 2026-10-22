// S212 (dep:S211) — VamlRenderer implementation (vaml::presentation).
//
// The MERGE-POINT vertical-slice render host. Implements the pure decision seams
// + the CPU/merge host (create()) + the LIVE owner (create_device()). The host
// composes the S207/S208 encoder + the S211 postbox + the S210 retained renderer +
// the S173 presenter; see VamlRenderer.h for the split + the judgment + the NO-GAP.
//
// The internally-wired DrawBody (live path) owns the command-buffer record and
// runs the host's fixed render-loop steps against the presenter-owned `cmd`:
//   ConsumeMarshalled -> EncodeIfDirty(+RecordRetained) -> RecordRetained -> Recycle
// (WaitFence + SubmitPresent are the Presenter's own — it waits the in-flight
// fence before invoking the DrawBody and submits/presents after it returns true).

#include <vaml/presentation/VamlRenderer.h>

#include <vaml/vulkan/CommandBuffer.h>      // describe_command_buffer_begin, build_render_pass_begin
#include <vaml/vulkan/ui/MarshalledScene.h> // has_newer / consume_latest

#include <utility>

namespace vaml::presentation {

// ---------------------------------------------------------------------------
// Pure decision seams + the CPU core
// ---------------------------------------------------------------------------

vaml::vulkan::ui::ElementVisual
classify_merge_element(const vaml::UIElement& element) noexcept
{
    // The vertical-slice classifier: every REAL UIElement (today the real layout
    // panels StackPanel/Grid/WrapPanel/Canvas, re-based onto UIElement at
    // S129/S131/S134/S136) contributes a SOLID fill at its ARRANGED final_rect,
    // alpha-tinted by its S208 effective_opacity (the inheritance-resolved value —
    // a child's transparent panel is honest). Degenerate (zero/negative extent)
    // rects are skipped by the encoder's ui_item_degenerate rule, so a not-yet-
    // arranged panel contributes nothing. A DependencyObject-only control seed
    // (Button/Text/etc., not yet re-based) is not a UIElement and gets None (no
    // draw; its subtree still walks). This is the S207 forward seam real control
    // materials replace in place at S400/S364 — never a core virtual (ADR-011).
    vaml::vulkan::ui::ElementVisual v;
    v.kind = vaml::vulkan::ui::MaterialKind::Solid;
    const vaml::Rect r = element.final_rect();
    const float a = static_cast<float>(vaml::vulkan::ui::effective_opacity(element));
    v.item  = vaml::vulkan::ui::solid_rect(r.x, r.y, r.width, r.height,
                                           vaml::vulkan::ui::Color4{ 0.75f, 0.80f, 0.90f, a });
    return v;
}

VisualOf default_visual_of()
{
    return static_cast<VisualOf>(
        [](const vaml::UIElement& e) -> vaml::vulkan::ui::ElementVisual {
            return classify_merge_element(e);
        });
}

void encode_visual_slice(const vaml::UIElement& root, const VisualOf& visualOf,
                         std::vector<vaml::vulkan::ui::DrawItem>& out)
{
    // Delegate to the S207 encoder (the walk + skip + grouping + z-order). The
    // visualOf seam is the material authority; a null/unset seam defaults.
    const VisualOf seam = visualOf ? visualOf : default_visual_of();
    vaml::vulkan::ui::encode_visual_tree(root, seam, out);
}

const std::vector<vaml::vulkan::ui::RenderLoopStep>& render_loop_steps() noexcept
{
    // Reuse the S210/S211 loop-plan (WaitFence -> ConsumeMarshalled -> EncodeIfDirty
    // -> RecordRetained -> SubmitPresent -> Recycle). The host's DrawBody runs the
    // same sequence; SubmitPresent/WaitFence are the Presenter's own steps.
    return vaml::vulkan::ui::render_loop_steps();
}

bool should_reencode(bool dirty) noexcept { return vaml::vulkan::ui::should_reencode(dirty); }
bool should_consume(bool hasNewer) noexcept { return vaml::vulkan::ui::should_consume(hasNewer); }
// presenter_lifetimes_ok is declared + implemented in Presenter.cpp (S173) — reused.

// ---------------------------------------------------------------------------
// The host (RAII; move-only)
// ---------------------------------------------------------------------------

std::unique_ptr<VamlRenderer> VamlRenderer::create()
{
    return std::unique_ptr<VamlRenderer>(new VamlRenderer());
}

std::unique_ptr<VamlRenderer>
VamlRenderer::create_device(const VamlRendererDeviceOptions& opts)
{
    if (!opts.loader || !opts.device || !opts.surface ||
        !presenter_lifetimes_ok(opts.loader->has_swapchain_loader(),
                                opts.loader->has_command_loader(), opts.renderPass) ||
        !opts.device->device())
        return nullptr;

    std::unique_ptr<VamlRenderer> host(new VamlRenderer());
    host->renderPass_ = opts.renderPass;
    host->clear_[0]   = opts.clearR;
    host->clear_[1]   = opts.clearG;
    host->clear_[2]   = opts.clearB;
    host->clear_[3]   = opts.clearA;

    // 1) The S210 retained scene owner (over the S206 scene spec).
    host->sceneRenderer_ =
        vaml::vulkan::ui::SceneRenderer::create(*opts.loader, *opts.device, opts.sceneSpec);
    if (!host->sceneRenderer_)
        return nullptr;

    // 2) The S173 presenter, with the DrawBody INTERNALLY wired to this host
    //    (captures `host`, which outlives the presenter it owns). `presenter.draw`
    //    is ignored/overwritten by the host's own record_draw.
    PresenterOptions optsDraw = opts.presenter;
    optsDraw.draw = [hostPtr = host.get()](void*_userData,
                                           vaml::presentation::Presenter& self,
                                           VkCommandBuffer cmd,
                                           std::uint32_t imageIndex) -> bool {
        (void)_userData;
        return hostPtr->record_draw(self, cmd, imageIndex);
    };
    host->presenter_ = vaml::presentation::Presenter::create(
        *opts.loader, *opts.device, *opts.surface, opts.renderPass, optsDraw);
    if (!host->presenter_)
        return nullptr;

    // In the live path, the UI-thread producer publishes into the SceneRenderer's
    // own postbox (which the DrawBody's consume_changes drains); this host's own
    // postbox_ stays the headless CPU-only transport.
    return host;
}

VamlRenderer::VamlRenderer() = default;
VamlRenderer::VamlRenderer(VamlRenderer&&) noexcept = default;
VamlRenderer& VamlRenderer::operator=(VamlRenderer&&) noexcept = default;
VamlRenderer::~VamlRenderer() = default;

bool VamlRenderer::set_root(vaml::UIElement* root) noexcept
{
    root_ = root;
    // A new root is never arranged yet -> a re-encode is pending.
    dirty_ = true;
    return root != nullptr;
}

void VamlRenderer::set_frame_size(float width, float height) noexcept
{
    frameW_ = width;
    frameH_ = height;
}

void VamlRenderer::set_visual_of(VisualOf visualOf)
{
    visualOf_ = std::move(visualOf);
}

std::uint64_t VamlRenderer::invalidate()
{
    if (!root_ || frameW_ <= 0.0f || frameH_ <= 0.0f)
        return 0;  // nothing to publish yet

    // Measure/Arrange the attached core tree at the frame size (the S123/S124
    // final_rects the encoder reads). The root is measured against an unbounded
    // offer; the arrange slot is the full frame.
    root_->Measure(vaml::Size{ frameW_, vaml::Size::Infinite().height });
    root_->Arrange(vaml::Rect{ 0.0f, 0.0f, frameW_, frameH_ });

    // CPU-encode the arranged tree into the S206 draw list (the S207 encoder via
    // the host's visualOf seam).
    std::vector<vaml::vulkan::ui::DrawItem> items;
    encode_visual_slice(*root_, visualOf_, items);

    // Publish the snapshot to the LIVE SceneRenderer's postbox when present (the
    // DrawBody's consume_changes drains it), else to this host's own postbox_.
    vaml::vulkan::ui::MarshalledScene& box =
        sceneRenderer_ ? sceneRenderer_->postbox() : postbox_;
    box.mutate(items);
    const std::uint64_t version = box.publish();
    dirty_ = true;  // a re-encode is pending on the render side
    return version;
}

std::optional<std::uint32_t> VamlRenderer::tick(bool invalidated, bool tickDue)
{
    if (!presenter_)
        return std::nullopt;
    return presenter_->tick(invalidated, tickDue);
}

void VamlRenderer::request_resize(std::uint32_t width, std::uint32_t height)
{
    if (presenter_)
        presenter_->request_resize(width, height);
}

vaml::vulkan::SwapchainState VamlRenderer::state() const noexcept
{
    return presenter_ ? presenter_->state() : vaml::vulkan::SwapchainState::Suspended;
}

bool VamlRenderer::record_draw(vaml::presentation::Presenter& self,
                               VkCommandBuffer cmd, std::uint32_t imageIndex)
{
    const vaml::vulkan::VulkanLoader& vk = self.loader();
    if (!cmd || !renderPass_ || !sceneRenderer_)
        return false;

    // The acquired image's framebuffer.
    const vaml::vulkan::Framebuffer* fb = self.machine().framebuffer(imageIndex);
    if (!fb)
        return false;

    // Own the command-buffer record: reset + begin, then begin the render pass
    // against the acquired framebuffer (the S166 build_render_pass_begin).
    if (vk.vkResetCommandBuffer(cmd, /*flags=*/0) != VK_SUCCESS)
        return false;
    const auto begin = vaml::vulkan::describe_command_buffer_begin();
    if (vk.vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS)
        return false;

    const VkExtent2D extent = self.extent();
    const float clearColor[4] = { clear_[0], clear_[1], clear_[2], clear_[3] };
    const auto clear = vaml::vulkan::describe_clear_value(clearColor);
    const auto rpBegin = vaml::vulkan::build_render_pass_begin(
        renderPass_, fb->framebuffer(), extent, clear,
        self.machine().depth_format().has_value(),
        /*hasResolve=*/false);
    vk.vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // The fixed loop steps the render side runs (the S210/S211 contract):
    // ConsumeMarshalled -> EncodeIfDirty(+RecordRetained) -> RecordRetained -> Recycle.
    // SceneRenderer::consume_changes drains the postbox ONCE; on a newer version it
    // re-encodes via set_scene (EncodeIfDirty); record() records the RETAINED buffers
    // (no re-encode) and recycle() advances the S209 fence-driven pool.
    const bool consumed = sceneRenderer_->consume_changes();
    (void)consumed;  // an idle frame simply records the retained buffers unchanged

    const bool recorded = sceneRenderer_->record(cmd);
    if (recorded)
        sceneRenderer_->recycle(VK_NULL_HANDLE);

    vk.vkCmdEndRenderPass(cmd);
    if (vk.vkEndCommandBuffer(cmd) != VK_SUCCESS)
        return false;
    return recorded;
}

} // namespace vaml::presentation
