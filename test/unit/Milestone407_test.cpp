// S407 (dep:S406) — Milestone: all controls fully implemented, each has a test,
// no stubs (vaml::core). Headless proof that the S400–S406 control set is the
// REAL re-based set (every control is a vaml::UIElement, participating in the
// visual tree + layout; NONE is a DependencyObject-only seed anymore), and that
// the built-in registry produces the concrete control types. This is the S407
// "each control has a test, no stubs" milestone gate at the type level.
//
// Uses the standing Shouldly-for-doctest vocabulary.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>                // the S400-S406 controls + register_builtin_elements
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/UIElement.h>

#include <memory>

TEST_CASE("S407: every S10 control is a real UIElement re-base (no DependencyObject-only seeds)")
{
    vaml::register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();

    // The S10 control set (all introduced/resolved this sprint) must each be a
    // concrete vaml::UIElement — NOT a DependencyObject-only seed.
    const char* controls[] = { "Text", "Button", "TextBox", "Slider",
                               "CheckBox", "ScrollViewer" };
    for (const char* name : controls)
    {
        auto e = g.try_create(name);
        SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(true));
        if (e)
        {
            SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr)
                       .ShouldBe(true));
        }
    }

    // Icon (S400_1, a NEW real control) is also a UIElement re-base.
    auto icon = g.try_create("Icon");
    SHOULD(ShouldlyIntegral(icon != nullptr).ShouldBe(true));
    if (icon)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(icon.get()) != nullptr)
                   .ShouldBe(true));
    }
}

TEST_CASE("S407: the S10 controls are registered as concrete built-in types")
{
    vaml::register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();

    // The concrete controls resolve to their real types (the S087 headline
    // dispatch, now REAL controls end-to-end).
    SHOULD(ShouldlyIntegral(g.is_registered("Text")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Button")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("TextBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Slider")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CheckBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ScrollViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Icon")).ShouldBe(true));
    // The built-in count: 9 seeds + WrapPanel + Canvas + Icon = 12.
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(12u));
}
