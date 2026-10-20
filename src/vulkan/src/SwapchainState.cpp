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

namespace {

// The S176 real generation: a complete, atomic swapchain generation owned as ONE
// unit (swapchain + S168 msaa target + per-image framebuffers). Its own
// destructor destroys its internals in REVERSE build order (framebuffers ->
// msaa target -> swapchain), which is what makes the S176 "Gen N stays live +
// teardown-order-reverse against a real generation unit" provable on CI.
class RealGeneration : public ISwapchainGeneration
{
public:
    RealGeneration(std::unique_ptr<Swapchain> sw,
                   std::unique_ptr<MsaaTarget> msaa,
                   std::vector<std::unique_ptr<Framebuffer>> fbs)
        : swapchain_(std::move(sw))
        , msaa_(std::move(msaa))
        , framebuffers_(std::move(fbs))
    {
    }

    ~RealGeneration() override
    {
        // Teardown-order-reverse (the S170 mirror proven on the real unit now):
        // framebuffers first (they reference BOTH the swapchain's image views and
        // the msaa target's views), then the msaa target (its images/memory), then
        // the swapchain. Declarations are swapchain_ (destroyed LAST via reverse
        // order) -> msaa_ -> framebuffers_ (destroyed FIRST). The explicit clears
        // are belt-and-suspenders to make the order self-documenting.
        framebuffers_.clear();
        msaa_.reset();
        swapchain_.reset();
    }

    const Swapchain* swapchain() const noexcept override { return swapchain_.get(); }
    VkSwapchainKHR handle() const noexcept override
    {
        return swapchain_ ? swapchain_->swapchain() : VK_NULL_HANDLE;
    }
    VkExtent2D extent() const noexcept override
    {
        return swapchain_ ? swapchain_->extent() : VkExtent2D{ 0, 0 };
    }
    std::uint32_t image_count() const noexcept override
    {
        return swapchain_ ? swapchain_->image_count() : 0u;
    }
    std::optional<VkFormat> depth_format() const noexcept override
    {
        return swapchain_ ? swapchain_->depth_format() : std::nullopt;
    }
    VkSampleCountFlagBits sample_count() const noexcept override
    {
        return msaa_ ? msaa_->sample_count() : VK_SAMPLE_COUNT_1_BIT;
    }
    const std::vector<std::unique_ptr<Framebuffer>>& framebuffers() const noexcept override
    {
        return framebuffers_;
    }
    const Framebuffer* framebuffer(std::uint32_t i) const noexcept override
    {
        return (i < framebuffers_.size()) ? framebuffers_[i].get() : nullptr;
    }
    const MsaaTarget* msaa_target() const noexcept override { return msaa_.get(); }

private:
    // Declared so CURRENT destruction order is framebuffers -> msaa -> swapchain
    // (reverse of declaration: swapchain_ declared first = destroyed last).
    std::unique_ptr<Swapchain>              swapchain_;
    std::unique_ptr<MsaaTarget>             msaa_;
    std::vector<std::unique_ptr<Framebuffer>> framebuffers_;
};

} // namespace

SwapchainStateBuilders SwapchainStateBuilders::real(
    const VulkanLoader& loader,
    const Device& device,
    const VulkanSurface& surface,
    VkRenderPass renderPass,
    VkSampleCountFlagBits sampleCount)
{
    SwapchainStateBuilders b;
    const bool msaa = sampleCount != VK_SAMPLE_COUNT_1_BIT;
    b.build = [&loader, &device, &surface, renderPass, msaa, sampleCount](
        std::uint32_t w, std::uint32_t h) -> std::unique_ptr<ISwapchainGeneration> {
        // Build the whole generation transactionally: swapchain (+ image views +
        // depth) -> S168 msaa target -> per-image framebuffers. ANY failure returns
        // nullptr (the stillborn generation); the partial temporaries are released
        // by RAII on this scope unwind. The caller (the machine) treats nullptr as
        // "stillborn — Gen N stays live".
        auto sw = Swapchain::create(loader, device, surface, w, h);
        if (!sw)
            return nullptr;

        // S168: when the pass is multisampled (msaa), build the multisampled target
        // from the swapchain's color/depth formats + extent. Non-MSAA -> no target.
        std::unique_ptr<MsaaTarget> nextMsaa;
        if (msaa)
        {
            nextMsaa = MsaaTarget::create(loader, device, sw->color_format(),
                                          sw->depth_format(), sw->extent(), sampleCount);
            if (!nextMsaa)
                return nullptr;  // a failed msaa build is a stillborn generation
        }

        // Per-image framebuffers. MSAA-aware: uses the msaa layout ([msaa color,
        // resolve, msaa depth?]) vs the classic non-MSAA layout ([colorView, depth?]).
        std::vector<std::unique_ptr<Framebuffer>> fbs;
        if (sw->image_count() != 0)
        {
            fbs.reserve(sw->image_views().size());
            if (nextMsaa && nextMsaa->valid())
            {
                for (VkImageView view : sw->image_views())
                {
                    auto fb = Framebuffer::create_msaa(
                        loader, device, renderPass, sw->extent(), *nextMsaa, view);
                    if (!fb)
                        return nullptr;  // stillborn generation teardown path
                    fbs.push_back(std::move(fb));
                }
            }
            else
            {
                const VkImageView depthView = sw->depth_image_view();
                for (VkImageView view : sw->image_views())
                {
                    auto fb = Framebuffer::create(
                        loader, device, renderPass, sw->extent(),
                        std::vector<VkImageView>{ view },
                        depthView ? std::optional<VkImageView>(depthView) : std::nullopt);
                    if (!fb)
                        return nullptr;  // stillborn generation teardown path
                    fbs.push_back(std::move(fb));
                }
            }
        }

        if (fbs.size() != sw->image_views().size())
            return nullptr;  // incomplete per-image framebuffer set => stillborn

        return std::make_unique<RealGeneration>(std::move(sw), std::move(nextMsaa), std::move(fbs));
    };
    return b;
}

std::unique_ptr<SwapchainStateMachine>
SwapchainStateMachine::create(SwapchainStateBuilders builders,
                              std::uint32_t framebufferWidth,
                              std::uint32_t framebufferHeight,
                              VkSampleCountFlagBits sampleCount)
{
    std::unique_ptr<SwapchainStateMachine> m(new SwapchainStateMachine(std::move(builders), sampleCount));
    m->state_ = SwapchainState::Fatal;  // no valid generation yet

    // Build the WHOLE initial generation through the single S176 `build` seam. A
    // failed generation (any internal ICD failure) returns nullptr — the one
    // legitimate "no presentable generation": there is no Gen N to fall back to.
    auto genN = m->builders_.build(framebufferWidth, framebufferHeight);
    if (!genN)
        return nullptr;

    m->genN_  = std::move(genN);
    m->state_ = SwapchainState::Ready;
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

    // --- Build Gen N+1 into a fresh temporary (the transactional model) ---
    // The temporary is RAII: on ANY failure it is destroyed exactly here, leaving
    // Gen N fully live. The COMMIT only happens after the whole generation built.
    // (Any internal ICD failure — vkCreateSwapchainKHR / image-view / depth /
    // framebuffer / DEVICE_LOST — surfaces as nullptr from the S176 `build` seam.)
    auto nextGen = builders_.build(w, h);
    if (!nextGen)
    {
        // Stillborn N+1: Gen N stays live + presentable. Land Retryable (keep
        // drawing N, retry).
        state_ = SwapchainState::Retryable;
        return false;
    }

    // --- COMMIT: atomically swap active = N+1, then destroy Gen N ---
    // Move the new generation into place, and only then release the old one. The
    // old generation (via its own reverse-order destructor) is destroyed AFTER the
    // commit (never before), so there is no instant where the engine holds a
    // null/invalid presentable swapchain.
    genN_  = std::move(nextGen);
    state_ = SwapchainState::Ready;
    return true;
}

SwapchainStateMachine::SwapchainStateMachine(SwapchainStateBuilders builders,
                                             VkSampleCountFlagBits sampleCount)
    : sampleCount_(sampleCount)
    , builders_(std::move(builders))
{
}

// Move ctor/assign: defaulted. The moved-from owner's genN_/framebuffersN_ become
// null/empty, so its dtor's release loop is a no-op — safe to move-assign.
SwapchainStateMachine::SwapchainStateMachine(SwapchainStateMachine&&) noexcept = default;
SwapchainStateMachine& SwapchainStateMachine::operator=(SwapchainStateMachine&&) noexcept = default;

SwapchainStateMachine::~SwapchainStateMachine()
{
    // The machine owns a SINGLE generation (`genN_`, an ISwapchainGeneration).
    // The generation's OWN destructor performs the reverse-build-order teardown
    // (framebuffers -> msaa target -> swapchain), so the machine simply releases
    // the one atomic unit. (The `emptyFramebuffers_` + other members are trivial.)
    genN_.reset();
}

} // namespace vaml::vulkan
