// 11j-6 (GAP CLOSE, dep:S450_1) — the `Application` document root element.
//
// RESOLVES the canonical WPF app entry `<Application ... StartupUri="...">`,
// which previously threw `unknown element 'Application'`. Application is a REAL,
// permanent, loadable no-op content shell — a DependencyObject (NOT a
// UIElement; WPF Application is not a FrameworkElement), carrying a String
// StartupUri DP for the most-common `<Application StartupUri="...">` case.
//
// RECORDED NO-GAP (not silent): StartupUri is a record-only DP at this step —
// the NAVIGATION (actually opening the referenced window) is a later app-host
// concern; the loader only needs it to PARSE the app root.
//
// Headless (Rail 1) — no window/runtime, no Vulkan.

#include "vaml/core/Element.h"     // Application / DependencyObject
#include "vaml/core/ElementFactoryRegistry.h" // S087
#include "vaml/core/VamlReader.h"  // text load path
#include "vaml/core/Value.h"

#include "doctest/doctest.h"

#include <memory>
#include <string>

using namespace vaml;

TEST_CASE("11j-6: Application is a DependencyObject (NOT a UIElement), WPF-faithful")
{
    Application app;
    CHECK(dynamic_cast<DependencyObject*>(&app) != nullptr);
    // WPF: Application is not a FrameworkElement — our faithful mapping is a
    // DependencyObject, never a UIElement.
    CHECK(dynamic_cast<UIElement*>(&app) == nullptr);
}

TEST_CASE("11j-6: Application resolves through the factory by name")
{
    register_builtin_elements();
    ElementFactoryRegistry& factory = ElementFactoryRegistry::global();
    auto created = factory.create("Application");
    REQUIRE(created != nullptr);
    CHECK(dynamic_cast<Application*>(created.get()) != nullptr);
    // It must not accidentally be a UIElement either.
    CHECK(dynamic_cast<UIElement*>(created.get()) == nullptr);
}

TEST_CASE("11j-6: Application startup_uri DP round-trips (String, default \"\")")
{
    Application app;
    CHECK(app.startup_uri().empty()); // default ""
    auto res = app.set_startup_uri("MainWindow.xaml");
    REQUIRE(res == SetResult::APPLIED);
    CHECK(app.startup_uri() == "MainWindow.xaml");
}

TEST_CASE("11j-6: <Application StartupUri=...> loads as a root (no throw)")
{
    // The canonical entry — text path.
    VamlReader reader;
    auto root = reader.Load("<Application StartupUri=\"MainWindow.xaml\"/>");
    REQUIRE(root != nullptr);
    CHECK(dynamic_cast<Application*>(root.get()) != nullptr);
    Application* app = dynamic_cast<Application*>(root.get());
    CHECK(app->startup_uri() == "MainWindow.xaml");
}
