// S145 — object block pool free-list implementation (see ObjectPool.h).
//
// ADR-018 posture: the pool is a straight thread-local structure, single-
// threaded by construction — zero locks (same posture as InternStore/NameTable;
// the only lock in the project remains the S037/S041 ChangeQueue mutex).

#include "ObjectPool.h"

#include <cstddef>
#include <cstdint>
#include <new>

namespace vaml::detail {

namespace {

// Initial slots per block (mirrors InternStore's kInitialArenaBytes sizing
// heuristic — one modest block avoids churn for typical counts).
constexpr std::size_t kInitialSlotsPerBlock = 32;

// Round `n` up to a multiple of `align` (assumed a power of two).
constexpr std::size_t align_up(std::size_t n, std::size_t align) noexcept
{
    return (n + align - 1) & ~(align - 1);
}

// --- Basic pool invariants -------------------------------------------------
// The slot size is cache-line-rounded in the header (896). We align each block
// base to the cache line so slot addresses carry at least that alignment.
constexpr std::size_t kCacheLine = 64;
constexpr std::size_t kBlockAllocBytes =
    align_up(kInitialSlotsPerBlock * ObjectPool::kSlotSize, kCacheLine);

} // namespace

ObjectPool::~ObjectPool()
{
    master_destroy_all();
}

void* ObjectPool::allocate()
{
    // 1) Free-list pop (LIFO) — the hot recycle path.
    if (freeList_ != nullptr)
    {
        void* p = freeList_;
        freeList_ = *static_cast<void**>(p); // the slot's intrusive next
        ++live_;
        return p;
    }

    // 2) Monotonic bump within the current block.
    if (current_ != nullptr && current_->used < current_->capacity)
    {
        std::uint8_t* slot = current_->base + current_->used * kSlotSize;
        ++current_->used;
        ++live_;
        return slot;
    }

    // 3) Monotonic spill to a NEW block (never reset/reorder). The new block
    //    becomes current_ (head of the chain).
    auto* blk = new Block{};
    blk->base = static_cast<std::uint8_t*>(::operator new(kBlockAllocBytes, std::align_val_t(kCacheLine)));
    blk->capacity = kInitialSlotsPerBlock;
    blk->used = 0;
    blk->next = blocks_;
    blocks_ = blk;
    current_ = blk;
    slotsTotal_ += blk->capacity;

    std::uint8_t* slot = blk->base; // + 0
    blk->used = 1;
    ++live_;
    return slot;
}

void ObjectPool::deallocate(void* p) noexcept
{
    if (p == nullptr)
    {
        return;
    }
    if (!contains(p))
    {
        // A foreign pointer (e.g. an oversized request that fell back to the
        // process heap) is forwarded to the global allocator, not pooled.
        ::operator delete(p);
        return;
    }
    // Recycle: push onto the intrusive LIFO. The slot bytes are reused.
    *static_cast<void**>(p) = freeList_;
    freeList_ = p;
    --live_;
}

bool ObjectPool::contains(const void* p) const noexcept
{
    if (p == nullptr)
    {
        return false;
    }
    const auto* addr = static_cast<const std::uint8_t*>(p);
    for (const Block* b = blocks_; b != nullptr; b = b->next)
    {
        const auto* begin = b->base;
        const auto* end = b->base + b->capacity * kSlotSize;
        if (addr >= begin && addr < end)
        {
            // Must be a natural slot boundary (doled slots always are).
            const std::size_t off = static_cast<std::size_t>(addr - begin);
            return (off % kSlotSize) == 0;
        }
    }
    return false;
}

void ObjectPool::master_destroy_all() noexcept
{
    Block* b = blocks_;
    while (b != nullptr)
    {
        Block* next = b->next;
        ::operator delete(b->base, std::align_val_t(kCacheLine));
        delete b;
        b = next;
    }
    blocks_ = nullptr;
    current_ = nullptr;
    freeList_ = nullptr;
    live_ = 0;
    slotsTotal_ = 0;
}

std::size_t ObjectPool::block_count() const noexcept
{
    std::size_t n = 0;
    for (const Block* b = blocks_; b != nullptr; b = b->next)
    {
        ++n;
    }
    return n;
}

std::size_t ObjectPool::slot_capacity() const noexcept
{
    return slotsTotal_;
}

// ---------------------------------------------------------------------------
// Per-thread binding + public detail entry points (the operator new/delete
// funnel). The pool is NOT itself thread_local (tests instantiate isolated
// pools); only this binding is thread_local — mirrors InternStore/Intern.cpp.
// ---------------------------------------------------------------------------
namespace {

thread_local ObjectPool g_objectPool;

} // namespace

void* object_pool_allocate(std::size_t size)
{
    if (size <= ObjectPool::kSlotSize)
    {
        return g_objectPool.allocate();
    }
    // Oversized request — fall back to the process heap (operator delete
    // forwards it back via deallocate's contains() check).
    return ::operator new(size);
}

void object_pool_deallocate(void* p) noexcept
{
    g_objectPool.deallocate(p);
}

void object_pool_master_destroy_all() noexcept
{
    g_objectPool.master_destroy_all();
}

std::size_t object_pool_size() noexcept
{
    return g_objectPool.size();
}

std::size_t object_pool_block_count() noexcept
{
    return g_objectPool.block_count();
}

} // namespace vaml::detail
