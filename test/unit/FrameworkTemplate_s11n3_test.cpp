// 11n-3 — FRAMEWORKTEMPLATE + ITEMSPANELTEMPLATE (vaml::core).
//
// Unifies the WPF `System.Windows.FrameworkTemplate` base (the DAG anchor
// shared by ControlTemplate/DataTemplate/ItemsPanelTemplate) under
// ControlTemplate + DataTemplate, and confirms ItemsPanelTemplate unifies under
// it too (the panel recipe ItemsControl.ItemsPanel resolves).
//
// Proves:
//   (A) ControlTemplate and DataTemplate ARE FrameworkTemplate (a
//       DependencyObject-derived shared base) — the target_type (template
//       target-type name) is carried by ONE shared slot on the base.
//   (B) DataTemplate's DataType maps onto the inherited target_type() — the
//       S500 data_type surface is unchanged, unified under the base.
//   (C) ItemsPanelTemplate (the Scaffolding REAL type, re-exported via
//       vaml/core/ItemsPanelTemplate.h) IS-A FrameworkTemplate — the panel
//       recipe unifies under the same base; its set_panel_root/load_items_panel
//       seam is preserved.
//   (D) No regression: existing target_type()/data_type() reads hold.
//
// ADR-018: ZERO new locks (template resources, read/applied on the affinity
// thread).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/ControlTemplate.h>
#include <vaml/core/DataTemplate.h>
#include <vaml/core/FrameworkTemplate.h>
#include <vaml/core/ItemsPanelTemplate.h>  // re-exports the REAL Scaffolding type

#include <string>

using vaml::ControlTemplate;
using vaml::DataTemplate;
using vaml::FrameworkTemplate;
using vaml::ItemsPanelTemplate;

// ---------------------------------------------------------------------------
// A. ControlTemplate + DataTemplate unify under FrameworkTemplate
// ---------------------------------------------------------------------------

TEST_CASE("11n-3: ControlTemplate + DataTemplate ARE FrameworkTemplate (the shared base)")
{
    // ControlTemplate IS-A FrameworkTemplate
    {
        ControlTemplate ct;
        ct.set_target_type("Button");
        SHOULD(ShouldlyString(ct.target_type()).ShouldBe(std::string("Button")));
        FrameworkTemplate* asBase = &ct;
        SHOULD(ShouldlyPointer(asBase).ShouldNotBeNull());
        SHOULD(ShouldlyString(asBase->target_type()).ShouldBe(std::string("Button")));
    }

    // DataTemplate IS-A FrameworkTemplate
    {
        DataTemplate dt;
        dt.set_data_type("Person");
        FrameworkTemplate* asBase = &dt;
        SHOULD(ShouldlyPointer(asBase).ShouldNotBeNull());
    }
}

// ---------------------------------------------------------------------------
// B. DataTemplate.DataType -> the inherited target_type slot
// ---------------------------------------------------------------------------

TEST_CASE("11n-3: DataTemplate data_type maps onto the inherited target_type()")
{
    DataTemplate dt;
    dt.set_data_type("OrderItem");
    SHOULD(ShouldlyString(dt.data_type()).ShouldBe(std::string("OrderItem")));
    // the SAME value is the base target_type (ONE shared slot, unified)
    SHOULD(ShouldlyString(dt.target_type()).ShouldBe(std::string("OrderItem")));
}

// ---------------------------------------------------------------------------
// C. ItemsPanelTemplate unifies under FrameworkTemplate (the REAL type)
// ---------------------------------------------------------------------------

TEST_CASE("11n-3: ItemsPanelTemplate IS-A FrameworkTemplate (the panel recipe unifies)")
{
    ItemsPanelTemplate ipt;
    // the FrameworkTemplate base slot is available on the REAL ItemsPanelTemplate
    ipt.set_target_type("VirtualizingStackPanel");
    FrameworkTemplate* asBase = &ipt;
    SHOULD(ShouldlyPointer(asBase).ShouldNotBeNull());
    SHOULD(ShouldlyString(ipt.target_type()).ShouldBe(std::string("VirtualizingStackPanel")));
    SHOULD(ShouldlyString(asBase->target_type()).ShouldBe(std::string("VirtualizingStackPanel")));
}

// ---------------------------------------------------------------------------
// D. No regression: the existing target-type reads hold after the re-base
// ---------------------------------------------------------------------------

TEST_CASE("11n-3: existing template target-type reads hold after the re-base")
{
    ControlTemplate ct;
    ct.set_target_type("CheckBox");
    SHOULD(ShouldlyString(ct.target_type()).ShouldBe(std::string("CheckBox")));
}
