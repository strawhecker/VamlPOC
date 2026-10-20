// S173 — Presenter implementation.
//
// Implements the S173 pure decision seams (headless-testable) + the real
// change-driven presenter owner. See Presenter.h for the full split + the
// LIFETIMES contract + the optional-framework rationale.

#include <vaml/presentation/Presenter.h>

#include <utility>

namespace vaml::presentation {

IterationAction iteration_action(vaml::vulkan::SwapchainState state) noexcept
{
    switch (state)
    {
        case vaml::vulkan::SwapchainState::Ready:
        case vaml::vulkan::SwapchainState::Retryable:
            return IterationAction::Draw;   // Gen N valid + presentable in both
        case vaml::vulkan::SwapchainState::Suspended:
            return IterationAction::Park;   // zero/minimized extent — nothing to present
        case vaml::vulkan::SwapchainState::Fatal:
            return IterationAction::Stop;   // engine stops + fires OnPresentationLost
    }
    return IterationAction::Stop;
}

bool should_render(bool invalidated, bool tickDue, vaml::vulkan::SwapchainState state) noexcept
{
    // The change-driven gate: re-render only on an invalidation or a tick, AND only
    // when the machine is drawable. Idle (no invalidated, no tickDue) -> no presents
    // regardless of state; a park (Suspended) or stop (Fatal) never renders.
    if (!invalidated && !tickDue)
        return false;
    return iteration_action(state) == IterationAction::Draw;
}

bool presenter_lifetimes_ok(bool hasSwapchainLoader,
                            bool hasCommandLoader,
                            VkRenderPass renderPass) noexcept
{
    // Gates the obviously-invalid no-handle case: the presenter cannot own the loop
    // if the swapchain + command loader entry points are absent or the render pass is
    // null (the per-image framebuffer + acquire/submit/present all need them). The
    // "outlive" ORDER is a caller contract (documented in the header). Pure +
    // headless-testable.
    return hasSwapchainLoader && hasCommandLoader && renderPass != VK_NULL_HANDLE;
}

std::unique_ptr<Presenter>
Presenter::create(const vaml::vulkan::VulkanLoader& loader,
                  const vaml::vulkan::Device& device,
                  const vaml::vulkan::VulkanSurface& surface,
                  VkRenderPass renderPass,
                  const PresenterOptions& options)
{
    if (!presenter_lifetimes_ok(loader.has_swapchain_loader(), loader.has_command_loader(),
                                renderPass) ||
        !device.device())
        return nullptr;
    if (!options.draw)
        return nullptr;

    std::unique_ptr<Presenter> p(new Presenter(loader, device));
    p->renderPass_ = renderPass;
    p->options_    = options;

    // The S167_1 real builders (swapchain + image views + depth + per-image
    // framebuffers + optional S168 MSAA target) behind one presenter. If the INITIAL
    // generation cannot be built there is no Gen N to fall back to -> startup failure.
    p->machine_ = vaml::vulkan::SwapchainStateMachine::create(
        vaml::vulkan::SwapchainStateBuilders::real(
            loader, device, surface, renderPass, options.sampleCount),
        options.width, options.height, options.sampleCount);

    // The S166 frame-slot resources (independent of image count — they survive every
    // swapchain recreation unchanged; the machine does NOT touch them).
    if (p->machine_ && loader.has_command_loader())
    {
        p->commandPool_ = vaml::vulkan::CommandPool::create(
            loader, device, device.queue_family(), p->machine_->image_count());
        p->frameSync_ = vaml::vulkan::FrameSync::create(
            loader, device, vaml::vulkan::choose_concurrency(p->machine_->image_count()));
    }

    if (!p->machine_ || !p->commandPool_ || !p->frameSync_)
        return nullptr;  // the owned members are RAII-destroyed on p unwind
    return p;
}

Presenter::Presenter(const vaml::vulkan::VulkanLoader& loader,
                     const vaml::vulkan::Device& device)
    : loader_(&loader)
    , device_(&device)
{
}

Presenter::Presenter(Presenter&&) noexcept = default;
Presenter& Presenter::operator=(Presenter&&) noexcept = default;

Presenter::~Presenter()
{
    // Destroy order (teardown-order-reverse of the build): frame sync -> command
    // pool -> machine (its per-image framebuffers then its swapchain). The caller
    // must have quiesced the device (a vkDeviceWaitIdle) before destroying the
    // presenter so no submit references the sync/command/machine. The borrowed
    // device/surface/render-pass outlive the presenter (callers own that ordering).
    frameSync_.reset();
    commandPool_.reset();
    machine_.reset();
}

void Presenter::request_resize(std::uint32_t width, std::uint32_t height)
{
    // Queue the S167_1 two-generation atomic resize; applied on the next tick()
    // after a device quiesce. A zero/minimized size parks the machine.
    resizePending_ = true;
    resizeW_       = width;
    resizeH_       = height;
}

std::optional<std::uint32_t> Presenter::tick(bool invalidated, bool tickDue)
{
    const vaml::vulkan::VulkanLoader& vk  = *loader_;
    const vaml::vulkan::Device&       dev = *device_;

    // Apply a pending resize TRANSACTION (quiesce first, then prepare_recreate).
    if (resizePending_)
    {
        resizePending_ = false;
        if (vk.vkDeviceWaitIdle)
            vk.vkDeviceWaitIdle(dev.device());
        if (!machine_->prepare_recreate(
                resizeW_ > 0 ? resizeW_ : 1u,
                resizeH_ > 0 ? resizeH_ : 1u))
        {
            // Retryable (Gen N still live) / Suspended (parked) — nothing drawn this
            // tick; the state machine + feedback surface it. On Fatal, report once
            // (no auto-device-rebirth — deferred-not-silent).
            if (machine_->state() == vaml::vulkan::SwapchainState::Fatal && !fatalReported_)
            {
                fatalReported_ = true;
                if (options_.onPresentationLost)
                    options_.onPresentationLost(options_.userData, machine_->state());
            }
            return std::nullopt;
        }
    }

    // The change-driven gate + the S167_1 state contract.
    if (!should_render(invalidated, tickDue, machine_->state()))
        return std::nullopt;

    // Surface the stale-ghost feedback each tick (S167_1 reuse, not re-authored).
    if (options_.onFeedback)
        options_.onFeedback(options_.userData,
                            vaml::vulkan::feedback_for_state(machine_->state()));

    // Obey the machine: only a Draw action acquires/presents.
    if (iteration_action(machine_->state()) != IterationAction::Draw)
        return std::nullopt;

    if (!vk.has_command_loader())
        return std::nullopt;

    const std::uint32_t frames = frameSync_->frame_count();
    const std::uint32_t f      = frameIndex_ % frames;

    // Acquire the next swapchain image (waiting imageAvailable[f]).
    std::uint32_t imageIndex = 0;
    VkResult acq = vk.vkAcquireNextImageKHR(
        dev.device(), machine_->handle(), UINT64_MAX,
        frameSync_->image_available(f), VK_NULL_HANDLE, &imageIndex);

    // Follow the S167_1 state machine: DEVICE/SURFACE_LOST -> Fatal (engine stops),
    // reported once; never draw from an invalid generation.
    const vaml::vulkan::SwapchainState nextAcq = vaml::vulkan::next_state(
        machine_->state(), acq,
        vaml::vulkan::should_suspend_extent(machine_->extent()));
    if (nextAcq == vaml::vulkan::SwapchainState::Fatal)
    {
        machine_->mark_fatal();
        if (!fatalReported_)
        {
            fatalReported_ = true;
            if (options_.onPresentationLost)
                options_.onPresentationLost(options_.userData, machine_->state());
        }
        return std::nullopt;
    }
    if (acq != VK_SUCCESS)
        return std::nullopt;

    // Wait the in-flight fence for this frame's slot, then reset it.
    const VkFence inFlight = frameSync_->in_flight_fence(f);
    vk.vkWaitForFences(dev.device(), 1, &inFlight, VK_TRUE, UINT64_MAX);
    vk.vkResetFences(dev.device(), 1, &inFlight);

    VkCommandBuffer cmd = commandPool_->command_buffer(f);

    // The frame handshake slot must have a framebuffer for the acquired image.
    const vaml::vulkan::Framebuffer* fb = machine_->framebuffer(imageIndex);
    if (!fb || !cmd)
        return std::nullopt;

    // The app DECLARES its pipeline/scene; the presenter owns the loop + submit+
    // present. Record via the draw body against `cmd` for `imageIndex`.
    if (!options_.draw(options_.userData, *this, cmd, imageIndex))
        return std::nullopt;

    // Submit: waits imageAvailable (color-attachment stage), signals renderFinished
    // + the in-flight fence.
    const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphore waitSem   = frameSync_->image_available(f);
    VkSemaphore signalSem = frameSync_->render_finished(f);
    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &waitSem;
    submit.pWaitDstStageMask    = &waitStage;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores    = &signalSem;
    const VkResult submitRes = vk.vkQueueSubmit(dev.graphics_queue(), 1, &submit, inFlight);
    if (submitRes != VK_SUCCESS)
    {
        // A submit failure (e.g. DEVICE_LOST) escalates the machine to our Fatal
        // contract — report once, never draw next.
        machine_->mark_fatal();
        if (!fatalReported_)
        {
            fatalReported_ = true;
            if (options_.onPresentationLost)
                options_.onPresentationLost(options_.userData, machine_->state());
        }
        return std::nullopt;
    }

    // Present: waits renderFinished on the swapchain's acquired image.
    VkSwapchainKHR swapchain = machine_->handle();
    VkSemaphore    renderDone = frameSync_->render_finished(f);
    VkPresentInfoKHR present{};
    present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores    = &renderDone;
    present.swapchainCount     = 1;
    present.pSwapchains        = &swapchain;
    present.pImageIndices      = &imageIndex;
    const VkResult pres = vk.vkQueuePresentKHR(dev.graphics_queue(), &present);

    // S167_1: an out-of-date/suboptimal present means run a recreate at the live
    // extent next tick. DEVICE/SURFACE_LOST escalates to Fatal (engine stops).
    const vaml::vulkan::SwapchainState nextPres = vaml::vulkan::next_state(
        machine_->state(), pres,
        vaml::vulkan::should_suspend_extent(machine_->extent()));
    if (nextPres == vaml::vulkan::SwapchainState::Fatal)
    {
        machine_->mark_fatal();
        if (!fatalReported_)
        {
            fatalReported_ = true;
            if (options_.onPresentationLost)
                options_.onPresentationLost(options_.userData, machine_->state());
        }
        return std::nullopt;
    }
    if (vaml::vulkan::should_recreate_swapchain(pres))
    {
        // Queue a recreate at the machine's live extent (the recreate body runs on
        // the change-driven gate next iteration so the device is quiesced).
        const VkExtent2D e = machine_->extent();
        resizePending_ = true;
        resizeW_       = e.width;
        resizeH_       = e.height;
        // This present still advanced the frame slot.
    }

    ++frameIndex_;
    return f;
}

} // namespace vaml::presentation
