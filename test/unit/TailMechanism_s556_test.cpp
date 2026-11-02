// S556 (Sprint 11m) — SERVICE / MECHANISM + U+ support types (the DAG
// `PLANNED` data-mechanism rows). CONSUMED BY the DONE surfaces (the S242/S501
// converter seam + binding glue); ZERO new MINIMAL REAL. Each type is REAL +
// tested for resolution + data semantics. Rendered through the DONE controls
// (no standalone behavior token).

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>          // Value
#include <vaml/core/TailMechanism.h>    // S556 — the mechanism + converter types
#include <vaml/core/Value.h>

#include <memory>
#include <string>
#include <vector>

using namespace vaml;

TEST_CASE("S556: the framework converters implement the S501 IValueConverter seam")
{
    AlternationConverter alt;
    alt.add_value(Value::String("A"));
    alt.add_value(Value::String("B"));
    BindingConversion r0 = alt.Convert(Value::Int32(0));
    BindingConversion r1 = alt.Convert(Value::Int32(1));
    SHOULD(ShouldlyIntegral(r0.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r0.value.get<std::string>()).ShouldBe("A"));
    SHOULD(ShouldlyString(r1.value.get<std::string>()).ShouldBe("B"));

    BooleanToSelectiveScrollingOrientationConverter sel;
    BindingConversion s1 = sel.Convert(Value::Bool(true));
    SHOULD(ShouldlyIntegral(s1.ok).ShouldBe(true));
    SHOULD(ShouldlyIntegral(s1.value.get<int>() == 1).ShouldBe(true));

    MenuScrollingVisibilityConverter menu;
    BindingConversion mv = menu.Convert(Value::Bool(true));
    SHOULD(ShouldlyIntegral(mv.ok).ShouldBe(true));
    SHOULD(ShouldlyIntegral(mv.value.get<int>() == 0).ShouldBe(true));

    DataGridHeadersVisibilityToVisibilityConverter hdr;
    BindingConversion hv = hdr.Convert(Value::Int32(2));
    SHOULD(ShouldlyIntegral(hv.ok).ShouldBe(true));
    SHOULD(ShouldlyIntegral(hv.value.get<int>() == 0).ShouldBe(true));

    BorderGapMaskConverter gap;
    BindingConversion gv = gap.Convert(Value::Int32(3));
    SHOULD(ShouldlyIntegral(gv.ok).ShouldBe(true));
}

TEST_CASE("S556: the validation family is real + fail-loud on data errors")
{
    ExceptionValidationRule exceptionRule;
    ValidationResult ex = exceptionRule.Validate(Value::Int32(5));
    SHOULD(ShouldlyIntegral(ex.is_valid()).ShouldBe(true));

    DataErrorValidationRule dataRule;
    ValidationResult bad = dataRule.Validate(Value::Null());
    SHOULD(ShouldlyIntegral(bad.is_valid()).ShouldBe(false));
    ValidationResult good = dataRule.Validate(Value::Int32(5));
    SHOULD(ShouldlyIntegral(good.is_valid()).ShouldBe(true));

    NotifyDataErrorValidationRule notifyRule;
    ValidationResult nbad = notifyRule.Validate(Value::Null());
    SHOULD(ShouldlyIntegral(nbad.is_valid()).ShouldBe(false));

    Validation::set_has_error(true);
    Validation::add_error(ValidationError("bad input"));
    SHOULD(ShouldlyIntegral(Validation::has_error()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(Validation::errors().size()).ShouldBe(1u));
    SHOULD(ShouldlyString(Validation::errors()[0].message()).ShouldBe("bad input"));
}

TEST_CASE("S556: the item-container + collection mechanism surfaces are real")
{
    ItemContainerGenerator gen;
    gen.add(std::make_shared<int>(42));
    auto* p = static_cast<int*>(gen.generator_for_index(0));
    SHOULD(ShouldlyIntegral(p != nullptr).ShouldBe(true));
    gen.recycle(nullptr);
    IItemContainerGenerator& iface = gen;
    (void)iface;

    ItemCollection items;
    items.add(Value::Int32(1));
    items.add(Value::Int32(2));
    SHOULD(ShouldlyIntegral(items.count()).ShouldBe(2u));

    SelectedItemCollection sel;
    sel.add(Value::String("a"));
    SHOULD(ShouldlyIntegral(sel.count()).ShouldBe(1u));

    SelectedCellsCollection cells;
    cells.add(0, 0);
    cells.add(1, 2);
    SHOULD(ShouldlyIntegral(cells.count()).ShouldBe(2u));

    UIElementCollection uec;
    auto el = std::make_shared<int>(7);
    uec.add(el);
    SHOULD(ShouldlyIntegral(uec.contains(el)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(uec.count()).ShouldBe(1u));

    ScrollInfo scroll;
    scroll.set_offset(5.0, 10.0);
    IScrollInfo& si = scroll;
    SHOULD(ShouldlyIntegral(si.offset_x() == 5.0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(si.offset_y() == 10.0).ShouldBe(true));
}

TEST_CASE("S556: the service facades carry their real data")
{
    TextSearch search;
    search.set_text("query");
    SHOULD(ShouldlyString(search.text()).ShouldBe("query"));

    SpellCheck spell;
    spell.set_enabled(true);
    SHOULD(ShouldlyIntegral(spell.enabled()).ShouldBe(true));

    ToolTipService tips;
    tips.set_tool_tip("A tip");
    SHOULD(ShouldlyString(tips.tool_tip()).ShouldBe("A tip"));

    ContextMenuService ctx;
    ctx.set_menu("menu");
    SHOULD(ShouldlyString(ctx.menu()).ShouldBe("menu"));

    PopupControlService popup;
    popup.set_popup("pop");
    SHOULD(ShouldlyString(popup.popup()).ShouldBe("pop"));
}
