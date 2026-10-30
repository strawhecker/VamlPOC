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
    // 11k-1: the invented `Text` is retired; the WPF-true `TextBlock` is the
    // authored text leaf.
    const char* controls[] = { "TextBlock", "Button", "TextBox", "Slider",
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

    // Sprint 11c: the DockPanel + selection controls are REAL UIElement re-bases
    // (S470–S473 — resolved at their own steps, no DependencyObject-only seeds).
    const char* controls11c[] = { "DockPanel", "RadioButton", "ComboBox",
                                  "ListBox", "ListView" };
    for (const char* name : controls11c)
    {
        auto e = g.try_create(name);
        SHOULD(ShouldlyIntegral(e != nullptr).ShouldBe(true));
        if (e)
        {
            SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(e.get()) != nullptr)
                       .ShouldBe(true));
        }
    }
}

TEST_CASE("S407: the S10 controls are registered as concrete built-in types")
{
    vaml::register_builtin_elements();
    vaml::ElementFactoryRegistry& g = vaml::ElementFactoryRegistry::global();

    // The concrete controls resolve to their real types (the S087 headline
    // dispatch, now REAL controls end-to-end). 11k-1: TextBlock is registered,
    // the S087-invented `Text` name is RETIRED (so it is NOT registered).
    SHOULD(ShouldlyIntegral(g.is_registered("TextBlock")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Text")).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.is_registered("Button")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("TextBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Slider")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CheckBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ScrollViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Icon")).ShouldBe(true));
    // Sprint 11c: the new controls are registered built-ins.
    SHOULD(ShouldlyIntegral(g.is_registered("DockPanel")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("RadioButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ComboBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ListBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ListView")).ShouldBe(true));
    // Sprint 11d: the core display + input controls are registered built-ins.
    SHOULD(ShouldlyIntegral(g.is_registered("Label")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Border")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Rectangle")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Ellipse")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Line")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Image")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ProgressBar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("PasswordBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("GroupBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Expander")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Separator")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("StatusBar")).ShouldBe(true));
    // The built-in count: 10 roadmap controls + WrapPanel + Canvas + Icon +
    // S453 (SolidColorBrush + Style) + S460 (LinearGradientBrush + GradientStop)
    // + Sprint 11c (DockPanel/RadioButton/ComboBox/ListBox/ListView) +
    // Sprint 11d (Label/Border/Rectangle/Ellipse/Line/Image/ProgressBar/
    // PasswordBox/GroupBox/Expander/Separator/StatusBar) + Sprint 11j
    // (UserControl + Application + ResourceDictionaryHolder) + 11k-2
    // (ScrollBar/RepeatButton/Viewbox/ToolBar/ToolBarTray) + 11k-4
    // (PathGeometry/GeometryGroup/RectangleGeometry/EllipseGeometry/
    // GeometryDrawing/DrawingGroup/DrawingBrush/DrawingImage/ImageBrush) +
    // Sprint 11e (11e: TabControl/TabItem/Menu/MenuItem/ContextMenu/ToolTip/
    // Popup/GridSplitter/DataGrid) = 59 (count bumps authored at the step,
    // never silent).
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(59u));
    SHOULD(ShouldlyIntegral(g.is_registered("UserControl")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Application")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ResourceDictionary")).ShouldBe(true));
}
