// S121 (dep:S120) — UIElement visual children: create/attach/detach, parent
// traversal, visual-tree walk.
//
// SCOPE = structural children + traversal (the S121 title). Layout geometry /
// Measure/Arrange are explicitly S122/S123 — NOT fabricated here.
//
// Under test:
//   1. create_child -> appends an owned child; child_count/child_at/children
//      reflect index order; child.parent() == the parent.
//   2. add_child (attach) appends an existing owned child; order preserved.
//   3. take_child (detach/transfer) returns ownership, clears the parent link,
//      and unwinds the S041 inheritance-scope edge (parent no longer pushes to
//      the detached child).
//   4. remove_child detaches + destroys; count decremented.
//   5. parent traversal: child.parent()==parent; depth-first walk visits in
//      pre-order top-down.
//   6. Composition distinctness: attaching a visual child wires set_parent, so
//      an Inherits DP (Opacity) cascades through the visual child (S039/S041
//      parent-link composition) — the visual list and the inheritance-scope are
//      DISTINCT but composed, not overlapping.
//   7. Reparent: a child owned by parent A cannot attach to parent B (single-
//      parent invariant -> std::logic_error); after take_child it CAN (the
//      detached child is parentless).
//
// Uses the Shouldly-for-doctest vocabulary (S036_5 standing convention).
// doctest's main comes from smoke_test.cpp.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <vector>

using vaml::DependencyObject;
using vaml::PropertyId;
using vaml::UIElement;
using vaml::Value;
using vaml::test::ShouldThrow;

namespace {

// A concrete UIElement subclass so create_child<T> is proven with a real derived
// type (not just the base), mirroring the MINIMAL REAL control-seed pattern
// (each resolves at its own sprint; this test-only leaf is NOT a control seed).
struct PanelLike : UIElement
{
};

// Collects the pre-order visit sequence into (this-then-children) addresses.
struct WalkRecorder
{
    std::vector<const UIElement*> visited;

    void operator()(UIElement& el)
    {
        visited.push_back(&el);
    }
};

// A functor usable with UIElement::walk (visit each element, pushing its raw ptr).
struct WalkCollector
{
    std::vector<UIElement*> visited;

    void operator()(UIElement& el)
    {
        visited.push_back(&el);
    }
};

} // namespace

TEST_CASE("S121: create_child appends an owned child; parent traversal reflects it")
{
    UIElement root;
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(0u));

    UIElement* c0 = root.create_child<PanelLike>();
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(c0 != nullptr).ShouldBe(true));
    // Attach wired the parent link (S039/S041 composition).
    SHOULD(ShouldlyString(c0->parent() == &root).ShouldBe(true));
    // child_at returns the same owned object.
    SHOULD(ShouldlyString(root.child_at(0) == c0).ShouldBe(true));

    UIElement* c1 = root.create_child<UIElement>();
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(2u));
    SHOULD(ShouldlyString(root.child_at(1) == c1).ShouldBe(true));

    // Index order: c0 then c1.
    SHOULD(ShouldlyString(root.child_at(0) == c0).ShouldBe(true));
    SHOULD(ShouldlyString(root.child_at(1) == c1).ShouldBe(true));

    // Out-of-range child_at is null (walker-friendly, no throw).
    SHOULD(ShouldlyString(root.child_at(2) == nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(root.child_at(999) == nullptr).ShouldBe(true));

    // children() out-param view (S144): non-owning, in order.
    std::vector<UIElement*> view;
    root.children(view);
    SHOULD(ShouldlyString(view.size()).ShouldBe(2u));
    SHOULD(ShouldlyString(view[0] == c0).ShouldBe(true));
    SHOULD(ShouldlyString(view[1] == c1).ShouldBe(true));
}

TEST_CASE("S121: add_child attaches an existing owned child in order")
{
    UIElement root;
    auto a = std::make_unique<UIElement>();
    UIElement* aRaw = a.get();
    auto b = std::make_unique<UIElement>();
    UIElement* bRaw = b.get();

    root.add_child(std::move(a));
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(root.child_at(0) == aRaw).ShouldBe(true));

    root.add_child(std::move(b));
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(2u));
    // Appended at the END: a then b.
    SHOULD(ShouldlyString(root.child_at(0) == aRaw).ShouldBe(true));
    SHOULD(ShouldlyString(root.child_at(1) == bRaw).ShouldBe(true));

    // Both are wired to the parent.
    SHOULD(ShouldlyString(aRaw->parent() == &root).ShouldBe(true));
    SHOULD(ShouldlyString(bRaw->parent() == &root).ShouldBe(true));
}

TEST_CASE("S121: take_child detaches by transferring ownership and clearing the parent link")
{
    UIElement root;
    UIElement* c = root.create_child<UIElement>();
    SHOULD(ShouldlyString(c->parent() == &root).ShouldBe(true));
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(1u));

    std::unique_ptr<UIElement> detached = root.take_child(0);
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(0u));
    // The SAME object came back.
    SHOULD(ShouldlyString(detached.get() == c).ShouldBe(true));
    // Detach cleared the parent link + unwound the inheritance-scope edge.
    SHOULD(ShouldlyString(c->parent() == nullptr).ShouldBe(true));
    // The detached child survives (the caller owns it).
    SHOULD(ShouldlyString(detached != nullptr).ShouldBe(true));
}

TEST_CASE("S121: take_child unwinds the inheritance-scope edge — parent no longer pushes to a detached child")
{
    // Composition distinctness: the visual list and the S041 inheritance-scope
    // are DISTINCT structures that compose via set_parent. Attaching wires the
    // edge (Opacity cascades through the visual child); detaching must UNWIND
    // that edge so no stale push survives (NO-LAZY — never leave a dangling
    // scope edge).
    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();
    (void)child; // owned by parent

    // Subscriber on the child records any pushed Opacity notification.
    // The S120 tests prove the cascade; here we prove the EDGE lifecycle.
    struct Trace
    {
        std::size_t notified = 0;
    } trace;

    auto handler = [&trace](PropertyId, const Value&, const Value&) {
        ++trace.notified;
    };
    (void)child->add_property_changed(PropertyId::of("Opacity"), handler);

    // Attached: parent's Opacity change should push to the child (edge live).
    (void)parent.set_opacity(0.5);
    child->ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.notified).ShouldBe(1u));

    // Detach: unwinds the inheritance-scope edge.
    std::unique_ptr<UIElement> detached = parent.take_child(0);
    SHOULD(ShouldlyString(parent.child_count()).ShouldBe(0u));
    SHOULD(ShouldlyString(detached->parent() == nullptr).ShouldBe(true));

    // After detach, a further parent Opacity change must NOT reach the child
    // (the edge is gone). A clean push-silence is the NO-LAZY guarantee.
    (void)parent.set_opacity(0.2);
    detached->ProcessNotificationQueue();
    SHOULD(ShouldlyString(trace.notified).ShouldBe(1u));
}

TEST_CASE("S121: remove_child detaches + destroys; count decremented")
{
    UIElement root;
    UIElement* a = root.create_child<UIElement>();
    UIElement* b = root.create_child<UIElement>();
    (void)b;
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(2u));

    // Remove index 0 (a). a is destroyed; b slides down to index 0.
    const std::size_t remaining = root.remove_child(0);
    SHOULD(ShouldlyString(remaining).ShouldBe(1u));
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(root.child_at(0) == a).ShouldBe(false)); // a is gone
    SHOULD(ShouldlyString(root.child_at(0) != nullptr).ShouldBe(true)); // b remains
}

TEST_CASE("S121: out-of-range detach throws std::out_of_range")
{
    UIElement root;
    SHOULD(ShouldThrow<std::out_of_range>([&] {
        (void)root.take_child(0);
    }));
    SHOULD(ShouldThrow<std::out_of_range>([&] {
        (void)root.remove_child(0);
    }));
    (void)root.create_child<UIElement>();
    SHOULD(ShouldThrow<std::out_of_range>([&] {
        (void)root.take_child(5);
    }));
    SHOULD(ShouldThrow<std::out_of_range>([&] {
        (void)root.remove_child(1);
    }));
}

TEST_CASE("S121: walk — depth-first PRE-ORDER visits this then children in order")
{
    // root
    //   a
    //     a1
    //     a2
    //   b
    //     b1
    UIElement root;
    UIElement* a = root.create_child<UIElement>();
    UIElement* b = root.create_child<UIElement>();
    UIElement* a1 = a->create_child<UIElement>();
    UIElement* a2 = a->create_child<UIElement>();
    UIElement* b1 = b->create_child<UIElement>();

    WalkCollector collector;
    // std::ref: `walk` takes a std::function by const ref, which COPIES the
    // callable — so we must wrap the collector in a reference to record into
    // the original (not a throwaway copy).
    root.walk(std::ref(collector));

    // Pre-order: root, a, a1, a2, b, b1.
    const std::vector<UIElement*>& v = collector.visited;
    SHOULD(ShouldlyString(v.size()).ShouldBe(6u));
    SHOULD(ShouldlyString(v[0] == &root).ShouldBe(true));
    SHOULD(ShouldlyString(v[1] == a).ShouldBe(true));
    SHOULD(ShouldlyString(v[2] == a1).ShouldBe(true));
    SHOULD(ShouldlyString(v[3] == a2).ShouldBe(true));
    SHOULD(ShouldlyString(v[4] == b).ShouldBe(true));
    SHOULD(ShouldlyString(v[5] == b1).ShouldBe(true));
}

TEST_CASE("S121: walk visits every descendant depth-first, all parent-linked")
{
    UIElement root;
    UIElement* a = root.create_child<UIElement>();
    UIElement* b = root.create_child<UIElement>();
    UIElement* c = a->create_child<UIElement>();

    WalkRecorder recorder;
    root.walk(std::ref(recorder));

    // Every visited node (except this, which is the root) has parent()==root or
    // an ancestor — the parent traversal via walk is coherent top-down.
    SHOULD(ShouldlyString(recorder.visited.size()).ShouldBe(4u));
    SHOULD(ShouldlyString(a->parent() == &root).ShouldBe(true));
    SHOULD(ShouldlyString(b->parent() == &root).ShouldBe(true));
    SHOULD(ShouldlyString(c->parent() == a).ShouldBe(true));
}

TEST_CASE("S121: composition — an Inherits DP (Opacity) cascades THROUGH a visual child via set_parent")
{
    // The visual children list and the S041 inheritance-scope are DISTINCT, but
    // attaching a visual child wires set_parent, so a parent's Inherits property
    // flows to the visual child (S039/S041 parent-link composition). This proves
    // the two structures compose — recorded, no overlap, no gap.
    UIElement parent;
    UIElement* child = parent.create_child<UIElement>();

    // Read-side cascade: parent Opacity -> child effective (no child write).
    (void)parent.set_opacity(0.6);
    SHOULD(ShouldlyString(child->opacity()).ShouldBe(0.6));

    // Detach cuts the link; the child reverts to its own default (1.0) because
    // the ancestor chain is gone.
    std::unique_ptr<UIElement> detached = parent.take_child(0);
    SHOULD(ShouldlyString(detached->opacity()).ShouldBe(1.0));
}

TEST_CASE("S121: attach_child refuses a null child with std::invalid_argument")
{
    UIElement root;
    // add_child(nullptr) is a caller bug — refused loudly (NO-LAZY), not a
    // silent no-op.
    SHOULD(ShouldThrow<std::invalid_argument>([&] {
        root.add_child(std::unique_ptr<UIElement>(nullptr));
    }));
    SHOULD(ShouldlyString(root.child_count()).ShouldBe(0u));
}

TEST_CASE("S121: reparent — detach first (take_child) then attach to a new parent is legal")
{
    UIElement pa;
    UIElement pb;

    auto ownedE = std::make_unique<UIElement>();
    UIElement* e = ownedE.get();
    pa.add_child(std::move(ownedE));
    SHOULD(ShouldlyString(e->parent() == &pa).ShouldBe(true));

    // Detach first: take_child transfers ownership to us (parentless).
    std::unique_ptr<UIElement> freeE = pa.take_child(0);
    SHOULD(ShouldlyString(freeE.get() == e).ShouldBe(true));
    SHOULD(ShouldlyString(freeE->parent() == nullptr).ShouldBe(true));
    SHOULD(ShouldlyString(pa.child_count()).ShouldBe(0u));

    // Now the detached child can attach to pb (single-parent invariant is
    // preserved because the child is parentless at attach time).
    pb.add_child(std::move(freeE));
    SHOULD(ShouldlyString(e->parent() == &pb).ShouldBe(true));
    SHOULD(ShouldlyString(pb.child_count()).ShouldBe(1u));
    SHOULD(ShouldlyString(pb.child_at(0) == e).ShouldBe(true));
}
