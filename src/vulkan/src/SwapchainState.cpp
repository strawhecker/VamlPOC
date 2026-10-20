// S167_1 — SwapchainState implementation.
//
// Implements the S167_1 pure decision seams (headless-testable) + the real
// two-generation atomic `SwapchainStateMachine` owner. See SwapchainState.h for
// the full split + the LIFETIMES contract + the corrective-to-S167 rationale.

#include <vaml/vulkan/SwapchainState.h>

#include <utility>

namespace vaml::vulkan {

bool should_suspend_extent(VkExtent2D extent) noexcept
{
    // A zero/minimized extent means "nothing to present, park until a resize
    // re-drives" — the Suspended condition. (The S164 choose_extent seam already
    // clamps to >=1 for a NORMAL surface extent; this catches the explicit
    // zero/minimized window case the resize loop reports before negotiation.)
    return extent.width == 0 || extent.height == 0;
}

SwapchainState next_state(SwapchainState current, VkResult result, bool extentZero) noexcept
{
    // Fatal is reachable ONLY from device/surface loss. Any other result (including
    // success / out-of-date / suboptimal) never escalates to Fatal here — the owner
    // drives Ready vs Retryable from the rebuild itself.
    if (result == VK_ERROR_DEVICE_LOST || result == VK_ERROR_SURFACE_LOST_KHR)
        return SwapchainState::Fatal;

    // A zero/minimized extent parks the machine until a resize re-drives it.
    if (extentZero)
        return SwapchainState::Suspended;

    // Otherwise the current generation stays authoritative (unchanged). VK_SUCCESS,
    // an out-of-date/suboptimal result, and a transient non-success all keep the
    // existing live generation presentable; the owner's prepare_recreate decides
    // whether a rebuild lands Ready or Retryable.
    return current;
}

FeedbackKind feedback_for_state(SwapchainState state) noexcept
{
    // Hard engine invariant: Retryable != Fatal. Retryable keeps showing Gen N's
    // last-good frame under a slow-breathing white FROST; Fatal is a FLAT white
    // overlay + OnPresentationLost. Ready/Suspended have no feedback (nothing
    // wrong to surface — Suspended parks quietly).
    switch (state)
    {
        case SwapchainState::Retryable: return FeedbackKind::RetryableFrost;
        case SwapchainState::Fatal:      return FeedbackKind::FatalWhite;
        case SwapchainState::Ready:
        case SwapchainState::Suspended:  return FeedbackKind::None;
    }
    return FeedbackKind::None;
}

SwapchainStateBuilders SwapchainStateBuilders::real(
    const VulkanLoader& loader,
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    const VulkanSurface& surface,
    VkRenderPass renderPass)
{
    SwapchainStateBuilders b;
    b.buildSwapchain = [&loader, device, physicalDevice, &surface](
        std::uint32_t w, std::uint32_t h) -> std::unique_ptr<Swapchain> {
        // NULLABLE: pass surface by reference (owned by the caller, must outlive the
        // machine). Return nullptr on any failure — the "stillborn generation" case.
        return Swapchain::create(loader, device, physicalDevice, surface, w, h);
    };
    b.buildFramebuffers = [&loader, device, renderPass](
        const Swapchain& sw) -> std::vector<std::unique_ptr<Framebuffer>> {
        std::vector<std::unique_ptr<Framebuffer>> fbs;
        if (sw.image_count() == 0)
            return fbs;
        fbs.reserve(sw.image_views().size());
        const VkImageView depthView = sw.depth_image_view();
        for (VkImageView view : sw.image_views())
        {
            auto fb = Framebuffer::create(
                loader, device, renderPass, sw.extent(),
                std::vector<VkImageView>{ view },
                depthView ? std::optional<VkImageView>(depthView) : std::nullopt);
            if (!fb)
                return {};  // empty => the stillborn-generation teardown path
            fbs.push_back(std::move(fb));
        }
        return fbs;
    };
    return b;
}

std::unique_ptr<SwapchainStateMachine>
SwapchainStateMachine::create(SwapchainStateBuilders builders,
                              std::uint32_t framebufferWidth,
                              std::uint32_t framebufferHeight)
{
    std::unique_ptr<SwapchainStateMachine> m(new SwapchainStateMachine(std::move(builders)));
    m->state_ = SwapchainState::Fatal;  // no valid generation yet

    auto genN = m->builders_.buildSwapchain(framebufferWidth, framebufferHeight);
    if (!genN)
        return nullptr;  // the one legitimate "no presentable generation": no Gen N to fall back to
    auto fbs = m->builders_.buildFramebuffers(*genN);
    if (fbs.empty())
        return nullptr;

    m->genN_           = std::move(genN);
    m->framebuffersN_  = std::move(fbs);
    m->state_          = SwapchainState::Ready;
    return m;
}

bool SwapchainStateMachine::prepare_recreate(std::uint32_t w, std::uint32_t h)
{
    // A zero/minimized extent is not a rebuild — park into Suspended (nothing to
    // present until a real size arrives). Never a Gen N+1 build of a zero extent.
    if (should_suspend_extent(VkExtent2D{ w, h }))
    {
        suspend();
        return false;
    }

    // --- Build Gen N+1 into fresh temporaries (the transactional model) ---
    // Local temporaries are RAII: on ANY failure below they are destroyed exactly
    // here, leaving Gen N fully live. The COMMIT only happens after every step.
    auto nextSwapchain = builders_.buildSwapchain(w, h);
    if (!nextSwapchain)
    {
        state_ = SwapchainState::Retryable;  // Gen N still live + presentable
        return false;
    }
    auto nextFramebuffers = builders_.buildFramebuffers(*nextSwapchain);
    if (nextFramebuffers.empty())
    {
        // Stillborn N+1: nextSwapchain (and any partial framebuffers) are destroyed
        // by RAII on unwinding this scope. Gen N stays live + presentable.
        state_ = SwapchainState::Retryable;
        return false;
    }

    // --- COMMIT: atomically swap active = N+1, then destroy Gen N ---
    // Move the new generation into place, and only then release the old one. The
    // old swapchain is destroyed AFTER the commit (never before), so there is no
    // instant where the engine holds a null/invalid presentable swapchain.
    genN_          = std::move(nextSwapchain);
    framebuffersN_ = std::move(nextFramebuffers);
    state_         = SwapchainState::Ready;
    return true;
}

SwapchainStateMachine::SwapchainStateMachine(SwapchainStateBuilders builders)
    : builders_(std::move(builders))
{
}

// Move ctor/assign: defaulted. The moved-from owner's genN_/framebuffersN_ become
// null/empty, so its dtor's release loop is a no-op — safe to move-assign.
SwapchainStateMachine::SwapchainStateMachine(SwapchainStateMachine&&) noexcept = default;
SwapchainStateMachine& SwapchainStateMachine::operator=(SwapchainStateMachine&&) noexcept = default;

SwapchainStateMachine::~SwapchainStateMachine()
{
    // Destruction runs in REVERSE build order: framebuffersN_ (declared LAST,
    // destroyed FIRST) before genN_ (the swapchain). A framebuffer references the
    // swapchain's image views, so the framebuffers must go before the swapchain —
    // teardown-order-reverse, no dangling view reference. (The loader/device must
    // still be alive here; callers own that ordering — the header LIFETIMES.)
    framebuffersN_.clear();
    genN_.reset();
}

} // namespace vaml::vulkan
