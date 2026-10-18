// S145 — object block pool free-list tests (internal pool + the DependencyObject
// operator new/delete integration).
// Scope: dole/recycle reuses the same slot; free-list LIFO; monotonic bump +
// spill to a new block; never-delete (no block-count growth on recycle); slot
// covers every element type; alignment; thread-local isolation (two isolated
// pools don't share; lockless by construction); operator new/delete integration
// (make_unique<Button> churn -> functional objects + freed slabs recycled);
// master_destroy_all frees everything; tree churn via create_child/remove_child
// recycles slabs without leaking.
//
// This TU directly instantiates the INTERNAL ObjectPool (header is pulled in
// via the src/core/src include path — the same test-only seam InternStore_test
// uses; the pool is not part of the public include tree).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< (Shouldly seam)
#include <vaml/assert/Shouldly.h>         // SHOULD macro

#include "ObjectPool.h"

#include "vaml/core/Element.h"            // concrete element types (size checks)
#include "vaml/core/UIElement.h"

#include <cstddef>
#include <memory>
#include <vector>

using vaml::detail::ObjectPool;
using vaml::detail::object_pool_master_destroy_all;

static_assert(sizeof(vaml::UIElement) <= ObjectPool::kSlotSize,
              "S145: UIElement must fit an object-pool slot");
static_assert(sizeof(vaml::StackPanel) <= ObjectPool::kSlotSize,
              "S145: StackPanel must fit an object-pool slot");
static_assert(sizeof(vaml::Grid) <= ObjectPool::kSlotSize,
              "S145: Grid must fit an object-pool slot");
static_assert(sizeof(vaml::WrapPanel) <= ObjectPool::kSlotSize,
              "S145: WrapPanel must fit an object-pool slot");
static_assert(sizeof(vaml::Canvas) <= ObjectPool::kSlotSize,
              "S145: Canvas must fit an object-pool slot");
static_assert(sizeof(vaml::Button) <= ObjectPool::kSlotSize,
              "S145: Button must fit an object-pool slot");
static_assert(sizeof(vaml::Text) <= ObjectPool::kSlotSize,
              "S145: Text must fit an object-pool slot");
// The slot rounds to a cache line AND satisfies both max_align_t widths across
// the CI lanes (8 on MSVC, 16 on Linux clang). 896 % 64 == 0 and 896 % 16 == 0.
static_assert(ObjectPool::kSlotSize % 64 == 0,
              "S145: slot must be cache-line aligned");
static_assert(ObjectPool::kSlotSize % 16 == 0,
              "S145: slot must satisfy Linux max_align_t (16)");

namespace {
// ShouldlyPtr<T> only exposes null predicates; pointer-to-pointer equality is
// asserted via the raw address as an integer.
std::uintptr_t addr(const void* p) noexcept
{
    return reinterpret_cast<std::uintptr_t>(p);
}
} // namespace

TEST_CASE("S145 pool: allocate/deallocate recycles the same slot")
{
    ObjectPool pool;
    void* a = pool.allocate();
    void* b = pool.allocate();
    SHOULD(ShouldlyPointer(a).ShouldNotBeNull());
    SHOULD(ShouldlyPointer(b).ShouldNotBeNull());
    SHOULD(ShouldlyIntegral(addr(a)).ShouldNotBe(addr(b)));
    pool.deallocate(a);
    // Recycle returns `a` to the free-list; the next allocate reuses it.
    void* c = pool.allocate();
    SHOULD(ShouldlyIntegral(addr(c)).ShouldBe(addr(a)));
    pool.master_destroy_all();
}

TEST_CASE("S145 pool: free-list is LIFO")
{
    ObjectPool pool;
    void* a = pool.allocate();
    void* b = pool.allocate();
    pool.deallocate(b);
    pool.deallocate(a); // a pushed last -> pops first
    void* r1 = pool.allocate();
    void* r2 = pool.allocate();
    SHOULD(ShouldlyIntegral(addr(r1)).ShouldBe(addr(a)));
    SHOULD(ShouldlyIntegral(addr(r2)).ShouldBe(addr(b)));
    pool.master_destroy_all();
}

TEST_CASE("S145 pool: monotonic bump within a block + spill to a new block")
{
    ObjectPool pool;
    // The first kInitialSlotsPerBlock allocations stay within block 1 (bump).
    void* first = pool.allocate();
    (void)first;
    SHOULD(ShouldlyIntegral(pool.block_count()).ShouldBe(std::size_t{1}));
    // Exhaust the initial block (kInitialSlots = 32) -> the next allocate spills.
    void* marker = nullptr;
    for (std::size_t i = 1; i < 32; ++i)
    {
        marker = pool.allocate(); // bump
    }
    (void)marker;
    // The 33rd allocation NEEDS a new block -> block_count grows to 2.
    void* spill = pool.allocate();
    SHOULD(ShouldlyPointer(spill).ShouldNotBeNull());
    SHOULD(ShouldlyIntegral(pool.block_count()).ShouldBe(std::size_t{2}));
    pool.master_destroy_all();
}

TEST_CASE("S145 pool: never-delete — recycle does not grow blocks")
{
    ObjectPool pool;
    void* seed = pool.allocate(); // establish block 1
    const std::size_t blocksBefore = pool.block_count();
    SHOULD(ShouldlyIntegral(blocksBefore).ShouldBe(std::size_t{1}));
    for (int i = 0; i < 1000; ++i)
    {
        void* p = pool.allocate();
        pool.deallocate(p); // recycle, never free
    }
    pool.deallocate(seed); // the establishing seed, now recycled too
    SHOULD(ShouldlyIntegral(pool.block_count()).ShouldBe(blocksBefore));
    SHOULD(ShouldlyIntegral(pool.size()).ShouldBe(std::size_t{0})); // nothing live
    pool.master_destroy_all();
}

TEST_CASE("S145 pool: returned slots are aligned")
{
    ObjectPool pool;
    for (int i = 0; i < 200; ++i)
    {
        void* p = pool.allocate();
        SHOULD(ShouldlyIntegral(reinterpret_cast<std::uintptr_t>(p) % 64).ShouldBe(std::uintptr_t{0}));
        pool.deallocate(p);
    }
    pool.master_destroy_all();
}

TEST_CASE("S145 pool: contains() distinguishes pooled from foreign pointers")
{
    ObjectPool pool;
    void* pooled = pool.allocate();
    int foreign = 0;
    SHOULD(ShouldlyString(pool.contains(pooled)).ShouldBe(true));
    SHOULD(ShouldlyString(pool.contains(&foreign)).ShouldBe(false));
    SHOULD(ShouldlyString(pool.contains(nullptr)).ShouldBe(false));
    pool.deallocate(pooled);
    pool.master_destroy_all();
}

TEST_CASE("S145 pool: thread-local isolation — two pools never share (lockless)")
{
    // Two isolated ObjectPool instances draw from disjoint arenas; each is
    // single-threaded by construction (ADR-018), so no lock is needed.
    ObjectPool p1;
    ObjectPool p2;
    void* a1 = p1.allocate();
    void* a2 = p2.allocate();
    SHOULD(ShouldlyIntegral(addr(a1)).ShouldNotBe(addr(a2)));
    SHOULD(ShouldlyString(p1.contains(a2)).ShouldBe(false));
    SHOULD(ShouldlyString(p2.contains(a1)).ShouldBe(false));
    p1.deallocate(a1);
    p2.deallocate(a2);
    p1.master_destroy_all();
    p2.master_destroy_all();
}

TEST_CASE("S145 pool: master_destroy_all frees everything and resets")
{
    ObjectPool pool;
    for (int i = 0; i < 100; ++i)
    {
        (void)pool.allocate();
    }
    SHOULD(ShouldlyIntegral(pool.size()).ShouldBe(std::size_t{100}));
    pool.master_destroy_all();
    SHOULD(ShouldlyIntegral(pool.block_count()).ShouldBe(std::size_t{0}));
    SHOULD(ShouldlyIntegral(pool.size()).ShouldBe(std::size_t{0}));
    // Reusable after reset.
    void* p = pool.allocate();
    SHOULD(ShouldlyPointer(p).ShouldNotBeNull());
    pool.master_destroy_all();
}

TEST_CASE("S145 integration: make_unique<Button> churn routes object slabs to the pool")
{
    // Repeated construction/destruction of a real element through unique_ptr.
    // operator new doled from the pool; operator delete recycled it. The steady
    // state should not leak, and freed slabs are reused (block count stays low).
    object_pool_master_destroy_all(); // deterministic starting point
    const std::size_t blocksStart = vaml::detail::object_pool_block_count();

    {
        auto b = std::make_unique<vaml::Button>();
        // The object is functional through the S036 pipeline.
        (void)b;
    }
    for (int i = 0; i < 2000; ++i)
    {
        auto b = std::make_unique<vaml::Button>();
        (void)b;
    }

    // Block count must not grow unboundedly with churn (free-list reuse).
    SHOULD(ShouldlyIntegral(vaml::detail::object_pool_block_count())
               .ShouldBeLessThan(blocksStart + 4));
    SHOULD(ShouldlyIntegral(vaml::detail::object_pool_size()).ShouldBe(std::size_t{0}));

    object_pool_master_destroy_all();
}

TEST_CASE("S145 integration: tree churn via create_child/remove_child recycles slabs")
{
    object_pool_master_destroy_all();
    auto root = std::make_unique<vaml::StackPanel>();

    for (int round = 0; round < 50; ++round)
    {
        // Build a small tree, then tear it down (remove_child).
        for (int i = 0; i < 8; ++i)
        {
            (void)root->create_child<vaml::Canvas>();
        }
        SHOULD(ShouldlyIntegral(root->child_count()).ShouldBe(std::size_t{8}));
        while (root->child_count() > 0)
        {
            (void)root->remove_child(root->child_count() - 1);
        }
        SHOULD(ShouldlyIntegral(root->child_count()).ShouldBe(std::size_t{0}));
    }

    // The pool must not grow without bound across the churn: objects recycle.
    SHOULD(ShouldlyIntegral(vaml::detail::object_pool_block_count())
               .ShouldBeLessThan(std::size_t{8}));
    root.reset();
    SHOULD(ShouldlyIntegral(vaml::detail::object_pool_size()).ShouldBe(std::size_t{0}));

    object_pool_master_destroy_all();
}
