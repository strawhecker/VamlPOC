#pragma once
// S145 — object block pool free-list (thread-local, retain-forever, LOCKLESS).
//
// The object-side half of the allocation fix (dep:S144). This pool dole/recycles
// DependencyObject/UIElement (and all derived element) slabs; the pool NEVER
// deletes during the thread's life, with an opt-in master_destroy_all().
//
// DESIGN (ADR-018: THREAD-LOCAL + LOCKLESS):
//   - This pool is NOT itself thread_local. It is a plain class so tests can
//     instantiate isolated pools and master_destroy_all() them. The per-thread
//     binding lives in ObjectPool.cpp (thread_local instantiation) — mirrors
//     InternStore.
//   - No mutex ANYWHERE. A thread owns its pool; allocate/deallocate are
//     single-threaded by construction — the pool stays thread-affine per
//     S037/ADR-018, so it needs NO lock (unlike the process-global string-pool
//     framing that was rejected). Zero deadlock surface.
//   - Retain-forever: blocks are freed ONLY on master_destroy_all()/destruction.
//     Dole/recycle never free a block and never delete the object bytes.
//
// HOW THE OBJECT FITS A SLOT (the "SIMD store sized per block slot"): a slot is
// sized to the LARGEST poolable element type (Grid, 856 B today → 896 after the
// cache-line round-up). Because the S142 store (Store slots_[kInline]) is a
// DependencyObject member living at a fixed offset IN the object's allocation,
// a slot carries BOTH the object AND its inline SIMD store — no per-object heap
// hop. Only store overflow / vector growth spills (monotonic).
//
// RECYCLE == SLAB REUSE, NOT STATE CARRYOVER: the object's internal dynamic
// state (Store overflow, children_/subs_/inheritance_scope_ vectors, owned
// PropertyMetadata/PropertyKey nodes, Grid::Impl) is torn down NORMALLY by the
// destructor; only the object's own slab bytes are retained on the free-list and
// reused.

#include <cstddef>
#include <cstdint>

namespace vaml::detail {

class ObjectPool
{
public:
    // The fixed slot size: the LARGEST poolable element type across BOTH CI
    // lanes, rounded up to a cache line. sizeof(Grid)==856 (release MSVC) /
    // 848 (release Linux clang), but the msvc-debug lane (checked iterators,
    // /MTd) inflates it to 952 — the slot MUST cover that worst case, so
    // kSlotSize = align_up(952, 64) = 960. 960 % 64 == 0 (cache line) and
    // 960 % 16 == 0 (Linux max_align_t). A slot holds the object AND its
    // inline S142 store with no per-object heap hop.
    static constexpr std::size_t kSlotSize = 960;

    // Guard: the pool doles elements of ANY type that fits; derived element
    // classes larger than this worst case would over-run a slot. The largest
    // current element is Grid (952 in the msvc-debug lane). If a future control
    // exceeds this, RAISE it (it is a compile-time constant, cheap to bump).
    static constexpr std::size_t kLargestElement = 952;

    ObjectPool() = default;
    ~ObjectPool();
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // Dole a slot: free-list pop; else monotonic bump within the current block;
    // else monotonic spill to a NEW block (never reset/reorder). Returns a
    // cache-line-aligned slot of kSlotSize.
    void* allocate();

    // Recycle `p` onto the free-list (LIFO). Never frees the block / never
    // deletes the object bytes. `p` MUST have come from this pool's allocate() —
    // except a foreign pointer is defensively forwarded to ::operator delete.
    void deallocate(void* p) noexcept;

    // True if `p` addresses a live (allocated, non-freed) slot of this pool.
    // Used by DependencyObject::operator delete to route object slabs back to
    // the pool vs the process heap. O(#blocks) — fine for the rare delete path.
    [[nodiscard]] bool contains(const void* p) const noexcept;

    // Opt-in reset: free every block + reset all state (test-only / explicit
    // teardown). Mirrors InternStore::master_destroy_all().
    void master_destroy_all() noexcept;

    // --- Introspection / heap-churn assertions -----------------------------
    [[nodiscard]] std::size_t size() const noexcept { return live_; } // live (non-recycled) objects
    [[nodiscard]] std::size_t block_count() const noexcept;           // current #blocks
    [[nodiscard]] std::size_t slot_capacity() const noexcept;         // total doled-or-recyclable slots

private:
    struct Block
    {
        Block*         next;
        std::uint8_t*  base;
        std::size_t    capacity; // slots in this block
        std::size_t    used;     // slots doled via monotonic bump
    };

    void*       freeList_  = nullptr; // intrusive LIFO (next written into each freed slot)
    Block*      blocks_    = nullptr; // block chain (head = most-recently spilled)
    Block*      current_   = nullptr; // the block being bump-filled
    std::size_t live_      = 0;
    std::size_t slotsTotal_ = 0;      // total slots across blocks (never shrinks)
};

// --- Per-thread object-pool binding (defined in ObjectPool.cpp) -------------
// The "object-side" allocation funnel invoked by DependencyObject's class-scoped
// operator new/delete (see DependencyObject.cpp). A size that fits a slot is
// doled from the pool; an oversized request falls back to ::operator new (and
// operator delete forwards it back there via deallocate's contains() check).
[[nodiscard]] void*       object_pool_allocate(std::size_t size);
void                     object_pool_deallocate(void* p) noexcept;
void                     object_pool_master_destroy_all() noexcept;
[[nodiscard]] std::size_t object_pool_size() noexcept;
[[nodiscard]] std::size_t object_pool_block_count() noexcept;

} // namespace vaml::detail
