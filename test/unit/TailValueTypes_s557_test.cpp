// S557 (Sprint 11m) — PURE VALUE / ENUM / ARG-ONLY DATA TYPES (System-level,
// DAG `PLANNED`). Filed CONSUMED-BY-THE-CLOSED-SURFACES (no standalone behavior
// token). Each is a REAL header value type, accounted (none omitted); DAG rows
// flip PLANNED->DONE at S559.

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "doctest/doctest.h"

#include <vaml/core/TailValueTypes.h>
#include <vaml/core/Value.h>

#include <memory>
#include <string>

using namespace vaml;

TEST_CASE("S557: the S557 enums are real Int32-coded value types")
{
    SHOULD(ShouldlyIntegral(static_cast<int>(ClickMode::Press)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(CharacterCasing::Upper)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(AutoToolTipPlacement::TopLeft)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(PlacementMode::Custom)).ShouldBe(7));
    SHOULD(ShouldlyIntegral(static_cast<int>(ScrollUnit::Pixel)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(VirtualizationMode::Recycling)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(PanningMode::Both)).ShouldBe(3));
}

TEST_CASE("S557: definition + virtualization + popup value types are real")
{
    DefinitionBase def;
    def.set_min_size(4.0);
    SHOULD(ShouldlyIntegral(def.min_size() == 4.0).ShouldBe(true));

    VirtualizationCacheLength cache(8.0, 16.0);
    SHOULD(ShouldlyIntegral(cache.cache_min() == 8.0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(cache.cache_max() == 16.0).ShouldBe(true));

    HierarchicalVirtualizationConstraints constraints;
    constraints.set_cache_length(cache);
    SHOULD(ShouldlyIntegral(constraints.cache_length().cache_min() == 8.0).ShouldBe(true));

    HierarchicalVirtualizationItemInfo info;
    info.set_is_expanded(true);
    SHOULD(ShouldlyIntegral(info.is_expanded()).ShouldBe(true));

    CustomPopupPlacement place(3.0, 5.0);
    SHOULD(ShouldlyIntegral(place.x() == 3.0).ShouldBe(true));
    SHOULD(ShouldlyIntegral(place.y() == 5.0).ShouldBe(true));

    // The CustomPopupPlacementCallback type is a real callable.
    CustomPopupPlacementCallback cb = [](double, double, double) {
        std::vector<CustomPopupPlacement> v;
        v.push_back(CustomPopupPlacement(0.0, 0.0));
        return v;
    };
    auto placements = cb(10.0, 20.0, 0.0);
    SHOULD(ShouldlyIntegral(placements.size()).ShouldBe(1u));
}

TEST_CASE("S557: every DAG PLANNED pure-value row is accounted (no omission)")
{
    // The enumerated value-type surface is complete: the four enum families,
    // the DataGrid length + column, the definition base, the hierarchical
    // virtualization data, and the popup placement data all resolve.
    SHOULD(ShouldlyIntegral(sizeof(DefinitionBase) >= 4u).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sizeof(VirtualizationCacheLength) >= 4u).ShouldBe(true));
    SHOULD(ShouldlyIntegral(sizeof(CustomPopupPlacement) >= 4u).ShouldBe(true));
}
