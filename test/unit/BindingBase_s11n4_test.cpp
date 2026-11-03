// 11n-4 (main, step 1) — the BindingBase recipe family (vaml::core).
//
// The dependency-root of the 11n-4 reactive binding surface:
//   MarkupExtension (base, DAG anchor) + BindingBase (
//   WPF BindingBase : MarkupExtension) + the three concrete recipes:
//   Binding / MultiBinding / PriorityBinding. REUSES the existing S240-S245
//   BindingExpression engine (no duplicate path).
//
// Proves:
//   (A) BindingBase IS-A MarkupExtension (the DAG anchor) — a Binding is-a
//       MarkupExtension.
//   (B) Binding: set_path/source/target -> make_spec() carries mode/converter/
//       fallback/policy into a BindingExpressionSpec (the engine input).
//   (C) MultiBinding holds ordered child Bindings + an IMultiValueConverter
//       that combines them into ONE Value (convert(values)).
//   (D) PriorityBinding holds an ORDERED candidate list (the fallback ladder).
//
// ADR-018: ZERO new locks (plain configured recipe objects, read on the
// affinity thread).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/BindingBase.h>
#include <vaml/core/ItemsControl.h>
#include <vaml/core/ObservableCollection.h>
#include <vaml/core/Value.h>

#include <string>

using vaml::Binding;
using vaml::BindingBase;
using vaml::BindingExpressionSpec;
using vaml::BindingMode;
using vaml::ItemsControl;
using vaml::MarkupExtension;
using vaml::MultiBinding;
using vaml::ObservableCollection;
using vaml::PriorityBinding;
using vaml::Value;

namespace {
// An IMultiValueConverter that concatenates string values.
struct ConcatMulti : public vaml::IMultiValueConverter
{
    vaml::Value convert(const std::vector<vaml::Value>& values) const override
    {
        std::string out;
        for (const auto& v : values)
        {
            if (v.type() == vaml::ValueType::String)
                out += v.get<std::string>();
            else
                out += "?";
        }
        return vaml::Value::String(out);
    }
};
} // namespace

// ---------------------------------------------------------------------------
// A. BindingBase IS-A MarkupExtension (the DAG anchor)
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: BindingBase IS-A MarkupExtension (the recipe family unifies)")
{
    Binding b("Name");
    MarkupExtension* asBase = &b;
    SHOULD(ShouldlyPointer(asBase).ShouldNotBeNull());
    // the shared base is a BindingBase
    BindingBase* asBindingBase = &b;
    SHOULD(ShouldlyPointer(asBindingBase).ShouldNotBeNull());
}

// ---------------------------------------------------------------------------
// B. Binding -> make_spec carries the engine input
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: Binding recipe -> BindingExpressionSpec (engine input)")
{
    Binding b("Person.Name");
    b.set_mode(BindingMode::OneWay);
    b.set_fallback(Value::String("n/a"));
    auto spec = b.make_spec();
    SHOULD(ShouldlyString(spec.sourcePath).ShouldBe(std::string("Person.Name")));
    SHOULD(ShouldlyIntegral(static_cast<int>(spec.mode)).ShouldBe(static_cast<int>(BindingMode::OneWay)));
    SHOULD(ShouldlyIntegral(spec.fallback.has_value() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(spec.fallback->get<std::string>()).ShouldBe(std::string("n/a")));
}

// ---------------------------------------------------------------------------
// C. MultiBinding: ordered children + IMultiValueConverter
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: MultiBinding combines children via IMultiValueConverter")
{
    MultiBinding mb;
    auto a = std::make_unique<Binding>("First");
    auto bb = std::make_unique<Binding>("Last");
    mb.add_binding(std::move(a));
    mb.add_binding(std::move(bb));
    SHOULD(ShouldlyIntegral(static_cast<int>(mb.binding_count())).ShouldBe(2));

    // child path order is preserved
    SHOULD(ShouldlyString(mb.binding_at(0)->path()).ShouldBe(std::string("First")));
    SHOULD(ShouldlyString(mb.binding_at(1)->path()).ShouldBe(std::string("Last")));

    // the converter combines the children into ONE value
    ConcatMulti conv;
    mb.set_multi_converter(&conv);
    const Value combined = mb.provide_value();
    // children provide_value() returns Null (they are not scalar value-
    // extensions); the converter folds them -> "??".
    SHOULD(ShouldlyIntegral(combined.type() == vaml::ValueType::String ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyString(combined.get<std::string>()).ShouldBe(std::string("??")));
}

// ---------------------------------------------------------------------------
// D. PriorityBinding: the ordered fallback ladder
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: PriorityBinding holds the ordered candidate list")
{
    PriorityBinding pb;
    auto first = std::make_unique<Binding>("High");
    auto second = std::make_unique<Binding>("Low");
    pb.add_binding(std::move(first));
    pb.add_binding(std::move(second));
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.binding_count())).ShouldBe(2));
    // priority order preserved
    SHOULD(ShouldlyString(pb.binding_at(0)->path()).ShouldBe(std::string("High")));
    SHOULD(ShouldlyString(pb.binding_at(1)->path()).ShouldBe(std::string("Low")));
}

// ---------------------------------------------------------------------------
// REACTIVE LIVE (the roadmap test): mutate the ObservableCollection -> the
// bound ItemsControl reflects the change live, via the event-subscription
// reactive spine (CollectionChanged -> incremental containers).
// ---------------------------------------------------------------------------

TEST_CASE("11n-4 (reactive live): a bound ItemsControl reflects ObservableCollection mutations live")
{
    ObservableCollection<std::string> items;
    ItemsControl ic;
    ic.set_items_observable(items.base());
    SHOULD(ShouldlyIntegral(ic.bound() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(0));

    // Mutate the ObservableCollection -> the bound ItemsControl reflects it live.
    items.Add("alpha");
    items.Add("beta");
    items.Add("gamma");
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(3));

    items.Remove("beta");
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(2));

    items.Clear(); // Reset
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(0));

    items.Add("delta");
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(1));
}
