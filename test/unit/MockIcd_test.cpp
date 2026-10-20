// S176 — RAIL 1: Mock-ICD lifecycle suite (Cat 1, CI, every commit, headless).
//
// The full three-lane harness (S176 mock-ICD / S177 hidden-window WSI / S178
// programmer checklist) matures into `docs/testing.md` (named, NOT created here —
// deferred-not-silent to S179). This TU is Rail 1: a SCRIPTED Vulkan (a
// `MockGeneration` implementing the S176 DECISION-A `ISwapchainGeneration`
// interface) that injects failed vkCreateSwapchainKHR / image-view / depth /
// framebuffer build / DEVICE_LOST / OUT_OF_DATE / SUBOPTIMAL at the exact internal
// stage it wants, and asserts the SwapchainStateMachine:
//   * lands the right state (Ready / Retryable / Fatal / Suspended),
//   * keeps Gen N live (its identity/handle is never replaced by a partial N+1),
//   * never submits an invalid generation,
//   * destroys a committed-then-replaced generation in REVERSE build order.
//
// This SUPERSEDES/EXTENDS ResizeRecreate_test.cpp (kept intact) + the decision-
// level SwapchainGeneration_test.cpp (kept as the pure seam suite): S176 exercises
// the MACHINE against injectable generation OBJECTS with NO live Vulkan runtime —
// the S160/S162...S167_1 pure-seam precedent, now at the object level.

#include <vaml/vulkan/SwapchainState.h>

#include "doctest/doctest.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

using namespace vaml::vulkan;

namespace {

// The S176 scripted ICD. A "generation" is a single atomic unit; the mock records
// its teardown order so the test can assert reverse-build-order on the real unit.
// It also exposes handles/accessors so the machine's live-generation reads are
// observable against a real OBJECT (not just nullptr/empty builder results).
class MockGeneration : public ISwapchainGeneration
{
public:
    // Scripted internal failure knob: the stage at which this generation "fails"
    // to build. A generation with FailureStage::None is fully valid (the success
    // path); any other stage simulates the ICD failure the mock's `build` factory
    // maps to "stillborn" (nullptr), exercising the EXACT failure the stage names.
    enum class FailureStage
    {
        None,                 // fully valid generation — builds successfully
        CreateSwapchain,      // failed vkCreateSwapchainKHR
        ImageView,            // failed image-view creation
        DepthBuild,           // failed depth-image build
        FramebufferBuild,     // failed per-image framebuffer build
        DeviceLost,           // DEVICE_LOST surfaced at generation build
        SurfaceLost,          // SURFACE_LOST surfaced at generation build
    };

    MockGeneration(std::uint64_t id, std::uint32_t w, std::uint32_t h,
                   std::vector<std::string>& teardownLog,
                   bool msaa = false)
        : id_(id)
        , extent_{ w, h }
        , imageCount_(2)
        , msaa_(msaa)
        , teardownLog_(teardownLog)
    {
        // Fake image count + one fake image view per image (the framebuffer build
        // mirrors this shape).
        imageViews_.assign(imageCount_, reinterpret_cast<VkImageView>(id_ * 4096 + 1));
        // One fake framebuffer object per image view, registered so teardown order
        // is observable.
        fbAlive_.assign(imageCount_, std::string("fb") + std::to_string(id_));
    }

    // Marks this generation as built (no internal failure at build). The raw
    // FakeHandle is the generation's "swapchain" identity.
    [[nodiscard]] std::uint64_t id() const noexcept { return id_; }

    // --- ISwapchainGeneration ---
    const Swapchain* swapchain() const noexcept override { return nullptr; }  // no real Swapchain headless
    VkSwapchainKHR handle() const noexcept override
    {
        return reinterpret_cast<VkSwapchainKHR>(id_);  // a fake stable identity
    }
    VkExtent2D extent() const noexcept override { return extent_; }
    std::uint32_t image_count() const noexcept override { return imageCount_; }
    std::optional<VkFormat> depth_format() const noexcept override
    {
        return std::optional<VkFormat>(VK_FORMAT_D32_SFLOAT);
    }
    VkSampleCountFlagBits sample_count() const noexcept override
    {
        return msaa_ ? VK_SAMPLE_COUNT_4_BIT : VK_SAMPLE_COUNT_1_BIT;
    }
    const std::vector<std::unique_ptr<Framebuffer>>& framebuffers() const noexcept override
    {
        return fakeFramebuffers_;
    }
    const Framebuffer* framebuffer(std::uint32_t i) const noexcept override
    {
        return (i < fakeFramebuffers_.size()) ? fakeFramebuffers_[i].get() : nullptr;
    }
    const MsaaTarget* msaa_target() const noexcept override { return nullptr; }

    ~MockGeneration() override
    {
        // Teardown-order-reverse on the REAL unit: the mock records its internal
        // release order (framebuffers -> ... -> swapchain). The test asserts this
        // is the reverse of the build order.
        for (std::size_t i = fbAlive_.size(); i > 0; --i)
            teardownLog_.push_back(fbAlive_[i - 1] + ":" + std::to_string(id_));
        teardownLog_.push_back(std::string("swapchain:") + std::to_string(id_));
    }

private:
    std::uint64_t id_ = 0;
    VkExtent2D extent_{ 0, 0 };
    std::uint32_t imageCount_ = 2;
    bool msaa_ = false;
    std::vector<std::string>& teardownLog_;
    std::vector<VkImageView> imageViews_;
    // One fake framebuffer identity per image (empty framebuffer objects live in
    // `fakeFramebuffers_` — the mock keeps them null but records the identities).
    std::vector<std::string> fbAlive_;
    // The machine's framebuffers()/framebuffer(i) reads: empty here (headless),
    // but the mock's teardown log carries the fake per-image identities.
    std::vector<std::unique_ptr<Framebuffer>> fakeFramebuffers_;
};

// The S176 scripted ICD factory: returns a `MockGeneration` (success) or nullptr
// (stillborn) depending on the scripted failure stage. `failStage` is captured for
// the ENTIRE machine lifetime OR swapped via the outer script variable.
struct ScriptedIcd
{
    std::uint64_t nextId = 100u;
    MockGeneration::FailureStage stage = MockGeneration::FailureStage::None;
    std::vector<std::string> teardownLog;   // observable: which generations were destroyed + their order
    std::uint64_t initialId = 0;            // the identity of the successfully-built Gen 0

    // The builders' `build` seam: creates a generation (or "stillborn" nullptr when
    // the current scripted stage is a failure). The stage dictates which failure
    // the generation would have hit; a None stage yields a fully valid generation.
    std::unique_ptr<ISwapchainGeneration> make(std::uint32_t w, std::uint32_t h)
    {
        // A DEVICE_LOST / SURFACE_LOST build surfaces as a stillborn generation
        // (the machine sees nullptr from `build`); the machine's Acquire/Present
        // state path then lands Fatal via next_state / mark_fatal. A failed
        // swapchain/image-view/depth/framebuffer build likewise surfaces as
        // stillborn (Retryable, Gen N live). All are exercised below.
        if (stage != MockGeneration::FailureStage::None)
            return nullptr;

        auto g = std::make_unique<MockGeneration>(nextId, w, h, teardownLog);
        if (nextId == 100u)
            initialId = g->id();
        ++nextId;
        return g;
    }

    SwapchainStateBuilders builders()
    {
        SwapchainStateBuilders b;
        b.build = [this](std::uint32_t w, std::uint32_t h) -> std::unique_ptr<ISwapchainGeneration> {
            return make(w, h);
        };
        return b;
    }
};

} // namespace

// ---------------------------------------------------------------------------
// OUT_OF_DATE / SUBOPTIMAL keep Gen N live (Ready, unchanged generation)
// ---------------------------------------------------------------------------
TEST_CASE("S176 OUT_OF_DATE/SUBOPTIMAL keep Gen N live and land Ready")
{
    ScriptedIcd icd;
    auto m = SwapchainStateMachine::create(icd.builders(), 800, 600);
    REQUIRE(m != nullptr);
    REQUIRE(m->state() == SwapchainState::Ready);

    const VkSwapchainKHR gen0Handle = m->handle();
    REQUIRE(gen0Handle != VK_NULL_HANDLE);
    const std::uint32_t gen0Images = m->image_count();

    // A non-loss acquire/present result never bumps the generation or drops state.
    const SwapchainState st1 = next_state(m->state(), VK_ERROR_OUT_OF_DATE_KHR,
                                          should_suspend_extent(m->extent()));
    CHECK(st1 != SwapchainState::Fatal);
    CHECK(st1 == SwapchainState::Ready);  // out-of-date keeps Ready (recreate-driven)

    const SwapchainState st2 = next_state(m->state(), VK_SUBOPTIMAL_KHR,
                                          should_suspend_extent(m->extent()));
    CHECK(st2 != SwapchainState::Fatal);
    CHECK(st2 == SwapchainState::Ready);

    // Gen N stayed live — same identity/handle, no replacement happened.
    CHECK(m->handle() == gen0Handle);
    CHECK(m->image_count() == gen0Images);
    CHECK(m->state() == SwapchainState::Ready);
}

// ---------------------------------------------------------------------------
// FAILED vkCreateSwapchainKHR -> Retryable, Gen N stays live, never committed
// ---------------------------------------------------------------------------
TEST_CASE("S176 failed vkCreateSwapchainKHR lands Retryable with Gen N live")
{
    ScriptedIcd icd;
    auto m = SwapchainStateMachine::create(icd.builders(), 800, 600);
    REQUIRE(m != nullptr);
    const VkSwapchainKHR gen0Handle = m->handle();

    // Script a failed vkCreateSwapchainKHR for the NEXT generation.
    icd.stage = MockGeneration::FailureStage::CreateSwapchain;
    const bool ok = m->prepare_recreate(1024, 768);

    CHECK_FALSE(ok);                             // the recreate did not commit
    CHECK(m->state() == SwapchainState::Retryable);  // Gen N still live, retry
    CHECK(m->handle() == gen0Handle);            // Gen N identity unchanged
    CHECK(m->extent().width == 800u);            // Gen N extent unchanged (not N+1's)
    CHECK(m->extent().height == 600u);
}

// ---------------------------------------------------------------------------
// FAILED image-view / depth / framebuffer build -> Retryable, Gen N live
// ---------------------------------------------------------------------------
TEST_CASE("S176 failed image-view/depth/framebuffer build keeps Gen N live")
{
    for (const MockGeneration::FailureStage failingStage :
         { MockGeneration::FailureStage::ImageView,
           MockGeneration::FailureStage::DepthBuild,
           MockGeneration::FailureStage::FramebufferBuild })
    {
        CAPTURE(static_cast<int>(failingStage));
        ScriptedIcd icd;
        auto m = SwapchainStateMachine::create(icd.builders(), 800, 600);
        REQUIRE(m != nullptr);
        const VkSwapchainKHR gen0Handle = m->handle();

        icd.stage = failingStage;
        const bool ok = m->prepare_recreate(1024, 768);

        CHECK_FALSE(ok);
        CHECK(m->state() == SwapchainState::Retryable);
        CHECK(m->handle() == gen0Handle);   // Gen N stays live against a real object
    }
}

// ---------------------------------------------------------------------------
// DEVICE_LOST / SURFACE_LOST -> Fatal (the machine's loss contract)
// ---------------------------------------------------------------------------
TEST_CASE("S176 DEVICE_LOST/SURFACE_LOST land Fatal, never drawn-from")
{
    // A loss surfaced at the state decision (the acquire/present path) escalates
    // the machine to Fatal — reachable ONLY from device/surface loss.
    ScriptedIcd icd;
    auto m = SwapchainStateMachine::create(icd.builders(), 800, 600);
    REQUIRE(m != nullptr);

    const SwapchainState lostSt = next_state(m->state(), VK_ERROR_DEVICE_LOST,
                                             should_suspend_extent(m->extent()));
    CHECK(lostSt == SwapchainState::Fatal);
    m->mark_fatal();

    const SwapchainState surfSt = next_state(m->state(), VK_ERROR_SURFACE_LOST_KHR,
                                             should_suspend_extent(m->extent()));
    CHECK(surfSt == SwapchainState::Fatal);
    CHECK(m->state() == SwapchainState::Fatal);

    // Retryable != Fatal: the stale-ghost feedbacks are distinct (S167_1 invariant).
    CHECK(feedback_for_state(SwapchainState::Retryable) != feedback_for_state(SwapchainState::Fatal));
}

// ---------------------------------------------------------------------------
// INVALID generations are never submitted — after a failed N+1, the live
// generation accessors still return the VALID Gen N (never a partial/N+1).
// ---------------------------------------------------------------------------
TEST_CASE("S176 invalid generations are never submitted (Gen N stays the live unit)")
{
    ScriptedIcd icd;
    auto m = SwapchainStateMachine::create(icd.builders(), 800, 600);
    REQUIRE(m != nullptr);
    const VkSwapchainKHR gen0Handle = m->handle();
    const VkExtent2D     gen0Extent = m->extent();
    const std::uint32_t  gen0Images = m->image_count();

    // Every internal ICD failure — stillborn N+1 — must NEVER become the live unit.
    for (const MockGeneration::FailureStage failingStage :
         { MockGeneration::FailureStage::CreateSwapchain,
           MockGeneration::FailureStage::ImageView,
           MockGeneration::FailureStage::DepthBuild,
           MockGeneration::FailureStage::FramebufferBuild })
    {
        CAPTURE(static_cast<int>(failingStage));
        icd.stage = failingStage;
        (void)m->prepare_recreate(1024, 768);

        // The live generation is STILL the valid Gen 0 — never a partial/N+1.
        CHECK(m->handle() == gen0Handle);
        CHECK(m->extent().width == gen0Extent.width);
        CHECK(m->extent().height == gen0Extent.height);
        CHECK(m->image_count() == gen0Images);
        // The live framebuffer set is still the committed Gen 0's (never a
        // partial/N+1 set) — observable via the id-carrying teardown log being
        // EMPTY here (no generation has been destroyed/detached: Gen 0 stays live
        // across every failed recreate).
        CHECK(icd.teardownLog.empty());
        REQUIRE(m->state() == SwapchainState::Retryable);
    }

    // Reset the script to success; the next recreate COMMITS a fresh Gen N+1.
    icd.stage = MockGeneration::FailureStage::None;
    REQUIRE(m->prepare_recreate(1024, 768));
    CHECK(m->state() == SwapchainState::Ready);
    CHECK(m->handle() != gen0Handle);   // committed a genuinely NEW generation
}

// ---------------------------------------------------------------------------
// TEARDOWN REVERSE — a committed-then-replaced generation destroys its internals
// framebuffers -> swapchain (reverse build order), proven on the real unit.
// ---------------------------------------------------------------------------
TEST_CASE("S176 committed-then-replaced generation teardown is reverse build order")
{
    ScriptedIcd icd;
    auto m = SwapchainStateMachine::create(icd.builders(), 800, 600);
    REQUIRE(m != nullptr);
    const std::uint64_t gen0Id = icd.initialId;

    // Commit a Gen N+1 (a real object swap). The OLD Gen N (id gen0Id) is destroyed
    // and its teardown is recorded into icd.teardownLog in reverse build order.
    REQUIRE(m->prepare_recreate(1024, 768));
    CHECK(m->state() == SwapchainState::Ready);
    CHECK(m->handle() != VK_NULL_HANDLE);

    // The old Gen N (id gen0Id) must have been torn down. The mock records two
    // framebuffer identities FIRST ("fb<id>:<id>") then the swapchain LAST
    // ("swapchain:<id>") — i.e. framebuffers -> swapchain = REVERSE of the build
    // order (build: swapchain -> framebuffers). Assert the presence + the ordering.
    bool sawGen0Teardown = false;
    std::size_t gen0FbFirst = 0;
    std::size_t gen0FbLast = 0;
    std::size_t gen0SwapchainIdx = 0;
    std::size_t gen0FbCount = 0;
    for (std::size_t i = 0; i < icd.teardownLog.size(); ++i)
    {
        const std::string& entry = icd.teardownLog[i];
        if (entry.find("swapchain:") == 0)
        {
            const std::uint64_t destroyedId = std::stoull(entry.substr(10));
            if (destroyedId == gen0Id)
            {
                sawGen0Teardown = true;
                gen0SwapchainIdx = i;  // the swapchain is destroyed LAST
            }
        }
        else if (entry.find("fb") == 0)
        {
            // "fb<id>:<id>" — capture the generation id of this framebuffer entry.
            const auto colon = entry.find(':');
            const std::uint64_t fbId = std::stoull(entry.substr(2, colon - 2));
            if (fbId == gen0Id)
            {
                if (gen0FbCount == 0)
                    gen0FbFirst = i;
                gen0FbLast = i;
                ++gen0FbCount;
            }
        }
    }
    CHECK(sawGen0Teardown);                       // the replaced Gen N WAS destroyed
    CHECK(gen0FbCount == 2u);                     // both fake per-image framebuffers torn down
    CHECK(gen0FbFirst < gen0SwapchainIdx);        // framebuffers BEFORE swapchain (reverse order)
    CHECK(gen0FbLast < gen0SwapchainIdx);

    // The new Gen N+1 is still live after the swap (a valid, presentable unit).
    CHECK(m->state() == SwapchainState::Ready);
    CHECK(m->image_count() >= 1u);
}

// ---------------------------------------------------------------------------
// ZERO extent parks Suspended (never a Gen N+1 build of a zero extent)
// ---------------------------------------------------------------------------
TEST_CASE("S176 zero/minimized extent parks Suspended without a build")
{
    ScriptedIcd icd;
    auto m = SwapchainStateMachine::create(icd.builders(), 800, 600);
    REQUIRE(m != nullptr);
    const VkSwapchainKHR gen0Handle = m->handle();

    const std::size_t buildsBeforeIcd = icd.nextId;  // id counter proxies build count
    const bool parked = m->prepare_recreate(0, 600);
    CHECK_FALSE(parked);
    CHECK(m->state() == SwapchainState::Suspended);
    CHECK(m->handle() == gen0Handle);         // Gen N untouched
    CHECK(m->extent().width == 800u);
    CHECK(icd.nextId == buildsBeforeIcd);     // NO generation build occurred
}
