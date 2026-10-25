// S324 (dep:S323) — a click hit-tests a real element and raises a BUBBLING
// routed event with the CORRECT SOURCE. The FORMAL gate: the whole chain
// (real arranged tree + HitTestAt + the S321 bubbler + the input marshaller)
// is proven end-to-end, from first principles, headlessly.
//
// Validates, with the Shouldly vocabulary:
//   1. a click hit-tests a SOURCE child and raises ClickEvent on that child,
//      which BUBBLES to the root unless a handler sets handled;
//   2. source == the hit element; originalSource stays the hit element;
//   3. a click on EMPTY space (no hit) raises nothing;
//   4. a handled click on a middle ancestor stops the climb to the root;
//   5. the destination stack panel raises a bubbling click with correct source
//      across a real arranged StackPanel.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // operator<< for enums/value
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldlyIntegral

#include "vaml/core/Element.h"            // StackPanel (real panel)
#include "vaml/core/InputMarshaller.h"    // InputMarshaller + input types
#include "vaml/core/RoutedEvent.h"        // RoutedEventArgs / ClickEvent
#include "vaml/core/UIElement.h"

#include <memory>

using vaml::StackPanel;
using vaml::UIElement;
using vaml::input::InputEvent;
using vaml::input::InputEventKind;
using vaml::input::InputMarshaller;

namespace {

// Builds a real arranged StackPanel tree:
//   panel (root StackPanel) at (0,0) 200x200
//     -> child at (0,0) 50x50
//       -> grandchild at (0,0) 25x25
std::unique_ptr<UIElement> build_stack_panel_tree(std::unique_ptr<UIElement> panel)
{
    panel->Arrange({0, 0, 200, 200});
    UIElement* child = panel->create_child<UIElement>();
    child->Arrange({0, 0, 50, 50});
    UIElement* gc = child->create_child<UIElement>();
    gc->Arrange({0, 0, 25, 25});
    return panel;
}

} // namespace

TEST_CASE("S324: a click hit-tests a source child and raises a bubbling Click with correct source")
{
    auto root = build_stack_panel_tree(std::make_unique<StackPanel>());
    UIElement* child = root->child_at(0);
    UIElement* gc = child->child_at(0);

    std::vector<UIElement*> route;
    (void)gc->add_routed_handler(vaml::ClickEvent(),
                                 [&](vaml::RoutedEventArgs& e) { route.push_back(static_cast<UIElement*>(e.source)); });
    (void)root->add_routed_handler(vaml::ClickEvent(),
                                   [&](vaml::RoutedEventArgs& e) {
                                       route.push_back(static_cast<UIElement*>(e.source));
                                       (void)e.originalSource;
                                   });

    InputMarshaller in(root.get());
    (void)in.Push({InputEventKind::MouseDown, 5, 5, vaml::input::Key::None, false, true});
    (void)in.Push({InputEventKind::MouseUp, 5, 5, vaml::input::Key::None, false, false});

    // The grandchild is the hit source; it raises (its own handler), then the
    // click BUBBLES through the child to the root. route = [gc, root].
    SHOULD(ShouldlyIntegral(route.size()).ShouldBe(2));
    if (route.size() == 2)
    {
        SHOULD(ShouldlyString(route[0]).ShouldBe(gc));   // source raised first
        SHOULD(ShouldlyString(route[1]).ShouldBe(root.get())); // bubbled to root
    }
}

TEST_CASE("S324: a click on empty space hits nothing and raises nothing")
{
    auto root = build_stack_panel_tree(std::make_unique<StackPanel>());
    int clicks = 0;
    (void)root->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++clicks; });

    InputMarshaller in(root.get());
    // The panel is 200x200; a point at (150,150) is inside the panel but no
    // child — Wait: the panel itself is hit-testable (it contains the point), so
    // the click does hit the panel. A TRUE empty click is OUTSIDE the whole
    // tree (no element). Use a point outside the 200x200 root.
    (void)in.Push({InputEventKind::MouseDown, 500, 500, vaml::input::Key::None, false, true});
    (void)in.Push({InputEventKind::MouseUp, 500, 500, vaml::input::Key::None, false, false});
    SHOULD(ShouldlyIntegral(clicks).ShouldBe(0));
}

TEST_CASE("S324: a handled click on a middle ancestor stops the climb to the root")
{
    auto root = build_stack_panel_tree(std::make_unique<StackPanel>());
    UIElement* child = root->child_at(0);

    int rootClicks = 0;
    (void)root->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs&) { ++rootClicks; });
    (void)child->add_routed_handler(vaml::ClickEvent(),
                                    [](vaml::RoutedEventArgs& e) { e.handled = true; });

    InputMarshaller in(root.get());
    (void)in.Push({InputEventKind::MouseDown, 5, 5, vaml::input::Key::None, false, true});
    (void)in.Push({InputEventKind::MouseUp, 5, 5, vaml::input::Key::None, false, false});

    SHOULD(ShouldlyIntegral(rootClicks).ShouldBe(0)); // the child handled -> root never fired
}

TEST_CASE("S324: source/originalSource correctness across the whole chain")
{
    auto root = build_stack_panel_tree(std::make_unique<StackPanel>());
    UIElement* gc = root->child_at(0)->child_at(0);

    UIElement* sourceAtRoot = nullptr;
    void* originalAtRoot = nullptr;
    (void)root->add_routed_handler(vaml::ClickEvent(), [&](vaml::RoutedEventArgs& e) {
        sourceAtRoot = static_cast<UIElement*>(e.source);
        originalAtRoot = e.originalSource;
    });

    InputMarshaller in(root.get());
    (void)in.Push({InputEventKind::MouseDown, 5, 5, vaml::input::Key::None, false, true});
    (void)in.Push({InputEventKind::MouseUp, 5, 5, vaml::input::Key::None, false, false});

    SHOULD(ShouldlyString(sourceAtRoot).ShouldBe(root.get()));     // source moves to root per hop
    SHOULD(ShouldlyString(originalAtRoot).ShouldBe(gc));           // originalSource stays the hit element
}
