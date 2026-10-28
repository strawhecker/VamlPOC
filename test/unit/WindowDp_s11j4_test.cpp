// 11j-4 (GAP CLOSE, dep:S440) — the WINDOW-OWNER DP set.
//
// S440 seeded Window with NO Window-owner DPs; the external SplashWindow corpus
// file authors AllowsTransparency / WindowStartupLocation / WindowStyle /
// Topmost / ResizeMode, which previously threw UnknownProperty. These are REAL
// window-config DPs (WPF-faithful defaults), resolved S043-attached-aware on
// ANY DependencyObject — the loader only needs them to RESOLVE so SplashWindow
// parses. The HOST maps them to the OS window at a later window-host step
// (recorded NO-GAP, not silent).
//
// Headless (Rail 1) — no window/runtime, no Vulkan.

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"      // Window
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"   // the text load path (name->enum proof)

#include "doctest/doctest.h"

using namespace vaml;

namespace {

void expectInt32(DependencyObject& o, const char* name, std::int32_t want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name);
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name), Value::Int32(want));
    CHECK(o.Get(PropertyId::of(name)).get<std::int32_t>() == want);
}

void expectBool(DependencyObject& o, const char* name, std::int32_t want)
{
    const DependencyProperty* dp = DependencyProperty::TryGet(name);
    REQUIRE(dp != nullptr);
    (void)o.Set(PropertyId::of(name), Value::Bool(want != 0));
    CHECK(o.Get(PropertyId::of(name)).get<bool>() == (want != 0));
}

} // namespace

TEST_CASE("11j-4: the Window-owner DPs resolve UNPREFIXED on a plain DependencyObject (S043 attached)")
{
    Window window; // triggers ensure_window_properties_registered
    DependencyObject o;

    // AllowsTransparency: BOOL (WPF-faithful), default false.
    expectBool(o, "AllowsTransparency", 1);
    // WindowStartupLocation: {Manual=0, CenterScreen=1, CenterOwner=2}, default 0.
    expectInt32(o, "WindowStartupLocation", 1); // CenterScreen
    // WindowStyle: {None=0, SingleBorderWindow=1, ThreeDBorderWindow=2, ToolWindow=3}, default 1.
    expectInt32(o, "WindowStyle", 0);           // None
    // Topmost: BOOL (WPF-faithful), default false.
    expectBool(o, "Topmost", 1);
    // ResizeMode: {NoResize=0, CanMinimize=1, CanResize=2, CanResizeWithGrip=3}, default 2.
    expectInt32(o, "ResizeMode", 3);            // CanResizeWithGrip
}

TEST_CASE("11j-4: the Window-owner DPs carry the WPF-faithful DEFAULTS")
{
    Window window;
    const DependencyProperty* p = DependencyProperty::TryGet("WindowStartupLocation");
    REQUIRE(p != nullptr);
    CHECK(window.Get(PropertyId::of("AllowsTransparency")).get<bool>() == false);
    CHECK(window.Get(PropertyId::of("WindowStartupLocation")).get<std::int32_t>() == 0); // Manual
    CHECK(window.Get(PropertyId::of("WindowStyle")).get<std::int32_t>() == 1);           // SingleBorderWindow
    CHECK(window.Get(PropertyId::of("Topmost")).get<bool>() == false);
    CHECK(window.Get(PropertyId::of("ResizeMode")).get<std::int32_t>() == 2);            // CanResize
}

TEST_CASE("11j-4: the Window enum NAMES convert (Bool for true/false, Int32 enums for others)")
{
    // 11j-4 adds the Window enum-name surface so corpus authoring
    // `AllowsTransparency="true"` (Bool) + `WindowStartupLocation="CenterScreen"`
    // / `WindowStyle="None"` / `ResizeMode="CanResizeWithGrip"` (Int32 enums)
    // parses — a SplashWindow-shaped root.
    VamlReader reader;
    auto root = reader.Load(
        "<Window AllowsTransparency=\"true\" WindowStartupLocation=\"CenterScreen\" "
        "WindowStyle=\"None\" Topmost=\"true\" ResizeMode=\"CanResizeWithGrip\"/>");
    REQUIRE(root != nullptr);
    CHECK(dynamic_cast<Window*>(root.get()) != nullptr);
}
