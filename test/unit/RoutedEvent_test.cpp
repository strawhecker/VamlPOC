// S320 — Signal<T> + RoutedEvent primitive + the route-policy hook.
// S321 — Bubbling + tunneling + handled over the REAL parent chain.
//   (Sprint 8; S320 dep:S292, S321 dep:S320.)
//
// S320 proves the ADR-005 Signal<T> primitive: token-based reentrancy-safe
// add/remove, registration-order dispatch, move-only RAII, and the
// RoutedEvent identity + strategy registry (the WPF RegisterRoutedEvent
// analogue) + the route-policy hook shape.
//
// S321 proves the routed-event ENGINE over the REAL parent chain
// (DependencyObject::parent / the S121 visual tree): bubbling child -> root
// in tree order, `handled` stopping the bubble (a mid-ancestor handler + a
// child handler's stop), tunneling (preview) root -> child order, a tunnel-
// set `handled` stopping before the source, source/originalSource correctness,
// and the ROUTE IS THE REAL PARENT CHAIN (a panel's child click bubbles
// through its parent — no separate structure).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/RoutedEvent.h"
#include "vaml/core/Signal.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Element.h"      // StackPanel (the real-panel route proof)

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

using vaml::RouteEventDirection;
using vaml::RoutedEventArgs;
using vaml::RoutedEvent;
using vaml::UIElement;

// ===========================================================================
// S320 — Signal<T>
// ===========================================================================

TEST_CASE("S320 Signal<T>: add returns a token; fire invokes handlers in registration order")
{
    vaml::Signal<void(int)> sig;
    std::vector<int> order;
    (void)sig.add([&order](int v) { order.push_back(v * 1); });
    (void)sig.add([&order](int v) { order.push_back(v * 2); });
    (void)sig.add([&order](int v) { order.push_back(v * 3); });

    sig.fire(5);
    SHOULD(ShouldlyIntegral(order.size()).ShouldBe(3));
    SHOULD(ShouldlyIntegral(order[0]).ShouldBe(5));
    SHOULD(ShouldlyIntegral(order[1]).ShouldBe(10));
    SHOULD(ShouldlyIntegral(order[2]).ShouldBe(15));
}

TEST_CASE("S320 Signal<T>: remove(token) removes exactly that handler; unknown token no-op")
{
    vaml::Signal<void(int)> sig;
    int calls = 0;
    auto t1 = sig.add([&calls](int) { ++calls; });
    auto t2 = sig.add([&calls](int) { ++calls; });
    auto t3 = sig.add([&calls](int) { ++calls; });
    (void)t1; (void)t3;

    (void)sig.remove(t2);
    sig.fire(1);
    SHOULD(ShouldlyIntegral(calls).ShouldBe(2)); // t1 + t3
    // Removing an already-removed token is a no-op.
    SHOULD(ShouldlyIntegral(sig.remove(t2)).ShouldBe(false));
    // Unknown token no-op.
    SHOULD(ShouldlyIntegral(sig.remove(9999)).ShouldBe(false));
}

TEST_CASE("S320 Signal<T>: reentrancy — a handler removing a LATER handler mid-fire is safe")
{
    // h1 removes h2 mid-fire; both are in the fire's snapshot.
    vaml::Signal<void(int)> s2;
    int b2 = 0;
    auto tokB = s2.add([&b2](int) { ++b2; });                   // B (counts)
    (void)s2.add([&s2, tokB](int) { (void)s2.remove(tokB); });  // A (removes B mid-fire)

    s2.fire(1);
    // Snapshot [A, B] — A removed B from the LIVE list, but B STILL fired this
    // fire (snapshot): b2 == 1; the live list is now [A] (count 1).
    SHOULD(ShouldlyIntegral(b2).ShouldBe(1));
    SHOULD(ShouldlyIntegral(s2.count()).ShouldBe(1));

    s2.fire(2);
    // Second fire snapshot is [A] only: B is gone, so it does NOT fire again.
    SHOULD(ShouldlyIntegral(b2).ShouldBe(1));
    SHOULD(ShouldlyIntegral(s2.count()).ShouldBe(1));
}

TEST_CASE("S320 Signal<T>: add-during-fire is deferred to the NEXT fire (deterministic)")
{
    vaml::Signal<void()> sig;
    int fired = 0;
    (void)sig.add([&sig, &fired]() {
        ++fired;
        if (fired == 1)
        {
            (void)sig.add([&fired]() { ++fired; }); // added mid-fire
        }
    });

    sig.fire();
    SHOULD(ShouldlyIntegral(fired).ShouldBe(1)); // only the original fired now
    sig.fire();
    SHOULD(ShouldlyIntegral(fired).ShouldBe(3)); // original + the added one
}

TEST_CASE("S320 Signal<T>: empty count/introspection + null handler ignored")
{
    vaml::Signal<void()> sig;
    SHOULD(ShouldlyIntegral(sig.empty()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sig.count()).ShouldBe(0));
    vaml::Signal<void()>::Handler nullHandler;
    auto tok = sig.add(std::move(nullHandler));
    SHOULD(ShouldlyIntegral(tok).ShouldBe(0));      // null ignored, no token
    SHOULD(ShouldlyIntegral(sig.empty()).ShouldBe(true));
    sig.fire(); // safe no-op
}

// ===========================================================================
// S320 — RoutedEvent primitive (identity + strategy)
// ===========================================================================

TEST_CASE("S320 RoutedEvent: registered name + strategy identity, idempotent, conflict throws")
{
    const RoutedEvent& click = RoutedEvent::registered("TestClickS320", RouteEventDirection::Bubble);
    SHOULD(ShouldlyString(click.name()).ShouldBe("TestClickS320"));
    SHOULD(ShouldlyIntegral(static_cast<int>(click.direction()))
               .ShouldBe(static_cast<int>(RouteEventDirection::Bubble)));

    // Idempotent: same name + same strategy -> same event.
    const RoutedEvent& click2 = RoutedEvent::registered("TestClickS320", RouteEventDirection::Bubble);
    SHOULD(ShouldlyString(&click).ShouldBe(&click2));

    // Conflict: same name, different strategy -> throws.
    SHOULD(vaml::test::ShouldThrow<std::logic_error>(
        [] { (void)RoutedEvent::registered("TestClickS320", RouteEventDirection::Tunnel); }));
}

TEST_CASE("S320 RoutedEvent: the Click / KeyDown / KeyUp statics are registered bubbling")
{
    SHOULD(ShouldlyString(vaml::ClickEvent().name()).ShouldBe("Click"));
    SHOULD(ShouldlyIntegral(static_cast<int>(vaml::ClickEvent().direction()))
               .ShouldBe(static_cast<int>(RouteEventDirection::Bubble)));
    SHOULD(ShouldlyString(vaml::KeyDownEvent().name()).ShouldBe("KeyDown"));
    SHOULD(ShouldlyString(vaml::KeyUpEvent().name()).ShouldBe("KeyUp"));
}

// ===========================================================================
// S321 — the routed-event engine over the REAL parent chain
// ===========================================================================

namespace {

// A helper that builds a small REAL visual tree: root -> {childA, childB}
// where childB -> grandchild. Returns the root; the caller owns it.
std::unique_ptr<UIElement> build_chain()
{
    auto root = std::make_unique<UIElement>();
    auto a = root->create_child<UIElement>();
    auto b = root->create_child<UIElement>();
    auto gc = b->create_child<UIElement>();
    (void)a; (void)gc;
    return root;
}

} // namespace

TEST_CASE("S321 bubbling: a child-raised Click bubbles to the root in tree order")
{
    auto root = build_chain();
    UIElement* a = root->child_at(0);
    UIElement* b = root->child_at(1);
    UIElement* gc = b->child_at(0);

    std::vector<UIElement*> visited;
    (void)gc->add_routed_handler(vaml::ClickEvent(),
                                 [&](RoutedEventArgs&) { visited.push_back(gc); });
    (void)a->add_routed_handler(vaml::ClickEvent(),
                                [&](RoutedEventArgs&) { visited.push_back(a); });
    (void)b->add_routed_handler(vaml::ClickEvent(),
                                [&](RoutedEventArgs&) { visited.push_back(b); });
    (void)root->add_routed_handler(vaml::ClickEvent(),
                                   [&](RoutedEventArgs&) { visited.push_back(root.get()); });

    // Raise a click on the grandchild: it bubbles gc -> b -> root.
    RoutedEventArgs args;
    args.source = gc;
    gc->RaiseEvent(vaml::ClickEvent(), args);

    SHOULD(ShouldlyIntegral(visited.size()).ShouldBe(3));
    SHOULD(ShouldlyString(visited[0]).ShouldBe(gc));      // source itself (its handlers fired)
    SHOULD(ShouldlyString(visited[1]).ShouldBe(b));
    SHOULD(ShouldlyString(visited[2]).ShouldBe(root.get()));
}

TEST_CASE("S321 bubbling: a handler setting handled=true stops the bubble (root NOT fired)")
{
    auto root = build_chain();
    UIElement* b = root->child_at(1);
    UIElement* gc = b->child_at(0);

    bool rootFired = false;
    (void)root->add_routed_handler(vaml::ClickEvent(),
                                   [&rootFired](RoutedEventArgs&) { rootFired = true; });
    // b handles + stops the click.
    (void)b->add_routed_handler(vaml::ClickEvent(),
                                [](RoutedEventArgs& a) { a.handled = true; });

    RoutedEventArgs args;
    args.source = gc;
    gc->RaiseEvent(vaml::ClickEvent(), args);

    SHOULD(ShouldlyIntegral(rootFired).ShouldBe(false)); // the bubble stopped at b
    SHOULD(ShouldlyIntegral(args.handled).ShouldBe(true));
}

TEST_CASE("S321 bubbling: a mid-ancestor handler + registration order (first handler of an element stops the second)")
{
    auto root = std::make_unique<UIElement>();
    UIElement* mid = root->create_child<UIElement>();
    UIElement* leaf = mid->create_child<UIElement>();

    int midFirst = 0;
    int midSecond = 0;
    (void)mid->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs& a) {
        ++midFirst;
        a.handled = true; // stops: the later handler of mid + the root won't fire
    });
    (void)mid->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs&) { ++midSecond; });
    bool rootFired = false;
    (void)root->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs&) { rootFired = true; });

    RoutedEventArgs args;
    args.source = leaf;
    leaf->RaiseEvent(vaml::ClickEvent(), args);

    SHOULD(ShouldlyIntegral(midFirst).ShouldBe(1));
    SHOULD(ShouldlyIntegral(midSecond).ShouldBe(0)); // stopped by the first mid handler
    SHOULD(ShouldlyIntegral(rootFired).ShouldBe(false));
}

TEST_CASE("S321 tunneling: a preview (tunnel) event routes ROOT -> source in order")
{
    auto root = std::make_unique<UIElement>();
    UIElement* mid = root->create_child<UIElement>();
    UIElement* leaf = mid->create_child<UIElement>();

    const RoutedEvent& preview = RoutedEvent::registered("PreviewS321", RouteEventDirection::Tunnel);

    std::vector<UIElement*> visited;
    (void)root->add_routed_handler(preview, [&](RoutedEventArgs&) { visited.push_back(root.get()); });
    (void)mid->add_routed_handler(preview, [&](RoutedEventArgs&) { visited.push_back(mid); });
    (void)leaf->add_routed_handler(preview, [&](RoutedEventArgs&) { visited.push_back(leaf); });

    RoutedEventArgs args;
    args.source = leaf;
    leaf->RaiseEvent(preview, args);

    SHOULD(ShouldlyIntegral(visited.size()).ShouldBe(3));
    SHOULD(ShouldlyString(visited[0]).ShouldBe(root.get()));
    SHOULD(ShouldlyString(visited[1]).ShouldBe(mid));
    SHOULD(ShouldlyString(visited[2]).ShouldBe(leaf));
}

TEST_CASE("S321 tunneling: handled during the tunnel stops it BEFORE the source fires")
{
    auto root = std::make_unique<UIElement>();
    UIElement* mid = root->create_child<UIElement>();
    UIElement* leaf = mid->create_child<UIElement>();

    const RoutedEvent& preview = RoutedEvent::registered("PreviewStopS321", RouteEventDirection::Tunnel);

    (void)root->add_routed_handler(preview, [](RoutedEventArgs& a) { a.handled = true; });
    bool leafFired = false;
    (void)leaf->add_routed_handler(preview, [&](RoutedEventArgs&) { leafFired = true; });

    RoutedEventArgs args;
    args.source = leaf;
    leaf->RaiseEvent(preview, args);

    SHOULD(ShouldlyIntegral(leafFired).ShouldBe(false)); // tunnel stopped at the root
    SHOULD(ShouldlyIntegral(args.handled).ShouldBe(true));
}

TEST_CASE("S321 source/originalSource correctness: source moves per hop, originalSource is fixed")
{
    auto root = std::make_unique<UIElement>();
    UIElement* mid = root->create_child<UIElement>();
    UIElement* leaf = mid->create_child<UIElement>();

    UIElement* sourceAtLeaf = nullptr;
    UIElement* sourceAtRoot = nullptr;
    UIElement* originalSeen = nullptr;
    (void)leaf->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs& a) {
        sourceAtLeaf = static_cast<UIElement*>(a.source);
        originalSeen = static_cast<UIElement*>(a.originalSource);
    });
    (void)root->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs& a) {
        sourceAtRoot = static_cast<UIElement*>(a.source);
    });

    RoutedEventArgs args;
    args.source = leaf;
    leaf->RaiseEvent(vaml::ClickEvent(), args);

    SHOULD(ShouldlyString(sourceAtLeaf).ShouldBe(leaf));
    SHOULD(ShouldlyString(originalSeen).ShouldBe(leaf));
    SHOULD(ShouldlyString(sourceAtRoot).ShouldBe(root.get()));
}

TEST_CASE("S321 the ROUTE is the real parent chain: a panel child click bubbles through the panel")
{
    // Real StackPanel (S129, a UIElement) hosting a child; the click on the
    // child bubbles through the panel to the root — the real parent() chain,
    // no separate structure.
    auto root = std::make_unique<UIElement>();
    auto panel = root->create_child<vaml::StackPanel>();
    UIElement* child = panel->create_child<UIElement>();

    bool panelFired = false;
    bool rootFired = false;
    (void)panel->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs&) { panelFired = true; });
    (void)root->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs&) { rootFired = true; });

    RoutedEventArgs args;
    args.source = child;
    child->RaiseEvent(vaml::ClickEvent(), args);

    SHOULD(ShouldlyIntegral(panelFired).ShouldBe(true)); // bubbled through the panel
    SHOULD(ShouldlyIntegral(rootFired).ShouldBe(true));
}

TEST_CASE("S321 remove_routed_handler stops a handler; remaining keep order")
{
    auto root = std::make_unique<UIElement>();
    int first = 0;
    int second = 0;

    auto t1 = root->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs&) { ++first; });
    auto t2 = root->add_routed_handler(vaml::ClickEvent(), [&](RoutedEventArgs&) { ++second; });

    (void)root->remove_routed_handler(vaml::ClickEvent(), t1);
    RoutedEventArgs args;
    args.source = root.get();
    root->RaiseEvent(vaml::ClickEvent(), args);

    SHOULD(ShouldlyIntegral(first).ShouldBe(0));
    SHOULD(ShouldlyIntegral(second).ShouldBe(1));
    (void)t2;
}
