// 11n-6 — ADORNER + VISUALTREEHELPER, CPU side (the standard BOTH-LANE
// headless suite; the step's GPU/[both apps, not CI] lane — encode_adorner_tree
// in the vulkan encoder — is deliberately NOT a ctest here).
//
// WPF ground truth (read from E:\GitRepos\wpf):
//   - Media\VisualTreeHelper.cs: static GetChildrenCount(Visual),
//     GetChild(Visual,int), GetParent(DependencyObject) — all walk the visual
//     children/parent.
//   - Documents\Adorner.cs: `Adorner : FrameworkElement`, AdornedElement
//     (read-only), MeasureOverride defaults to the adorned element's
//     RenderSize, GetDesiredTransform virtual filter, IsClipEnabled default
//     false.
//   - Documents\AdornerLayer.cs: Add/Remove/Update/GetAdorners +
//     static GetAdornerLayer(Visual), _zOrderMap (DefaultZOrder =
//     Int32.MaxValue).
//
// Under test (all CPU-side, no GPU, both CI lanes):
//   (A) VisualTreeHelper.GetChildrenCount / GetChild / GetParent over REAL
//       S121 trees (create_child wiring parent + the ordered children list) —
//       the helper is a thin read-only wrapper over the LIVE tree.
//   (B) GetDescendantBounds — the S124 final_rect union over the subtree
//       (leaf -> its own rect; a parent -> the union of its children's rects;
//       zero-extent children never grow the union).
//   (C) Adorner — measure-to-adorned-size default (the WPF adorner measure
//       contract), AdornedElement non-owning binding, the GetDesiredTransform
//       passthrough default (identity fold), IsClipEnabled default false +
//       round-trip.
//   (D) AdornerLayer attach/detach — Add adopts ownership keyed by the ADORNED
//       element (not the layer's own children), duplicate-Add no-op, Remove
//       never throws on an absent adorner, GetAdorners/HasAdorners/
//       AdornerCount, Update re-syncs the adorners over the adorned element's
//       final rect, z-order (the MAX-z adorner is topmost / last).
//   (E) AdornerDecorator — the S550 Decorator-derived host that OWNS a
//       synthesized AdornerLayer + the static GetAdornerLayer ancestor walk
//       (a descendant visual reaches the nearest decorator's layer; a visual
//       with NO decorator ancestor -> nullptr).
//
// ADR-018: ZERO new locks (single-threaded walk/attach/detach). ZERO MINIMAL
// REAL — every symbol below is the permanent, forward-resolvable WPF-faithful
// surface.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Adorner.h"
#include "vaml/core/AdornerDecorator.h"
#include "vaml/core/AdornerLayer.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/VisualTreeHelper.h"
#include "vaml/core/Transform.h"   // Matrix2D — GetDesiredTransform

#include <memory>

using vaml::Adorner;
using vaml::AdornerDecorator;
using vaml::AdornerLayer;
using vaml::Matrix2D;
using vaml::Size;
using vaml::Rect;
using vaml::UIElement;
using vaml::VisualTreeHelper;

namespace {
// A concrete Adorner subclass so create/measure/transform tests exercise a real
// derived adorner (not the abstract base). This is a TEST-ONLY leaf, not a
// control seed.
struct TestAdorner final : Adorner
{
    using Adorner::Adorner;
};

// A derived adorner that overrides GetDesiredTransform (a translation offset —
// the WPF filter seam) so the test proves an adorner can adjust the offered
// transform.
struct OffsetAdorner final : Adorner
{
    using Adorner::Adorner;
    Matrix2D GetDesiredTransform(const Matrix2D& offered) const override
    {
        // Compose a translation AFTER the offered transform (an overhanging
        // handle offset).
        return vaml::multiply(vaml::Matrix2D::translation(4.0f, 4.0f), offered);
    }
};
} // namespace

TEST_CASE("11n-6 VisualTreeHelper walks the real S121 tree")
{
    // GetParent/GetChildrenCount/GetChild are thin read-only wrappers over the
    // LIVE S121 visual children list (create_child wires parent + order).
    UIElement root;
    UIElement* a = root.create_child<UIElement>();
    UIElement* b = root.create_child<UIElement>();
    CHECK(VisualTreeHelper::GetChildrenCount(&root) == 2);
    CHECK(VisualTreeHelper::GetChild(&root, 0) == a);
    CHECK(VisualTreeHelper::GetChild(&root, 1) == b);

    // GetParent returns the visual parent (VAML parent()).
    CHECK(VisualTreeHelper::GetParent(a) == &root);
    CHECK(VisualTreeHelper::GetParent(b) == &root);
    CHECK(VisualTreeHelper::GetParent(&root) == nullptr); // root has no parent

    // Out-of-range child / null element are nullptr, never a throw.
    CHECK(VisualTreeHelper::GetChild(&root, 2) == nullptr);
    CHECK(VisualTreeHelper::GetChildrenCount(nullptr) == 0);
    CHECK(VisualTreeHelper::GetChild(nullptr, 0) == nullptr);
    CHECK(VisualTreeHelper::GetParent(nullptr) == nullptr);
}

TEST_CASE("11n-6 VisualTreeHelper.GetDescendantBounds unions the S124 final_rect")
{
    // The WPF GetDescendantBounds: the union of the visual's extent over the
    // subtree. VAML = the S124 final_rect union (a leaf -> its own rect; a
    // parent -> the union of its children's rects; zero-extent children never
    // grow the union).
    UIElement leaf;
    leaf.Arrange(Rect{ 10.0f, 20.0f, 30.0f, 40.0f });
    const Rect leafBounds = VisualTreeHelper::GetDescendantBounds(&leaf);
    CHECK(leafBounds == Rect{ 10.0f, 20.0f, 30.0f, 40.0f });

    // A parent of two disjoint children: the union covers both.
    UIElement root;
    UIElement* a = root.create_child<UIElement>();
    UIElement* b = root.create_child<UIElement>();
    a->Arrange(Rect{ 0.0f, 0.0f, 10.0f, 10.0f });
    b->Arrange(Rect{ 100.0f, 50.0f, 20.0f, 30.0f });
    const Rect rootBounds = VisualTreeHelper::GetDescendantBounds(&root);
    CHECK(rootBounds == Rect{ 0.0f, 0.0f, 120.0f, 80.0f });

    // A zero-extent child (default final_rect 0) does NOT grow the union.
    UIElement root2;
    (void)root2.create_child<UIElement>(); // un-arranged -> zero rect at (0,0,0,0)
    const Rect root2Bounds = VisualTreeHelper::GetDescendantBounds(&root2);
    CHECK(root2Bounds == Rect{ 0.0f, 0.0f, 0.0f, 0.0f });
}

TEST_CASE("11n-6 Adorner measures to its adorned element's size")
{
    // The WPF adorner measure contract: an Adorner's MeasureOverride returns
    // the ADORNED element's extent (the overlay matches its decorated box).
    UIElement adorned;
    adorned.Arrange(Rect{ 5.0f, 6.0f, 100.0f, 50.0f }); // the decorated box

    TestAdorner adorner(&adorned);
    CHECK(adorner.AdornedElement() == &adorned); // non-owning binding

    adorner.Measure(Size::Infinite());
    CHECK(adorner.desired_size() == Size{ 100.0f, 50.0f });
}

TEST_CASE("11n-6 Adorner GetDesiredTransform default is the passthrough")
{
    // The WPF GetDesiredTransform seam: the DEFAULT returns the offered
    // transform unchanged (an adorner renders at the layer's offered Model).
    UIElement adorned;
    TestAdorner adorner(&adorned);

    const Matrix2D offered = Matrix2D::translation(10.0f, 20.0f);
    CHECK(adorner.GetDesiredTransform(offered) == offered);

    // A derived adorner can override the seam (an overhanging handle offset).
    OffsetAdorner offset(&adorned);
    const Matrix2D filtered = offset.GetDesiredTransform(offered);
    CHECK(filtered.dx == 10.0f + 4.0f);
    CHECK(filtered.dy == 20.0f + 4.0f);
}

TEST_CASE("11n-6 Adorner IsClipEnabled defaults false + round-trips")
{
    // WPF-faithful: an adorner is NOT clipped by default (adorners overhang).
    UIElement adorned;
    TestAdorner adorner(&adorned);
    CHECK(adorner.is_clip_enabled() == false);

    CHECK(adorner.set_is_clip_enabled(true) == vaml::SetResult::APPLIED);
    CHECK(adorner.is_clip_enabled() == true);
    CHECK(adorner.set_is_clip_enabled(false) == vaml::SetResult::APPLIED);
    CHECK(adorner.is_clip_enabled() == false);
}

TEST_CASE("11n-6 AdornerLayer.Add registers keyed by the adorned element")
{
    // The WPF ElementMap: adorners are registered under their ADORNED element,
    // NOT the layer's own children.
    UIElement adorned1;
    UIElement adorned2;

    AdornerLayer layer;
    auto a1 = std::make_unique<TestAdorner>(&adorned1);
    auto a2 = std::make_unique<TestAdorner>(&adorned2);

    // Add registers the borrowed adorner reference (the caller owns lifetime).
    CHECK(layer.Add(a1.get()) == true);
    CHECK(layer.Add(a2.get()) == true);
    CHECK(layer.AdornerCount() == 2);

    // GetAdorners is keyed by the ADORNED element.
    const std::vector<Adorner*>* of1 = layer.GetAdorners(&adorned1);
    REQUIRE(of1 != nullptr);
    CHECK(of1->size() == 1);
    CHECK((*of1)[0] == a1.get());

    const std::vector<Adorner*>* of2 = layer.GetAdorners(&adorned2);
    REQUIRE(of2 != nullptr);
    CHECK((*of2)[0] == a2.get());

    // An adorned element with no adorner -> nullptr (WPF GetAdorners null).
    UIElement noAdorner;
    CHECK(layer.GetAdorners(&noAdorner) == nullptr);
    CHECK(layer.HasAdorners(&adorned1) == true);
    CHECK(layer.HasAdorners(&noAdorner) == false);
}

TEST_CASE("11n-6 AdornerLayer duplicate Add is a no-op + Remove of absent is silent")
{
    UIElement adorned;
    AdornerLayer layer;
    auto a = std::make_unique<TestAdorner>(&adorned);

    CHECK(layer.Add(a.get()) == true);
    // Duplicate-Add: single-owner discipline -> no-op (returns false).
    CHECK(layer.Add(a.get()) == false);
    CHECK(layer.AdornerCount() == 1);

    // Remove of a NON-present adorner NEVER throws (WPF "always returns
    // normally") and returns false.
    UIElement other;
    auto absent = std::make_unique<TestAdorner>(&other);
    CHECK(layer.Remove(absent.get()) == false);
    CHECK(layer.AdornerCount() == 1);

    // Removing a present adorner works + the key drops when empty.
    CHECK(layer.Remove(a.get()) == true);
    CHECK(layer.AdornerCount() == 0);
    CHECK(layer.GetAdorners(&adorned) == nullptr);
}

TEST_CASE("11n-6 AdornerLayer z-order - the MAX-z adorner is topmost")
{
    // The WPF _zOrderMap: an adorner renders TOP when its z is the MAX among
    // its adorned element's adorners. The layer keeps the list z-ascending
    // (the topmost adorner comes last).
    UIElement adorned;
    AdornerLayer layer;

    auto low  = std::make_unique<TestAdorner>(&adorned);
    auto high = std::make_unique<TestAdorner>(&adorned);

    CHECK(layer.Add(low.get(), 5) == true);
    CHECK(layer.Add(high.get(), 100) == true);

    const std::vector<Adorner*>* list = layer.GetAdorners(&adorned);
    REQUIRE(list != nullptr);
    CHECK(list->size() == 2);
    // z-ascending: low (5) first, high (100) last/topmost.
    CHECK((*list)[0] == low.get());
    CHECK((*list)[1] == high.get());

    // Attach order (equal z, DefaultZOrder) is preserved (stable).
    auto d1 = std::make_unique<TestAdorner>(&adorned);
    auto d2 = std::make_unique<TestAdorner>(&adorned);
    CHECK(layer.Add(d1.get()) == true);
    CHECK(layer.Add(d2.get()) == true);
    const std::vector<Adorner*>* all = layer.GetAdorners(&adorned);
    REQUIRE(all != nullptr);
    // ... d1 before d2 in attach order (both DefaultZOrder, after the two
    // explicit-z adorners which sort lower).
    CHECK(all->back() == d2.get());
}

TEST_CASE("11n-6 AdornerLayer.Update re-syncs adorners over the adorned final_rect")
{
    // The WPF "Update called when the adorned element repaints": re-measure +
    // re-arrange each hosted adorner into its adorned element's box.
    UIElement adorned;
    adorned.Arrange(Rect{ 0.0f, 0.0f, 200.0f, 60.0f });

    AdornerLayer layer;
    auto a = std::make_unique<TestAdorner>(&adorned);
    CHECK(layer.Add(a.get()) == true);

    // The adorner is not yet measured/arranged by the layer.
    CHECK(a->desired_size() == Size{ 0.0f, 0.0f });

    // Update() re-syncs the whole layer; Update(adorned) only that element's.
    const std::size_t syncedAll = layer.Update();
    CHECK(syncedAll == 1);
    CHECK(a->desired_size() == Size{ 200.0f, 60.0f });
    CHECK(a->final_rect() == Rect{ 0.0f, 0.0f, 200.0f, 60.0f });

    // An adorned element with no adorner updates nothing.
    UIElement other;
    CHECK(layer.Update(&other) == 0);
}

TEST_CASE("11n-6 AdornerDecorator owns a layer + GetAdornerLayer ancestor walk")
{
    // AdornerDecorator : Decorator (the S550 single-content base) OWNS a
    // synthesized AdornerLayer; the static GetAdornerLayer walks UP to the
    // nearest decorator.
    AdornerDecorator deco;
    CHECK(deco.Layer() != nullptr); // the synthesized overlay surface

    // A descendant visual reaches the NEAREST decorator's layer via the walk.
    // The decorator hosts ONE content child via the S550 Decorator content slot
    // (set_content — which wires the visual parent chain the walk uses).
    auto host_wrap = std::make_unique<UIElement>();
    UIElement* content = host_wrap.get();
    UIElement* grandchild = content->create_child<UIElement>();
    deco.set_content(std::move(host_wrap));

    CHECK(AdornerDecorator::GetAdornerLayer(grandchild) == deco.Layer());
    CHECK(AdornerDecorator::GetAdornerLayer(content) == deco.Layer());
    CHECK(AdornerDecorator::GetAdornerLayer(&deco) == deco.Layer());

    // A visual with NO decorator ancestor -> nullptr (WPF returns null).
    UIElement orphan;
    (void)orphan.create_child<UIElement>();
    CHECK(AdornerDecorator::GetAdornerLayer(&orphan) == nullptr);

    // The decorator's Arrange drives the content + the layer: arrange the
    // decorator, then the layer re-syncs an adorner over the content.
    auto a = std::make_unique<TestAdorner>(content);
    CHECK(deco.Layer()->Add(a.get()) == true);

    deco.Arrange(Rect{ 0.0f, 0.0f, 120.0f, 30.0f });
    CHECK(a->final_rect() == Rect{ 0.0f, 0.0f, 120.0f, 30.0f });
}
