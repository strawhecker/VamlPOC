// 11j-9 (GAP CLOSE, dep:S131) — `<Grid.ColumnDefinitions>` / `<Grid.RowDefinitions>`
// MARKUP authoring.
//
// S131's Grid track vectors exist in code (set_row_definitions /
// set_column_definitions); the MARKUP `<Grid.ColumnDefinitions><ColumnDefinition
// Width="*"/></Grid.ColumnDefinitions>` previously hit `unknown property
// 'ColumnDefinitions'` (no such DP — RowDefinitions is real member state, NOT a
// DP). 11j-9 wires the property-element surface onto the code-level API: the
// authored RowDefinition/ColumnDefinition children's Width/Height attributes are
// parsed into GridLengths (Auto / numeric Pixel / "N*" Star) and handed to
// grid->set_row_definitions / set_column_definitions.
//
// Headless (Rail 1) — no Vulkan.

#include "vaml/core/Element.h"       // Grid / ColumnDefinition / RowDefinition
#include "vaml/core/GridLength.h"    // GridUnitType / GridLength
#include "vaml/core/VamlReader.h"

#include "doctest/doctest.h"

using namespace vaml;

TEST_CASE("11j-9: <Grid.ColumnDefinitions> authoring produces the correct track vectors")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Grid>"
        "<Grid.ColumnDefinitions>"
        "<ColumnDefinition Width=\"Auto\"/>"
        "<ColumnDefinition Width=\"80\"/>"
        "<ColumnDefinition Width=\"*\"/>"
        "<ColumnDefinition Width=\"2*\"/>"
        "</Grid.ColumnDefinitions>"
        "</Grid>");
    REQUIRE(root != nullptr);
    Grid* grid = dynamic_cast<Grid*>(root.get());
    REQUIRE(grid != nullptr);

    // The reader handed the parsed widths to the code-level API (member state).
    const auto& cols = grid->column_definitions();
    REQUIRE(cols.size() == 4u);
    CHECK(cols[0].width.unitType == GridUnitType::Auto);
    CHECK(cols[1].width.unitType == GridUnitType::Pixel);
    CHECK(cols[1].width.value == doctest::Approx(80.0f));
    CHECK(cols[2].width.unitType == GridUnitType::Star);
    CHECK(cols[2].width.value == doctest::Approx(1.0f));
    CHECK(cols[3].width.unitType == GridUnitType::Star);
    CHECK(cols[3].width.value == doctest::Approx(2.0f));
}

TEST_CASE("11j-9: <Grid.RowDefinitions> authoring parses Auto/Pixel/Star heights")
{
    VamlReader reader;
    auto root = reader.Load(
        "<Grid>"
        "<Grid.RowDefinitions>"
        "<RowDefinition Height=\"Auto\"/>"
        "<RowDefinition Height=\"24\"/>"
        "<RowDefinition Height=\"*\"/>"
        "</Grid.RowDefinitions>"
        "</Grid>");
    REQUIRE(root != nullptr);
    Grid* grid = dynamic_cast<Grid*>(root.get());
    REQUIRE(grid != nullptr);

    const auto& rows = grid->row_definitions();
    REQUIRE(rows.size() == 3u);
    CHECK(rows[0].height.unitType == GridUnitType::Auto);
    CHECK(rows[1].height.unitType == GridUnitType::Pixel);
    CHECK(rows[1].height.value == doctest::Approx(24.0f));
    CHECK(rows[2].height.unitType == GridUnitType::Star);
}

TEST_CASE("11j-9: a malformed track length is a structured error (never silent)")
{
    VamlReader reader;
    bool threw = false;
    try
    {
        auto root = reader.Load(
            "<Grid>"
            "<Grid.ColumnDefinitions><ColumnDefinition Width=\"bogus\"/></Grid.ColumnDefinitions>"
            "</Grid>");
    }
    catch (const ValueConversionException&)
    {
        threw = true;
    }
    CHECK(threw);
}
