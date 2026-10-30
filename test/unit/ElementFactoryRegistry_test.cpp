// S087 (dep:S086) — ElementFactoryRegistry tests.
//
// Proves the typeName -> factory<unique_ptr<DependencyObject>> registry + the
// startup registration (register_builtin_elements):
//   * register + create returns a live DependencyObject of the right CONCRETE
//     type (dynamic_cast<Button*> non-null -> typeName dispatches to a real
//     type, not a generic blob);
//   * a registered factory produces a FRESH instance per create() call;
//   * duplicate registration throws ElementAlreadyRegistered and the registry
//     is UNCHANGED (name-unique dedupe, S032 discipline);
//   * create() on an unknown name throws ElementNotFound;
//   * try_create() on an unknown name -> nullptr (never throws);
//   * is_registered / size / empty;
//   * registered_names() returns INSERTION ORDER deterministically;
//   * STARTUP GATE — register_builtin_elements() then create("Button"),
//     create("StackPanel"), ... yields the concrete type for every built-in
//     element name (the S087 headline verified end-to-end).
//
// Uses the standing Shouldly-for-doctest vocabulary (S036_5 convention).

// doctest's main comes from smoke_test.cpp.
#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/DependencyObject.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using vaml::Button;
using vaml::Canvas;
using vaml::CheckBox;
using vaml::ComboBox;
using vaml::DependencyObject;
using vaml::DockPanel;
using vaml::ElementAlreadyRegistered;
using vaml::ElementFactory;
using vaml::ElementFactoryRegistry;
using vaml::ElementNotFound;
using vaml::Grid;
using vaml::Icon;
using vaml::ListBox;
using vaml::ListView;
using vaml::RadioButton;
using vaml::RepeatButton;
using vaml::ScrollBar;
using vaml::ScrollViewer;
using vaml::Slider;
using vaml::ToolBar;
using vaml::ToolBarTray;
using vaml::StackPanel;
using vaml::TextBlock;
using vaml::TextBox;
using vaml::Window;
using vaml::WrapPanel;
using vaml::register_builtin_elements;
using vaml::test::ShouldThrow;

namespace {

// Assert `p` is non-null AND dynamic_cast<Concrete*>(p) is non-null (proves the
// produced object's concrete type, not a generic DependencyObject).
template <typename Concrete>
void assertConcrete(const std::unique_ptr<DependencyObject>& p)
{
    SHOULD(ShouldlyIntegral(p != nullptr).ShouldBe(true));
    if (p)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<Concrete*>(p.get()) != nullptr).ShouldBe(true));
    }
}

} // namespace

// ---------------------------------------------------------------------------
// register + create -> concrete instance
// ---------------------------------------------------------------------------

TEST_CASE("S087: register+create returns a live object of the concrete type")
{
    ElementFactoryRegistry r;
    r.register_element("ButtonS087", [] { return std::make_unique<Button>(); });

    std::unique_ptr<DependencyObject> o = r.create("ButtonS087");
    assertConcrete<Button>(o);
}

TEST_CASE("S087: register+create returns a live object of the concrete type (WrapPanel)")
{
    // S134: a WrapPanel concrete instance resolves from the registry (the
    // typeName -> concrete-type dispatch is real, not a generic blob).
    ElementFactoryRegistry r;
    r.register_element("WrapPanelS134", [] { return std::make_unique<WrapPanel>(); });
    std::unique_ptr<DependencyObject> o = r.create("WrapPanelS134");
    assertConcrete<WrapPanel>(o);
}

TEST_CASE("S087: register+create returns a live object of the concrete type (Canvas)")
{
    // S136: a Canvas concrete instance resolves from the registry (the
    // typeName -> concrete-type dispatch is real, not a generic blob).
    ElementFactoryRegistry r;
    r.register_element("CanvasS136", [] { return std::make_unique<Canvas>(); });
    std::unique_ptr<DependencyObject> o = r.create("CanvasS136");
    assertConcrete<Canvas>(o);
}

TEST_CASE("S087: create produces a fresh instance per call")
{
    ElementFactoryRegistry r;
    r.register_element("TextS087", [] { return std::make_unique<TextBlock>(); });

    std::unique_ptr<DependencyObject> a = r.create("TextS087");
    std::unique_ptr<DependencyObject> b = r.create("TextS087");
    SHOULD(ShouldlyIntegral(a != nullptr).ShouldBe(true));
    SHOULD(ShouldlyIntegral(b != nullptr).ShouldBe(true));
    // Two distinct objects, both concrete TextBlock.
    SHOULD(ShouldlyIntegral(a.get() != b.get()).ShouldBe(true));
    assertConcrete<TextBlock>(a);
    assertConcrete<TextBlock>(b);
}

// ---------------------------------------------------------------------------
// dedupe
// ---------------------------------------------------------------------------

TEST_CASE("S087: duplicate registration throws ElementAlreadyRegistered; registry unchanged")
{
    ElementFactoryRegistry r;
    r.register_element("ButtonS087", [] { return std::make_unique<Button>(); });
    const std::size_t before = r.size();

    bool threw = false;
    try
    {
        // Registering the same NAME with a DIFFERENT factory must be rejected.
        r.register_element("ButtonS087", [] { return std::make_unique<Grid>(); });
    }
    catch (const ElementAlreadyRegistered& e)
    {
        threw = true;
        SHOULD(ShouldlyString(std::string(e.what())).ShouldNotBe(std::string("")));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));

    // Registry unchanged: size identical AND the ORIGINAL factory still resolves
    // to the Button (a rejected insert never swaps the factory).
    SHOULD(ShouldlyIntegral(r.size()).ShouldBe(before));
    assertConcrete<Button>(r.create("ButtonS087"));
}

TEST_CASE("S087: ShouldThrow form of dedupe rejection")
{
    ElementFactoryRegistry r;
    r.register_element("WindowS087", [] { return std::make_unique<Window>(); });
    // Same name -> ElementAlreadyRegistered via the typed-exception assertion.
    SHOULD(ShouldThrow<ElementAlreadyRegistered>([&] {
        r.register_element("WindowS087", [] { return std::make_unique<Window>(); });
    }));
    // Still the Window, still registered exactly once.
    assertConcrete<Window>(r.create("WindowS087"));
    SHOULD(ShouldlyIntegral(r.size()).ShouldBe(1u));
}

// ---------------------------------------------------------------------------
// lookup failures
// ---------------------------------------------------------------------------

TEST_CASE("S087: create on an unknown name throws ElementNotFound")
{
    ElementFactoryRegistry r;
    SHOULD(ShouldThrow<ElementNotFound>([&] {
        (void)r.create("DoesNotExistS087");
    }));
}

TEST_CASE("S087: try_create on an unknown name returns nullptr (never throws)")
{
    ElementFactoryRegistry r;
    SHOULD(ShouldlyIntegral(r.try_create("DoesNotExistS087") == nullptr).ShouldBe(true));
}

// ---------------------------------------------------------------------------
// is_registered / size / empty / insertion order
// ---------------------------------------------------------------------------

TEST_CASE("S087: is_registered + size + empty reflect registration")
{
    ElementFactoryRegistry r;
    SHOULD(ShouldlyIntegral(r.empty()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(r.is_registered("X_S087")).ShouldBe(false));

    r.register_element("X_S087", [] { return std::make_unique<Button>(); });
    SHOULD(ShouldlyIntegral(r.empty()).ShouldBe(false));
    SHOULD(ShouldlyIntegral(r.is_registered("X_S087")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(r.is_registered("Y_S087")).ShouldBe(false));
    SHOULD(ShouldlyIntegral(r.size()).ShouldBe(1u));
}

TEST_CASE("S087: registered_names preserves insertion order deterministically")
{
    ElementFactoryRegistry r;
    r.register_element("FirstS087", [] { return std::make_unique<Button>(); });
    r.register_element("SecondS087", [] { return std::make_unique<TextBlock>(); });
    r.register_element("ThirdS087", [] { return std::make_unique<Grid>(); });

    // Insertion order, not hash order — deterministic across runs. S144:
    // registered_names() returns string_views (no by-value owned copies).
    const std::vector<std::string_view> names = r.registered_names();
    SHOULD(ShouldlyIntegral(names.size()).ShouldBe(3u));
    SHOULD(ShouldlyString(std::string(names[0])).ShouldBe(std::string("FirstS087")));
    SHOULD(ShouldlyString(std::string(names[1])).ShouldBe(std::string("SecondS087")));
    SHOULD(ShouldlyString(std::string(names[2])).ShouldBe(std::string("ThirdS087")));
}

// ---------------------------------------------------------------------------
// STARTUP GATE — register_builtin_elements() + create() for every built-in
// ---------------------------------------------------------------------------

TEST_CASE("S087: startup registration — every built-in element resolves to its concrete type")
{
    // register_builtin_elements() populates the process-wide singleton.
    register_builtin_elements();

    ElementFactoryRegistry& g = ElementFactoryRegistry::global();

    SHOULD(ShouldlyIntegral(g.is_registered("Button")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("StackPanel")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Grid")).ShouldBe(true));
    // 11k-1: TextBlock is registered, the S087-invented Text name is RETIRED.
    SHOULD(ShouldlyIntegral(g.is_registered("TextBlock")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Text")).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.is_registered("TextBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CheckBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Slider")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ScrollViewer")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Window")).ShouldBe(true));
    // S134: WrapPanel is a real built-in (registered at S134, not a seed).
    SHOULD(ShouldlyIntegral(g.is_registered("WrapPanel")).ShouldBe(true));
    // S136: Canvas is a real built-in (registered at S136, not a seed).
    SHOULD(ShouldlyIntegral(g.is_registered("Canvas")).ShouldBe(true));
    // S400_1: Icon is a real control (registered at S400_1, not a seed).
    SHOULD(ShouldlyIntegral(g.is_registered("Icon")).ShouldBe(true));
    // Sprint 11c: the DockPanel + selection controls are real built-ins.
    SHOULD(ShouldlyIntegral(g.is_registered("DockPanel")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("RadioButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ComboBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ListBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ListView")).ShouldBe(true));
    // Sprint 11d: the core display + input controls are real built-ins.
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
    // 11k-2: the missing WPF control set is registered.
    SHOULD(ShouldlyIntegral(g.is_registered("ScrollBar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("RepeatButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Viewbox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ToolBar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("ToolBarTray")).ShouldBe(true));

    // The S087 headline: create("Button") etc. yields the CONCRETE type, not a
    // generic blob — the typeName -> type dispatch is real end-to-end.
    assertConcrete<Button>(g.create("Button"));
    assertConcrete<StackPanel>(g.create("StackPanel"));
    assertConcrete<Grid>(g.create("Grid"));
    // 11k-1: TextBlock resolves (the S087-invented Text name is retired).
    assertConcrete<TextBlock>(g.create("TextBlock"));
    assertConcrete<TextBox>(g.create("TextBox"));
    assertConcrete<CheckBox>(g.create("CheckBox"));
    assertConcrete<Slider>(g.create("Slider"));
    assertConcrete<ScrollViewer>(g.create("ScrollViewer"));
    assertConcrete<Window>(g.create("Window"));
    // S134: the registry-resolve assertion is authored at THIS step (the
    // element was registered here, not silently backfilled later).
    assertConcrete<WrapPanel>(g.create("WrapPanel"));
    // S136: the registry-resolve assertion is authored at THIS step.
    assertConcrete<Canvas>(g.create("Canvas"));
    // S400_1: the registry-resolve assertion is authored at THIS step.
    assertConcrete<Icon>(g.create("Icon"));
    // Sprint 11c: registry-resolve assertions for the new controls.
    assertConcrete<vaml::DockPanel>(g.create("DockPanel"));
    assertConcrete<vaml::RadioButton>(g.create("RadioButton"));
    assertConcrete<vaml::ComboBox>(g.create("ComboBox"));
    assertConcrete<vaml::ListBox>(g.create("ListBox"));
    assertConcrete<vaml::ListView>(g.create("ListView"));
    // Sprint 11d: registry-resolve assertions for the new controls.
    assertConcrete<vaml::Label>(g.create("Label"));
    assertConcrete<vaml::Border>(g.create("Border"));
    assertConcrete<vaml::Rectangle>(g.create("Rectangle"));
    assertConcrete<vaml::Ellipse>(g.create("Ellipse"));
    assertConcrete<vaml::Line>(g.create("Line"));
    assertConcrete<vaml::Image>(g.create("Image"));
    assertConcrete<vaml::ProgressBar>(g.create("ProgressBar"));
    assertConcrete<vaml::PasswordBox>(g.create("PasswordBox"));
    assertConcrete<vaml::GroupBox>(g.create("GroupBox"));
    assertConcrete<vaml::Expander>(g.create("Expander"));
    assertConcrete<vaml::Separator>(g.create("Separator"));
    assertConcrete<vaml::StatusBar>(g.create("StatusBar"));
    // 11k-2: registry-resolve assertions for the new controls.
    assertConcrete<vaml::ScrollBar>(g.create("ScrollBar"));
    assertConcrete<vaml::RepeatButton>(g.create("RepeatButton"));
    assertConcrete<vaml::Viewbox>(g.create("Viewbox"));
    assertConcrete<vaml::ToolBar>(g.create("ToolBar"));
    assertConcrete<vaml::ToolBarTray>(g.create("ToolBarTray"));
    // Sprint 11j (GAP CLOSE): UserControl (11j-1) + Application (11j-6).
    assertConcrete<vaml::UserControl>(g.create("UserControl"));
    assertConcrete<vaml::Application>(g.create("Application"));
    // 11j-2: the ResourceDictionary root (a DependencyObject HOLDER).
    assertConcrete<vaml::ResourceDictionaryHolder>(g.create("ResourceDictionary"));

    // Size matches the full built-in set (36: the roadmap controls + WrapPanel +
    // Canvas + Icon + the S453 authored-resource types SolidColorBrush + Style +
    // the S460 brush-authoring types LinearGradientBrush + GradientStop + the
    // Sprint 11c controls DockPanel/RadioButton/ComboBox/ListBox/ListView + the
    // Sprint 11d controls Label/Border/Rectangle/Ellipse/Line/Image/ProgressBar/
    // PasswordBox/GroupBox/Expander/Separator/StatusBar + the Sprint 11j
    // GAP-CLOSE UserControl + Application + ResourceDictionaryHolder + the
    // 11k-2 control set ScrollBar/RepeatButton/Viewbox/ToolBar/ToolBarTray +
    // the 11k-4 brush/vector resource set PathGeometry/GeometryGroup/
    // RectangleGeometry/EllipseGeometry/GeometryDrawing/DrawingGroup/
    // DrawingBrush/DrawingImage/ImageBrush + the Sprint 11e (11e) container +
    // menu controls TabControl/TabItem/Menu/MenuItem/ContextMenu/ToolTip/Popup/
    // GridSplitter/DataGrid (50 -> 59; count bumps are authored at the step,
    // never silent).
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(59u));
}

TEST_CASE("S087: register_builtin_elements is idempotent (second call is a no-op, not a throw)")
{
    register_builtin_elements();
    register_builtin_elements(); // second population must NOT throw
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    // Exactly the built-in set, no duplicates (a duplicate would have thrown or
    // inflated size). S134: WrapPanel the 10th; S136: Canvas the 11th; S400_1:
    // Icon the 12th; S453: SolidColorBrush + Style the 13th/14th; S460:
    // LinearGradientBrush + GradientStop the 15th/16th; Sprint 11c: DockPanel/
    // RadioButton/ComboBox/ListBox/ListView the 17th-21st; Sprint 11d:
    // Label/Border/Rectangle/Ellipse/Line/Image/ProgressBar/PasswordBox/
    // GroupBox/Expander/Separator/StatusBar the 22nd-33rd; Sprint 11j:
    // UserControl + Application + ResourceDictionaryHolder the 34th-36th;
    // the 11k-2 set ScrollBar/RepeatButton/Viewbox/ToolBar/ToolBarTray the
    // 37th-41st + the 11k-4 set PathGeometry/GeometryGroup/RectangleGeometry/
    // EllipseGeometry/GeometryDrawing/DrawingGroup/DrawingBrush/DrawingImage/
    // ImageBrush the 42nd-50th + the Sprint 11e (11e) set TabControl/TabItem/
    // Menu/MenuItem/ContextMenu/ToolTip/Popup/GridSplitter/DataGrid the
    // 51st-59th (count bumps authored at the step, never silent).
    SHOULD(ShouldlyIntegral(g.size()).ShouldBe(59u));
    assertConcrete<Button>(g.create("Button"));
}
