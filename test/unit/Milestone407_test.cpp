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
    // 59 (S11e set) + 2 (S502 behaviors: EventTrigger + InvokeCommandAction) +
    // 16 (S550 scaffolding: ComboBoxItem/ListBoxItem/ListViewItem/StatusBarItem/
    // GroupItem/DataGridRow/DataGridCell/ItemsPresenter/ItemsPanelTemplate/
    // ItemContainerTemplate/GridView/GridViewRowPresenter/
    // GridViewHeaderRowPresenter/GridViewColumnHeader/VirtualizingStackPanel/
    // SelectiveScrollingGrid) = 78.
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(100u));
    SHOULD(ShouldlyIntegral(g.is_registered("UserControl")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Application")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ResourceDictionary")).ShouldBe(true));
    // S550 scaffolding resolved + loadable (each a REAL permanent symbol).
    SHOULD(ShouldlyIntegral(g.is_registered("ComboBoxItem")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ListBoxItem")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ListViewItem")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("StatusBarItem")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("GroupItem")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("DataGridRow")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("DataGridCell")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ItemsPresenter")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ItemsPanelTemplate")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ItemContainerTemplate")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("GridView")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("GridViewRowPresenter")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("GridViewHeaderRowPresenter")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("GridViewColumnHeader")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("VirtualizingStackPanel")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("SelectiveScrollingGrid")).ShouldBe(true));
    // S551 shape/channel tail resolved + loadable (each a REAL permanent symbol;
    // the abstract Shape base stays unregistered — the 11k-4 precedent).
    SHOULD(ShouldlyIntegral(g.is_registered("Path")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Polygon")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Polyline")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Track")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("TickBar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Shape")).ShouldBe(false));
    // S552 tree view controls resolved + loadable (each a REAL permanent symbol).
    SHOULD(ShouldlyIntegral(g.is_registered("TreeView")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("TreeViewItem")).ShouldBe(true));
    // S553 date/calendar controls resolved + loadable (each a REAL permanent
    // symbol; CalendarDateRange/CalendarBlackoutDatesCollection are pure data,
    // no factory element).
    SHOULD(ShouldlyIntegral(g.is_registered("DatePicker")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("DatePickerTextBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Calendar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarDayButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarItem")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarDateRange")).ShouldBe(false));
    // S554 drawing/media tail resolved + loadable (InkCanvas/InkPresenter are
    // DONE-classified strokes surfaces; MediaElement real). WebBrowser/ViewPort3D
    // are OUT-OF-SCOPE(11m) — the S520 import/inject seam, reclassified at S559.
    SHOULD(ShouldlyIntegral(g.is_registered("InkCanvas")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("InkPresenter")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("MediaElement")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("WebBrowser")).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.is_registered("ViewPort3D")).ShouldBe(false));
    // S555 document/frame tail resolved + loadable (each a REAL permanent
    // symbol; DocumentViewerBase is abstract — unregistered).
    SHOULD(ShouldlyIntegral(g.is_registered("Page")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Frame")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("DocumentViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("FlowDocumentReader")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("FlowDocumentScrollViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("SinglePageViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("DocumentViewerBase")).ShouldBe(false));
}
