#pragma once
// S141 — SIMD probe store (CONTENT-keyed, no hash).
//
// A single, append-only, identity-keyed store for interned UTF-8 strings.
// This is the S141 mechanism behind the PUBLIC vaml::intern() (S140_1) and
// the thread-affine identity keys / parser tree text (S140).
//
// DESIGN (ADR-018: THREAD-LOCAL + LOCKLESS):
//   - This store is NOT itself thread_local. It is a plain class so tests can
//     instantiate isolated private stores and master_destroy_all() them. The
//     per-thread binding lives in Intern.cpp (thread_local instantiation).
//   - No mutex ANYWHERE. A thread owns its store; reads and writes are
//     single-threaded by construction. Zero deadlock surface. This is the
//     ADR-018 design: the roadmapped process-global/pointer-identity framing
//     was rejected; equality is by CONTENT (SIMD full compare), never pointer.
//
// STORE CONTRACT (verbatim from roadmap):
//   - insertion-ordered append-only array
//   - SIMD-probe on 32-byte / 32-lane groups (8-bit search bytes)
//   - empty marker 0xFF (bytes past length padded with 0xFF)
//   - probe byte = fold_bytes(sv) (rotating-XOR over bytes) % 255
//     (never 255 == 0xFF, so the empty marker is unreachable as a probe index)
//   - growth = memmove+append rounded up to a 32-entry group
//   - never re-probe / reorder / shrink; erase/tombstone never applies
//   - full string_view compare disambiguates collisions; index<n bounds the
//     scan to the used prefix so tail padding never false-matches.
//
// LIFETIME: views returned by intern() are backed by a retain-forever block
// pool that is never freed for the life of the owning thread. Once in, always
// there, never removed, never relocated. (Cross-thread view passing is
// forbidden — carry literals/owned strings across threads instead.)

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace vaml::detail {

// Entry in the store: a 32-byte lane of 8-bit search bytes (0xFF-padded past
// the string length) plus the length of the original string.
class InternStore
{
public:
    InternStore() = default;
    InternStore(const InternStore&) = delete;
    InternStore& operator=(const InternStore&) = delete;
    ~InternStore();

    // Interns sv BY VALUE: probes the store; on HIT returns the existing view
    // (content-dedup); on MISS retains the bytes into the pool, inserts the
    // entry, returns the new permanent view. Caller-independent lifetime.
    [[nodiscard]] std::string_view intern(std::string_view sv) noexcept;

    // Number of distinct interned strings. Test/introspection aid.
    [[nodiscard]] std::size_t size() const noexcept { return count_; }

    // Opt-in reset (test-only / explicit teardown). Frees the entire arena.
    void master_destroy_all() noexcept;

private:
    // Number of bytes each lane carries.
    static constexpr std::size_t kLaneBytes = 32;

    // Fold byte: rotating-XOR over sv's bytes, % 255 -> 0..254.
    // Because 255 never results, the 0xFF padding marker is unreachable as a
    // probe index — a clean sentinel.
    static std::uint8_t fold_bytes(std::string_view sv) noexcept;

    // A retained string's canonical slot.
    struct Entry
    {
        const char* data; // points into the retain arena (never freed)
        std::uint32_t length;
        std::uint32_t next; // next entry index sharing this probe slot (UINT32_MAX = end)
    };

    // Probe slot: head index into entries_ for each fold byte 0..254.
    std::uint32_t slots_[255];
    bool slots_init_ = false;

    // Append-only entry array (never reordered/moved).
    Entry* entries_ = nullptr;
    std::size_t entry_capacity_ = 0;
    std::size_t count_ = 0;

    // Retain-forever block arena. Appended blocks live for the thread's life.
    struct Block
    {
        char* data;
        std::size_t cap;
        std::size_t used;
        Block* next;
    };
    Block* arena_ = nullptr;
};

} // namespace vaml::detail
