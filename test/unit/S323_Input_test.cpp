// S323 (dep:S322) — Mouse/keyboard input -> routed events via hit-testing.
// Two parts, both headless/core (GLFW-free, ADR-011):
//   A. UIElement::HitTestAt — the deepest-topmost hit-test over the real
//      final_rect layout tree: reverse-z (last child on top), deepest wins,
//      Collapsed/Hidden not hit-testable, child-clipped-by-ancestor not hit,
//      empty/off-point -> nullptr.
//   B. InputMarshaller — the library-level input -> routed-event state machine:
//      press+release on the SAME element -> a bubbling ClickEvent with the
//      correct source (the S321 engine); press-move-release (drag) -> NO click;
//      a release on empty space -> no click; a key Down/Up raises bubbling key
//      events on the focused element.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for Value/enums/Rect
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral

#include "vaml/core/Element.h"            // StackPanel (real layout tree)
#include "vaml/core/InputMarshaller.h"    // InputMarshaller + input types
#include "vaml/core/RoutedEvent.h"        // RoutedEventArgs / ClickEvent
#include "vaml/core/UIElement.h"

#include <memory>

using vaml::StackPanel;
using vaml::UIElement;
using vaml::Visibility;
using vaml::input::InputEvent;
using vaml::input::InputEventKind;
using vaml::input::InputMarshaller;
using vaml::input::Key;

namespace {

// Builds a root with two SEQUENTIAL children at known final_rects:
//   root  : (0,0) 400x300
//   a     : (0,0) 100x100   (bottom, index 0)
//   b     : (200,150) 100x100 (top, index 1 — LATER child = higher z)
std::unique_ptr<UIElement> build_layout_tree(std::size_t count = 2)
{
    auto root = std::make_unique<UIElement>();
    root->Arrange({0, 0, 400, 300});
    for (std::size_t i = 0; i < count; ++i)
    {
        UIElement* c = root->create_child<UIElement>();
        // Arrange each child into a distinct rect (a at 0,0; b at 200,150 if count 2).
        const float x = (i == 0) ? 0.0f : 200.0f;
        const float y = (i == 0) ? 0.0f : 150.0f;
        c->Arrange({x, y, 100, 100});
    }
    return root;
}

} // namespace

// ---------------------------------------------------------------------------
// A. HitTestAt
// ---------------------------------------------------------------------------

TEST_CASE("S323 hit-test: deepest element containing a point, topmost-z wins")
{
    auto root = build_layout_tree();
    UIElement* a = root->child_at(0);
    UIElement* b = root->child_at(1);

    // Point inside a only -> a.
    SHOULD(ShouldlyString(root->HitTestAt(10, 10)).ShouldBe(a));
    // Point inside b only -> b.
    SHOULD(ShouldlyString(root->HitTestAt(210, 160)).ShouldBe(b));
    // Point inside root but NO child -> root.
    SHOULD(ShouldlyString(root->HitTestAt(350, 250)).ShouldBe(root.get()));
    // Point outside root -> nullptr.
    SHOULD(ShouldlyString(root->HitTestAt(-10, -10)).ShouldBe(nullptr));
    SHOULD(ShouldlyString(root->HitTestAt(500, 500)).ShouldBe(nullptr));
}

TEST_CASE("S323 hit-test: reverse-z — the LATER (top) child wins an overlap")
{
    // Two overlapping children: a at (0,0) 100x100, b at (0,0) 200x200 (b on top,
    // later index). The shared region (50,50) must hit b (topmost), not a.
    auto root = std::make_unique<UIElement>();
    root->Arrange({0, 0, 300, 300});
    UIElement* a = root->create_child<UIElement>();
    UIElement* b = root->create_child<UIElement>(); // later = top z
    a->Arrange({0, 0, 100, 100});
    b->Arrange({0, 0, 200, 200});

    SHOULD(ShouldlyString(root->HitTestAt(50, 50)).ShouldBe(b));   // b on top
    SHOULD(ShouldlyString(root->HitTestAt(150, 150)).ShouldBe(b)); // inside b, outside a
    SHOULD(ShouldlyString(root->HitTestAt(5, 5)).ShouldBe(b));     // inside both -> b
}

TEST_CASE("S323 hit-test: Collapsed / Hidden elements are not hit-testable")
{
    auto root = std::make_unique<UIElement>();
    root->Arrange({0, 0, 300, 300});
    UIElement* hidden = root->create_child<UIElement>();
    hidden->Arrange({0, 0, 100, 100});
    (void)hidden->set_visibility(Visibility::Hidden);
    SHOULD(ShouldlyString(root->HitTestAt(10, 10)).ShouldBe(root.get())); // skips Hidden child

    UIElement* collapsed = root->create_child<UIElement>();
    collapsed->Arrange({0, 0, 100, 100});
    (void)collapsed->set_visibility(Visibility::Collapsed);
    SHOULD(ShouldlyString(root->HitTestAt(110, 110)).ShouldBe(root.get())); // skips Collapsed
}

TEST_CASE("S323 hit-test: a child clipped by an ancestor's final_rect is not hit")
{
    // child c is HUGE but its parent p is small; the part outside p's final_rect
    // must not hit (clip). A point inside c's rect but outside p's rect -> p's
    // sibling (or root), NOT c.
    auto root = std::make_unique<UIElement>();
    root->Arrange({0, 0, 400, 400});
    UIElement* p = root->create_child<UIElement>();
    p->Arrange({0, 0, 100, 100});
    UIElement* c = p->create_child<UIElement>();
    c->Arrange({0, 0, 300, 300}); // extends past p's 100x100

    // Inside both (p and c) -> c (deepest).
    SHOULD(ShouldlyString(root->HitTestAt(50, 50)).ShouldBe(c));
    // Inside c's rect but OUTSIDE p's rect (p clips c's overflow) -> root
    // (c's overflow is clipped; p is not hit because the point is outside p too).
    SHOULD(ShouldlyString(root->HitTestAt(150, 150)).ShouldBe(root.get()));
}

TEST_CASE("S323 hit-test: empty tree / off-point / const overload")
{
    auto root = std::make_unique<UIElement>();
    // A root with no Arrange (final_rect default 0) hit-tests a point nowhere.
    SHOULD(ShouldlyString(root->HitTestAt(0, 0)).ShouldBe(nullptr));
    const UIElement* croot = root.get();
    SHOULD(ShouldlyString(croot->HitTestAt(10, 10)).ShouldBe(nullptr));
}

// ---------------------------------------------------------------------------
// B. InputMarshaller
// ---------------------------------------------------------------------------

TEST_CASE("S323 marshaller: press+release on the same element raises a bubbling Click on it")
{
    auto root = build_layout_tree();
    UIElement* a = root->child_at(0); // at (0,0) 100x100
    InputMarshaller in(root.get());

    // Capture the Click on the marshaller root: wire a handler that records the
    // raised source via the S321 route.
    UIElement* clicked = nullptr;
    int clicks = 0;
    (void)a->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs& e) {
        ++clicks;
        clicked = static_cast<UIElement*>(e.source);
    });

    // Press + release inside a at the same point.
    (void)in.Push({InputEventKind::MouseDown, 10, 10, Key::None, false, true});
    (void)in.Push({InputEventKind::MouseUp, 10, 10, Key::None, false, false});
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(1));
    SHOULD(ShouldlyString(clicked).ShouldBe(a)); // source = the hit element
}

TEST_CASE("S323 marshaller: a release over empty space / a drag does NOT raise a click")
{
    auto root = build_layout_tree();
    InputMarshaller in(root.get());
    int clicks = 0;
    (void)root->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++clicks; });

    // Press inside a, release over EMPTY (root area not a) -> no click.
    (void)in.Push({InputEventKind::MouseDown, 10, 10, Key::None, false, true});
    (void)in.Push({InputEventKind::MouseUp, 350, 250, Key::None, false, false});
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(0));

    // A DRAG (move beyond tolerance while down) -> no click.
    (void)in.Push({InputEventKind::MouseDown, 10, 10, Key::None, false, true});
    (void)in.Push({InputEventKind::MouseMove, 60, 60, Key::None, false, true});  // moved > 4px
    (void)in.Push({InputEventKind::MouseUp, 60, 60, Key::None, false, false});
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(0));
}

TEST_CASE("S323 marshaller: press+release on a CHILD bubbles to the parent (the S321 route through input)")
{
    auto root = std::make_unique<UIElement>();
    root->Arrange({0, 0, 200, 200});
    UIElement* panel = root->create_child<UIElement>();
    panel->Arrange({0, 0, 200, 200});
    UIElement* child = panel->create_child<UIElement>();
    child->Arrange({0, 0, 50, 50});

    int parentClicks = 0;
    (void)panel->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++parentClicks; });

    InputMarshaller in(root.get());
    (void)in.Push({InputEventKind::MouseDown, 10, 10, Key::None, false, true});
    (void)in.Push({InputEventKind::MouseUp, 10, 10, Key::None, false, false});

    // The click on child bubbles: even without a child handler, the parent's
    // handler fires (the S321 parent-chain route driving through input).
    SHOULD(ShouldlyIntegral(parentClicks).ShouldBe(1));
}

TEST_CASE("S323 marshaller: KeyDown/KeyUp raise bubbling routed key events on the focused element")
{
    auto root = std::make_unique<UIElement>();
    UIElement* leaf = root->create_child<UIElement>();
    InputMarshaller in(root.get());
    in.set_focused(leaf);

    int downs = 0;
    int ups = 0;
    (void)leaf->add_routed_handler(vaml::KeyDownEvent(), [&](vaml::RoutedEventArgs&) { ++downs; });
    (void)leaf->add_routed_handler(vaml::KeyUpEvent(), [&](vaml::RoutedEventArgs&) { ++ups; });

    (void)in.Push({InputEventKind::KeyDown, 0, 0, Key::Enter, true, false});
    (void)in.Push({InputEventKind::KeyUp, 0, 0, Key::Enter, false, false});
    SHOULD(ShouldlyIntegral(downs).ShouldBe(1));
    SHOULD(ShouldlyIntegral(ups).ShouldBe(1));
}
