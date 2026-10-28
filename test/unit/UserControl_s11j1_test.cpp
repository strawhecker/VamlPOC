// 11j-1 (GAP CLOSE, dep:S450_1) — the `UserControl` element.
//
// RESOLVES the single largest corpus blocker: ~90 external WPF corpus files
// author `<UserControl .../>` as a standalone root / nested content container,
// which previously threw `unknown element 'UserControl'`. 11j-1 makes
// UserControl a REAL, permanent, loadable content-container element (a
// self-contained UIElement with a single-content child slot), so a UserControl
// root RENDERS its content through the generic classifier.
//
// SCOPE BOUNDARY (recorded, NO-GAP not silent): the full `ContentControl`
// ABSTRACT BASE is the S649 continuation — NOT introduced here. UserControl is
// self-contained (Content DP + Window-faithful single-content slot), satisfying
// the corpus authoring surface without pre-empting S649. Width/Height inherit
// the base UIElement layout DPs; FontSize resolves against the shared Control
// base DP hoisted at 11j-5 (bare "FontSize" — recorded, not silent).
//
// Headless (Rail 1) — no window/runtime, no Vulkan (the standing S160+
// precedent).

#include "vaml/core/Element.h"     // UserControl / Grid / UIElement
#include "vaml/core/ElementFactoryRegistry.h" // S087: type-name registry
#include "vaml/core/VamlReader.h"  // the text load path (root-load proof)

#include "doctest/doctest.h"

#include <memory>
#include <string>

using namespace vaml;

TEST_CASE("11j-1: UserControl is a real UIElement that can host content")
{
    UserControl uc;
    CHECK(dynamic_cast<UIElement*>(&uc) != nullptr);
    CHECK(dynamic_cast<DependencyObject*>(&uc) != nullptr);
    CHECK(uc.child_count() == 0u);
    CHECK(uc.content() == nullptr);

    // Single-content slot (Window-faithful): one logical content child owned by
    // the visual-children list (S121), content() = the non-owning observer.
    auto child = std::make_unique<Grid>();
    Grid* raw = child.get();
    uc.set_content(std::move(child));
    CHECK(uc.child_count() == 1u);
    CHECK(uc.content() == raw);
    CHECK(uc.child_at(0) == raw);
    CHECK(raw->parent() == &uc);

    // A second set_content REPLACES the first.
    auto child2 = std::make_unique<Grid>();
    Grid* raw2 = child2.get();
    uc.set_content(std::move(child2));
    CHECK(uc.child_count() == 1u);
    CHECK(uc.content() == raw2);
    CHECK(uc.child_at(0) == raw2);
}

TEST_CASE("11j-1: UserControl resolves through the factory by name")
{
    register_builtin_elements();
    ElementFactoryRegistry& factory = ElementFactoryRegistry::global();
    auto created = factory.create("UserControl");
    REQUIRE(created != nullptr);
    CHECK(dynamic_cast<UserControl*>(created.get()) != nullptr);
    CHECK(dynamic_cast<UIElement*>(created.get()) != nullptr);
}

TEST_CASE("11j-1: UserControl is loadable as a standalone document root")
{
    // Text path (the S2 dev reader). Load builds the root + queues children;
    // the S440 finalize hand-off transfers the content into the S121 list.
    VamlReader reader;
    auto root = reader.Load("<UserControl><Grid/></UserControl>");
    REQUIRE(root != nullptr);
    CHECK(dynamic_cast<UserControl*>(root.get()) != nullptr);
    // Finalize: the single content child is routed into set_content (11j-1) +
    // the S121 list, so a UserControl root RENDERS its content.
    CHECK(reader.finalize_visual_tree() == 1u);
    CHECK(dynamic_cast<UIElement*>(root.get())->child_count() == 1u);
    CHECK(dynamic_cast<UserControl*>(root.get())->content() != nullptr);
}
