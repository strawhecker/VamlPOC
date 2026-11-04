// S595a (dep:S595) — `UIElement.IsHitTestVisible` PER-ELEMENT INPUT HIT-TEST
// TRANSPARENCY (headless Rail 1, vaml-core-tests). WPF-faithful
// (UIElement.IsHitTestVisible / HitTestCore): an element with this false still
// RENDERS but does NOT intercept input (the SAME skip axis WPF uses for
// Visibility, but DECOUPLED — a "100% transparent" overlay can pass clicks
// through, and toggle to capture them). The skip is in UIElement::HitTestAt
// (const), a pure tree-walk read; registration is the idempotent Meyers-static
// first-ctor guard. ZERO new locks (ADR-018). (CORE surfaces only — the
// ScreenCaptureOverlay viewer-growth proof lives in the vaml-presentation-tests
// TU ViewerHitTest_s595a_pres_test.cpp.)

#include <vaml/core/UIElement.h>
#include <vaml/core/Element.h>     // StackPanel (a real UIElement + a content leaf)
#include <vaml/core/VamlReader.h>  // corpus-shaped authoring proof (Bool DP)

#include "doctest/doctest.h"

#include <memory>

using namespace vaml;

namespace {

// A content-bearing leaf so a hand-built tree has a NON-degenerate final_rect
// (the S593 alpha-test precedent: test-local, not a control seed).
class Leaf : public UIElement
{
public:
    explicit Leaf(float w, float h) : extent_{ w, h } {}
protected:
    vaml::Size MeasureOverride(const vaml::Size& /*available*/) const override { return extent_; }
private:
    vaml::Size extent_;
};

} // namespace

TEST_CASE("S595a default: a fresh UIElement is_hit_test_visible()==true")
{
    UIElement e;
    CHECK(e.is_hit_test_visible() == true);
}

TEST_CASE("S595a round-trip: typed set (false->true->false) + the Set/Get path")
{
    UIElement e;
    (void)e.set_is_hit_test_visible(false);
    CHECK(e.is_hit_test_visible() == false);
    (void)e.set_is_hit_test_visible(true);
    CHECK(e.is_hit_test_visible() == true);
    (void)e.set_is_hit_test_visible(false);
    CHECK(e.is_hit_test_visible() == false);

    // The standard Set/Get path on PropertyId("IsHitTestVisible") agrees (Bool
    // payload).
    (void)e.Set(vaml::PropertyId::of("IsHitTestVisible"), vaml::Value::Bool(true));
    CHECK(e.Get(vaml::PropertyId::of("IsHitTestVisible")).get<bool>() == true);
    CHECK(e.is_hit_test_visible() == true);
}

TEST_CASE("S595a HEADLINE: Visible + IsHitTestVisible=false SKIPPED for hit-test while still RENDERED")
{
    // Hand-build a real small visual tree: root + a child at a known final_rect.
    vaml::StackPanel root;
    Leaf* child = root.create_child<Leaf>(100.0f, 40.0f);
    root.Measure(vaml::Size{ 400.0f, 200.0f });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 200.0f });

    // The child is Visible (still renderable) but IsHitTestVisible=false.
    CHECK(child->visibility() == vaml::Visibility::Visible);
    (void)child->set_is_hit_test_visible(false);
    CHECK(child->is_hit_test_visible() == false);

    // HitTestAt at the child's coords returns the ROOT (the child is SKIPPED),
    // NOT the child.
    const float cx = child->final_rect().x + child->final_rect().width / 2.0f;
    const float cy = child->final_rect().y + child->final_rect().height / 2.0f;
    UIElement* hit = root.HitTestAt(cx, cy);
    CHECK(hit == &root);

    // Flip the child back to hit-testable -> HitTestAt now returns the child.
    (void)child->set_is_hit_test_visible(true);
    UIElement* hit2 = root.HitTestAt(cx, cy);
    CHECK(hit2 == child);
}

TEST_CASE("S595a decoupling from Visibility: the two skip axes are independent")
{
    vaml::StackPanel root;
    Leaf* child = root.create_child<Leaf>(100.0f, 40.0f);
    root.Measure(vaml::Size{ 400.0f, 200.0f });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 200.0f });
    const float cx = child->final_rect().x + 10.0f;
    const float cy = child->final_rect().y + 10.0f;

    // (a) IsHitTestVisible=true + Visibility::Collapsed -> still skipped (the S323 axis).
    (void)child->set_is_hit_test_visible(true);
    (void)child->set_visibility(vaml::Visibility::Collapsed);
    CHECK(root.HitTestAt(cx, cy) == &root);

    // (b) IsHitTestVisible=false + Visibility::Visible -> skipped (the S595a axis).
    (void)child->set_visibility(vaml::Visibility::Visible);
    (void)child->set_is_hit_test_visible(false);
    CHECK(child->is_hit_test_visible() == false);
    CHECK(root.HitTestAt(cx, cy) == &root);
}

TEST_CASE("S595a authoring parity: corpus-shaped doc authors the Bool DP IsHitTestVisible")
{
    // A corpus-shaped doc authors `IsHitTestVisible="false"` on a leaf; the Bool DP
    // resolves through the reader (the AllowsTransparency/Topmost typed-DP path).
    const char* doc = R"(
<StackPanel xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml">
  <StackPanel IsHitTestVisible="false"/>
</StackPanel>
)";
    vaml::VamlReader reader;
    std::unique_ptr<vaml::DependencyObject> root = reader.Load(doc);
    REQUIRE(root != nullptr);
    // The S120/S121 hand-off that TRANSFERS the declared child into its parent's
    // visual-children list (so the nested panel actually has its child).
    (void)reader.finalize_visual_tree();
    auto* panelRoot = dynamic_cast<vaml::StackPanel*>(root.get());
    REQUIRE(panelRoot != nullptr);
    REQUIRE(panelRoot->child_count() == 1u);
    auto* child = dynamic_cast<vaml::UIElement*>(panelRoot->child_at(0));
    REQUIRE(child != nullptr);
    CHECK(child->is_hit_test_visible() == false);
}

TEST_CASE("S595a NOT inherited: a parent IsHitTestVisible=false does NOT disable its children")
{
    // The property is NOT inherited: a parent with IsHitTestVisible=false still
    // lets its children evaluate their OWN value (each child is independent).
    vaml::StackPanel root;
    (void)root.set_is_hit_test_visible(false);
    Leaf* child = root.create_child<Leaf>(100.0f, 40.0f);
    root.Measure(vaml::Size{ 400.0f, 200.0f });
    root.Arrange(vaml::Rect{ 0.0f, 0.0f, 400.0f, 200.0f });

    // The child (not set) is still hit-test-visible default true.
    CHECK(root.is_hit_test_visible() == false);
    CHECK(child->is_hit_test_visible() == true);

    // A hit at the child's coords: the ROOT is skipped (its own flag false), so
    // the walk descends to the child which IS hit-testable -> the child wins.
    const float cx = child->final_rect().x + 5.0f;
    const float cy = child->final_rect().y + 5.0f;
    UIElement* hit = root.HitTestAt(cx, cy);
    CHECK(hit == child);
}
