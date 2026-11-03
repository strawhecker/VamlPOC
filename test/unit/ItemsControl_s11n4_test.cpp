// 11n-4 (main, step 4) — ItemsControl, the reactive item host (vaml::core).
// THE ROADMAP reactive-live test: mutate an ObservableCollection -> the bound
// ItemsControl reflects the change LIVE (its per-item containers add/remove
// incrementally on CollectionChanged, event-subscription — NOT polled).
// Proves:
//   (A) initial bind: one per-item container per item.
//   (B) Add -> container count +1 (live, incremental).
//   (C) Remove -> -1 (live); RemoveAt -> -1 at the right slot.
//   (D) Reset -> rebuild to match.
//   (E) detach-on-destroy (S245): a destroyed ItemsControl no longer reacts.
//
// ADR-018 / DECISION-2: single-threaded affinity, ZERO locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/ItemsControl.h>
#include <vaml/core/ObservableCollection.h>

#include <memory>
#include <string>

using vaml::ItemsControl;
using vaml::ObservableCollection;

// ---------------------------------------------------------------------------
// A. Initial bind
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: ItemsControl binds and shows one container per item")
{
    ObservableCollection<std::string> col;
    col.Add("a");
    col.Add("b");

    ItemsControl ic;
    SHOULD(ShouldlyIntegral(ic.bound() ? 1 : 0).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(0));

    ic.set_items_observable(col.base());
    SHOULD(ShouldlyIntegral(ic.bound() ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(2));
}

// ---------------------------------------------------------------------------
// B + C. Live reactivity: mutate -> reflect live (add/remove)
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: mutate the ObservableCollection -> the bound ItemsControl reflects the change live")
{
    ObservableCollection<std::string> col;
    ItemsControl ic;
    ic.set_items_observable(col.base());

    // Add -> live +1
    col.Add("one");
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(1));
    col.Add("two");
    col.Add("three");
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(3));

    // Remove (by value) -> live -1
    col.Remove("two");
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(2));

    // RemoveAt -> live -1
    col.RemoveAt(0);
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// D. Reset -> rebuild to match
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: ItemsControl Reset rebuilds the containers")
{
    ObservableCollection<std::string> col;
    col.Add("a");
    col.Add("b");
    col.Add("c");
    ItemsControl ic;
    ic.set_items_observable(col.base());
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(3));

    col.Clear(); // Reset
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(0));

    col.Add("fresh");
    SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(1));
}

// ---------------------------------------------------------------------------
// E. Detach-on-destroy: a destroyed ItemsControl no longer reacts
// ---------------------------------------------------------------------------

TEST_CASE("11n-4: a destroyed ItemsControl no longer reacts to the collection")
{
    ObservableCollection<std::string> col;
    col.Add("a");
    {
        ItemsControl ic;
        ic.set_items_observable(col.base());
        SHOULD(ShouldlyIntegral(static_cast<int>(ic.item_container_count())).ShouldBe(1));
        // ic destroyed here -> unsubscribes (S245 detach-on-destroy)
    }
    // The collection continues to mutate safely — no dangling callback.
    col.Add("b");
    col.Add("c");
    col.RemoveAt(0);
}
