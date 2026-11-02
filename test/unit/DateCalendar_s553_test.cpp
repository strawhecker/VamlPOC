// S553 (Sprint 11m) — DATE & CALENDAR CONTROLS (dep:S552).
//
// DatePicker (+ DatePickerTextBox) + Calendar + the Calendar family
// (CalendarDayButton/CalendarButton/CalendarItem) + the pure date-range data
// types (CalendarDateRange/CalendarBlackoutDatesCollection) + the routed
// SelectedDatesChanged/SelectedDateChanged identities. Real + tested. ZERO
// MINIMAL REAL.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/Element.h>
#include <vaml/core/ElementFactoryRegistry.h>
#include <vaml/core/RoutedEvent.h>
#include <vaml/core/UIElement.h>
#include <vaml/core/VamlReader.h>
#include <vaml/presentation/VamlRenderer.h>
#include <vaml/vulkan/ui/UiScene.h>

#include <memory>
#include <string>

using namespace vaml;
using vaml::register_builtin_elements;
using vaml::presentation::classify_merge_element;
using vaml::vulkan::ui::MaterialKind;

TEST_CASE("S553: the date/calendar controls are registered + resolvable")
{
    register_builtin_elements();
    ElementFactoryRegistry& g = ElementFactoryRegistry::global();
    SHOULD(ShouldlyIntegral(g.is_registered("DatePicker")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("DatePickerTextBox")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("Calendar")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarDayButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarButton")).ShouldBe(true));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarItem")).ShouldBe(true));
    // The pure date-range data types are NOT authorable factory elements.
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarDateRange")).ShouldBe(false));
    SHOULD(ShouldlyIntegral(g.is_registered("CalendarBlackoutDatesCollection")).ShouldBe(false));
}

TEST_CASE("S553: DatePicker + DatePickerTextBox carry the real date surface")
{
    DatePicker picker;
    (void)picker.set_selected_date(1900000000000ll);
    SHOULD(ShouldlyIntegral(picker.selected_date() == 1900000000000ll).ShouldBe(true));
    (void)picker.set_display_date(1900000000000ll);
    SHOULD(ShouldlyIntegral(picker.display_date() == 1900000000000ll).ShouldBe(true));
    (void)picker.set_is_drop_down_open(true);
    SHOULD(ShouldlyIntegral(picker.is_drop_down_open()).ShouldBe(true));
    picker.Measure(Size{ 160.0f, 30.0f });
    SHOULD(ShouldlyIntegral(picker.desired_size().width > 0.0f).ShouldBe(true));

    DatePickerTextBox box;
    (void)box.set_text("2025-01-15");
    SHOULD(ShouldlyString(box.text()).ShouldBe("2025-01-15"));
    (void)box.set_selected_date(1900000000000ll);
    SHOULD(ShouldlyIntegral(box.selected_date() == 1900000000000ll).ShouldBe(true));
}

TEST_CASE("S553: Calendar + the family carry the real calendar surface")
{
    Calendar cal;
    (void)cal.set_display_date(1900000000000ll);
    (void)cal.set_selected_date(1900000000000ll + 864000000000ll);
    SHOULD(ShouldlyIntegral(cal.selected_date() > 0ll).ShouldBe(true));
    (void)cal.set_first_day_of_week(1);
    SHOULD(ShouldlyIntegral(cal.first_day_of_week()).ShouldBe(1));
    (void)cal.set_selection_mode(1);
    SHOULD(ShouldlyIntegral(cal.selection_mode()).ShouldBe(1));

    // The real blackout-date collection + a date range.
    CalendarBlackoutDatesCollection blackout;
    blackout.add(CalendarDateRange(100ll, 200ll));
    blackout.add(CalendarDateRange(500ll));
    SHOULD(ShouldlyIntegral(blackout.size()).ShouldBe(2u));
    cal.set_blackout_dates(blackout);
    SHOULD(ShouldlyIntegral(cal.blackout_dates().size()).ShouldBe(2u));
    SHOULD(ShouldlyIntegral(cal.blackout_dates().ranges()[0].start() == 100ll).ShouldBe(true));
    cal.Measure(Size{ 260.0f, 220.0f });
    SHOULD(ShouldlyIntegral(cal.desired_size().width > 0.0f).ShouldBe(true));

    CalendarItem item;
    (void)item.set_display_mode(0);
    (void)item.set_display_date(1900000000000ll);
    SHOULD(ShouldlyIntegral(item.display_mode()).ShouldBe(0));

    CalendarDayButton day;
    (void)day.set_is_today(true);
    (void)day.set_is_selected(true);
    (void)day.set_day_number(21);
    SHOULD(ShouldlyIntegral(day.is_today()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(day.is_selected()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(day.day_number()).ShouldBe(21));

    CalendarButton month;
    (void)month.set_display_date(1900000000000ll);
    (void)month.set_is_selected(true);
    SHOULD(ShouldlyIntegral(month.is_selected()).ShouldBe(true));
}

TEST_CASE("S553: routed SelectedDatesChanged/SelectedDateChanged resolve")
{
    register_builtin_elements();
    SHOULD(ShouldlyString(std::string(SelectedDatesChangedEvent().name())).ShouldBe("SelectedDatesChanged"));
    SHOULD(ShouldlyString(std::string(SelectedDateChangedEvent().name())).ShouldBe("SelectedDateChanged"));
    SHOULD(ShouldlyIntegral(static_cast<int>(SelectedDateChangedEvent().direction()) ==
                            static_cast<int>(RouteEventDirection::Bubble)).ShouldBe(true));
}

TEST_CASE("S553: the date/calendar controls classify to the material slice")
{
    register_builtin_elements();
    DatePicker picker;
    SHOULD(ShouldlyIntegral(classify_merge_element(picker).kind == MaterialKind::Solid).ShouldBe(true));
    Calendar cal;
    SHOULD(ShouldlyIntegral(classify_merge_element(cal).kind == MaterialKind::Solid).ShouldBe(true));
    CalendarItem item;
    SHOULD(ShouldlyIntegral(classify_merge_element(item).kind == MaterialKind::Solid).ShouldBe(true));
    CalendarDayButton day;
    SHOULD(ShouldlyIntegral(classify_merge_element(day).kind == MaterialKind::Solid).ShouldBe(true));
}

TEST_CASE("S553: an authored date/calendar assembly loads end-to-end")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<DatePicker/>"
        "<Calendar/>"
        "</Window>");
    SHOULD(ShouldlyIntegral(root != nullptr).ShouldBe(true));
    auto* win = dynamic_cast<Window*>(root.get());
    SHOULD(ShouldlyIntegral(win != nullptr).ShouldBe(true));
}
