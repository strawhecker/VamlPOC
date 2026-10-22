// S212 (dep:S211) — MERGE POINT: the first real vertical slice, headless (Rail 1).
//
// Proves the CPU/merge half of "UI declared in VAML renders to window via the full
// S5 path" WITHOUT a live Vulkan runtime (the standing S160+ precedent), through
// the vaml::presentation::VamlRenderer host:
//   (1) VamlReader::Load a VAML doc (real panels via S091/S092) -> root dynamic_casts
//       to UIElement (the REAL core visual tree).
//   (2) Measure/Arrange -> the merge's encoder (encode_visual_slice) produces the
//       expected S206 DrawItems from REAL arranged final_rects.
//   (3) The default vertical-slice classifier fills each real UIElement panel at its
//       final_rect (the accepted judgment — the honest first slice of the layout);
//       the encoder's skip rule drops a degenerate (zero-extent) panel honestly.
//   (4) The producer/consumer contract: invalidate() arranges + encodes + publishes a
//       NEWER version; an unchanged frame does NOT re-encode (should_reencode /
//       should_consume + the MarshalledScene gates).
//   (5) The host's DrawBody seam = render_loop_steps() (the S210/S211 fixed loop,
//       reused) + presenter_lifetimes_ok (the S173 gate, reused).
//   (6) VamlRenderer is move-only RAII (the S170 discipline).
//
// NO live Vulkan/device/surface/render-pass constructed. The device-backed
// create_device path (SceneRenderer + Presenter + the internally-wired DrawBody) is
// [both apps, not CI] per the S206/S210/S211 precedent.

#include <vaml/presentation/VamlRenderer.h>

#include <vaml/core/Element.h>      // StackPanel (re-based onto UIElement)
#include <vaml/core/UIElement.h>    // UIElement, Visibility
#include <vaml/core/VamlReader.h>   // VamlReader::Load (the VAML disk -> tree path)
#include <vaml/core/Geometry.h>     // Size, Rect

#include "doctest/doctest.h"

#include <cstdint>
#include <memory>
#include <type_traits>
#include <vector>

using namespace vaml::presentation;
using vaml::vulkan::ui::DrawItem;
using vaml::vulkan::ui::DrawItemKind;
using vaml::vulkan::ui::RenderLoopStep;
using vaml::vulkan::ui::solid_rect;

namespace {

// A VAML-declared single-panel dock (real panels only, per the current control set).
const char* kSliceDoc = R"(
<StackPanel x:Name="root" xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" />
)";

// A test-local content leaf (a UIElement with a real MeasureOverride content extent)
// so a hand-built panel tree has NON-degenerate final_rects for the multi-node slice
// proof. Not a control seed — test/POC-local, mirroring PocLeaf/RecordLeaf.
class ContentLeaf : public vaml::UIElement
{
public:
    explicit ContentLeaf(float w, float h) : extent_{ w, h } {}

protected:
    vaml::Size MeasureOverride(const vaml::Size& /*available*/) const override
    {
        return extent_;
    }

private:
    vaml::Size extent_;
};

} // namespace

TEST_CASE("S212: VamlReader::Load -> root is a UIElement (the REAL core visual tree)")
{
    vaml::VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSliceDoc);
    REQUIRE(root != nullptr);
    // The Load root (a StackPanel, re-based onto UIElement at S129) dynamic_casts to
    // UIElement — the vertical slice operates on the REAL core visual tree.
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(root.get());
    CHECK(uiRoot != nullptr);
}

TEST_CASE("S212: arrange the Load tree -> the encoder emits the root at its real final_rect")
{
    vaml::VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSliceDoc);
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(root.get());
    REQUIRE(uiRoot != nullptr);

    // Arrange the root at a 400x300 frame (the S123/S124 layout writes final_rect()).
    uiRoot->Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    uiRoot->Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 300.0f });
    CHECK(uiRoot->final_rect().width == doctest::Approx(400.0f));
    CHECK(uiRoot->final_rect().height == doctest::Approx(300.0f));

    // The merge's encoder over the arranged real tree with the default classifier
    // -> a SOLID fill at the root's real final_rect (the declared-layout slice).
    std::vector<DrawItem> out;
    encode_visual_slice(*uiRoot, default_visual_of(), out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].kind == DrawItemKind::SolidRect);
    CHECK(out[0].x == doctest::Approx(0.0f));
    CHECK(out[0].y == doctest::Approx(0.0f));
    CHECK(out[0].w == doctest::Approx(400.0f));
    CHECK(out[0].h == doctest::Approx(300.0f));
}

TEST_CASE("S212: default visual-of fills every real panel at its final_rect (accepted judgment)")
{
    // A hand-built panel tree with real content leaves (non-degenerate extents) so
    // EVERY node carries a real final_rect — the multi-node slice.
    vaml::StackPanel root;
    ContentLeaf* a = root.create_child<ContentLeaf>(100.0f, 40.0f);
    ContentLeaf* b = root.create_child<ContentLeaf>(100.0f, 60.0f);

    root.Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 200.0f });

    std::vector<DrawItem> out;
    encode_visual_slice(root, default_visual_of(), out);

    // Default slice classifier fills every UIElement: the root panel + both leaves
    // (all re-based UIElement) = 3 solid items in pre-order (root, a, b).
    REQUIRE(out.size() == 3u);
    for (const DrawItem& item : out)
        CHECK(item.kind == DrawItemKind::SolidRect);

    // The leaves' items match their ARRANGED final_rects exactly (the render slot).
    CHECK(out[1].x == doctest::Approx(a->final_rect().x));
    CHECK(out[1].y == doctest::Approx(a->final_rect().y));
    CHECK(out[1].w == doctest::Approx(a->final_rect().width));
    CHECK(out[1].h == doctest::Approx(a->final_rect().height));
    CHECK(out[2].x == doctest::Approx(b->final_rect().x));
    CHECK(out[2].y == doctest::Approx(b->final_rect().y));
    CHECK(out[2].w == doctest::Approx(b->final_rect().width));
    CHECK(out[2].h == doctest::Approx(b->final_rect().height));
}

TEST_CASE("S212: the encoder honestly skips a degenerate (zero-extent) panel")
{
    // A StackPanel root whose NON-last child is an EMPTY panel (0 desired height)
    // -> degenerate final_rect -> the encoder's ui_item_degenerate rule drops it,
    // while the last-child-stretch panel fills the remaining track. This is the REAL
    // layout: an empty container contributes no geometry, honestly skipped.
    vaml::StackPanel root;
    vaml::StackPanel* empty = root.create_child<vaml::StackPanel>();
    ContentLeaf* fill = root.create_child<ContentLeaf>(100.0f, 50.0f);

    root.Measure(vaml::Size{ 400.0f, vaml::Size::Infinite().height });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 200.0f });

    std::vector<DrawItem> out;
    encode_visual_slice(root, default_visual_of(), out);

    // root + fill (the last-child-stretch panel) emit; the empty panel (zero extent)
    // is skipped by the encoder's degenerate rule but its subtree still walks.
    REQUIRE(out.size() == 2u);
    CHECK(out[0].h == doctest::Approx(200.0f));         // the root fills the frame
    CHECK(out[1].h == doctest::Approx(fill->final_rect().height));
    (void)empty;
}

TEST_CASE("S212: producer/consumer contract — invalidate publishes ever-newer; unchanged does not re-encode")
{
    // The pre-arrange retained/re-encode + drain gates (reused from S210/S211).
    CHECK(should_reencode(true) == true);
    CHECK(should_reencode(false) == false);
    CHECK(should_consume(true) == true);
    CHECK(should_consume(false) == false);

    // A headless VamlRenderer host (create() — no device needed).
    auto host = VamlRenderer::create();
    REQUIRE(host != nullptr);

    // No root yet -> invalidate publishes nothing.
    CHECK(host->invalidate() == 0u);

    vaml::VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(kSliceDoc);
    auto* uiRoot = dynamic_cast<vaml::UIElement*>(root.get());
    REQUIRE(uiRoot != nullptr);

    // Attach the VAML tree + a frame size; the first invalidate arranges + encodes +
    // publishes a NEWER version (scene_version 1) to the host's postbox.
    CHECK(host->set_root(uiRoot) == true);
    host->set_frame_size(400.0f, 300.0f);
    const std::uint64_t v1 = host->invalidate();
    CHECK(v1 == 1u);
    CHECK(host->postbox().scene_version() == 1u);
    CHECK(host->postbox().last_consumed_version() == 0u);
    CHECK(host->postbox().pending() == false);   // published (not backpressured)

    // A SECOND invalidate BEFORE the render side drains is BACKPRESSURED (the S211
    // lock-free contract): the producer may mutate/queue as often as it likes between
    // frames (coalescing latest-wins, producer-private), but the lock-free flip
    // cannot proceed until the destination slot is consumed — publish() returns the
    // CURRENT published version (1) and RETAINS the newer update (pending) for the
    // next publish, never lost, never torn.
    const std::uint64_t v2 = host->invalidate();
    CHECK(v2 == 1u);                             // backpressured (the current published)
    CHECK(host->postbox().scene_version() == 1u);  // the last PUBLISHED stays 1
    CHECK(host->postbox().pending() == true);    // the newer layout is retained

    // Drain the LATEST published (the render thread's consume-once-per-frame): the
    // already-published v1 root fill is drained; the postbox is then EMPTY of newer
    // content to consume.
    std::vector<DrawItem> snapshot;
    const std::uint64_t drained = host->postbox().consume_latest(snapshot);
    CHECK(drained == 1u);
    CHECK(snapshot.size() == 1u);
    CHECK(snapshot[0].kind == DrawItemKind::SolidRect);
    CHECK(snapshot[0].w == doctest::Approx(400.0f));
    CHECK(snapshot[0].h == doctest::Approx(300.0f));

    // An UNCHANGED frame (no new invalidate, nothing newer published): the drain
    // gate is false (no re-encode) and consume_latest advances nothing.
    CHECK(host->postbox().has_newer() == false);
    CHECK(should_consume(host->postbox().has_newer()) == false);

    // Now the destination slot is free (lastConsumed_ == version_): a THIRD
    // invalidate (a real UI change) publishes the retained update as version 2.
    const std::uint64_t v3 = host->invalidate();
    CHECK(v3 == 2u);
    CHECK(host->postbox().scene_version() == 2u);
    CHECK(host->postbox().pending() == false);
}

TEST_CASE("S212: the host's DrawBody seam == the fixed S210/S211 render-loop plan")
{
    // The host reuses (not re-authors) the S210/S211 fixed per-frame loop: the
    // render-thread DrawBody runs ConsumeMarshalled -> EncodeIfDirty -> RecordRetained
    // -> Recycle, with WaitFence + SubmitPresent the Presenter's own. The plan is the
    // S210/S211 6-step loop with ConsumeMarshalled AHEAD of EncodeIfDirty.
    const auto& plan = render_loop_steps();
    REQUIRE(plan.size() == 6u);
    CHECK(plan[0] == RenderLoopStep::WaitFence);
    CHECK(plan[1] == RenderLoopStep::ConsumeMarshalled);
    CHECK(plan[2] == RenderLoopStep::EncodeIfDirty);
    CHECK(plan[3] == RenderLoopStep::RecordRetained);
    CHECK(plan[4] == RenderLoopStep::SubmitPresent);
    CHECK(plan[5] == RenderLoopStep::Recycle);
}

TEST_CASE("S212: presenter_lifetimes_ok gate is reused (the S173 no-handle gate)")
{
    // The host applies the S173 gate before create_device (a presentable vertical
    // slice needs the swapchain + command loaders + a real render pass).
    CHECK_FALSE(presenter_lifetimes_ok(false, false, VK_NULL_HANDLE));
    CHECK_FALSE(presenter_lifetimes_ok(true, true, VK_NULL_HANDLE));
    const VkRenderPass nonNullRp =
        reinterpret_cast<VkRenderPass>(static_cast<std::uintptr_t>(1));
    CHECK(presenter_lifetimes_ok(true, true, nonNullRp));
}

TEST_CASE("S212: VamlRenderer is move-only RAII (the S170 discipline)")
{
    static_assert(std::is_move_constructible<VamlRenderer>::value,
                  "VamlRenderer must be movable");
    static_assert(std::is_move_assignable<VamlRenderer>::value,
                  "VamlRenderer must be move-assignable");
    static_assert(!std::is_copy_constructible<VamlRenderer>::value,
                  "VamlRenderer must be non-copyable");
    static_assert(!std::is_copy_assignable<VamlRenderer>::value,
                  "VamlRenderer must be non-copy-assignable");
    CHECK(true);
}
