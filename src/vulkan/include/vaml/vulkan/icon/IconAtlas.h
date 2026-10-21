#pragma once
// S202_1 — IconAtlas: the real owner that binds the S202_1 import/raster seam
// (icon::raster_icon) to the S202 quad::TextureAtlas. IconAtlas holds the
// packaged IconSourceSet, a 1:1 slot map (key → { tileIndex, texelCount }), and
// a reference to a quad::TextureAtlas; add_icon() rasterizes a source ONCE,
// uploads it as one tile via the S202 sub-region upload (which dirties ONLY
// that tile), and records the slot TAGGED with the raster's texel count — the
// S213_1 best-fit input.
//
// The atlas is drawn by the UNCHANGED S200/S201 quad pipeline: a consumer
// samples texture_atlas() (the existing image view/sampler) and selects a tile
// with quad UVs. S202_1 adds NO renderer, NO vector-on-screen path.
//
// SIZE FAMILY (S213_1 best-fit): the same key may be added at several
// requested sizes; each add_icon() produces a distinct tile, each carrying its
// own texelCount. The DPI best-fit SELECTION policy remains S213_1's (per the
// S202_1 plan decision #2) — this class only PRODUCES the tagged candidates.
//
// THREADING (ADR-018): zero locks. add_icon()/rasterize is CPU-bound,
// single-threaded, once-at-import. The underlying quad::TextureAtlas must
// outlive this object, and its graphics queue must be idle before flush().
//
// NOT here (recorded, not silent): the monochrome tint is S205_1; the fallback
// GLYPH is S400_1's; the IconKey→resource dict is S360_1's. This is a real,
// permanent owner — NOT a stub / NOT MINIMAL REAL.

#include <vaml/vulkan/quad/TextureAtlas.h>
#include <vaml/vulkan/icon/IconPipeline.h>

#include <cstdint>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::icon {

// One populated atlas slot: which tile it occupies + the raster's texel count
// (the S213_1 best-fit tag). texelCount == texelCount of the raster's tile.
struct IconSlot
{
    std::uint32_t tileIndex  = 0;
    std::uint32_t texelCount = 0;
};

// The real S202_1 owner. RAII but move-COPYABLE-by-value semantics are NOT
// provided: it is a lightweight value that borrows a quad::TextureAtlas (which
// itself is move-only) and owns the packaged sources + slot map. Constructed
// directly (no create() factory needed — it holds no Vulkan handles itself).
class VAML_VULKAN_EXPORT IconAtlas
{
public:
    // Borrows `atlas` for tile uploads. `atlas` must outlive this object.
    explicit IconAtlas(quad::TextureAtlas& atlas) noexcept;

    // Copies `sources` in (the packaged, ADR-015-bounded icon set). Keys may be
    // any uint32; only members of this set are ever consumable by add_icon().
    void set_sources(const IconSourceSet& sources) noexcept;

    // Rasterizes `key`'s source ONCE at sizeW×sizeH and uploads it as the next
    // free atlas tile. On success records/overwrites the slot for `key` tagged
    // with the raster's texelCount and returns true. Collects the RENDERED size
    // into renderW/renderH (nullptr tolerated). Returns false — WITHOUT corrupting
    // anything — and sets outError (if given) when: the key is out-of-set (the
    // ADR-015 structural rule → OutOfSet), the source fails to decode/raster
    // (Malformed/Unsupported), or the atlas has no free tile. The bytes are NOT
    // flushed to the GPU until flush() is called (S202 sub-region invalidation:
    // only the newly dirtied tile is uploaded).
    [[nodiscard]] bool add_icon(std::uint32_t key,
                                std::uint32_t sizeW, std::uint32_t sizeH,
                                IconErrorKind* outError = nullptr,
                                std::uint32_t* renderW = nullptr,
                                std::uint32_t* renderH = nullptr) noexcept;

    // The slot record for `key` (tileIndex + texelCount); {0,0} if never added.
    [[nodiscard]] IconSlot slot_for(std::uint32_t key) const noexcept;

    // True iff `key` has a populated slot.
    [[nodiscard]] bool has_slot(std::uint32_t key) const noexcept;

    // The atlas all icons are drawn from (sampled by the unchanged S200/S201
    // pipeline). Forwards: stable after construction.
    [[nodiscard]] quad::TextureAtlas& atlas() noexcept { return *atlas_; }
    [[nodiscard]] const quad::TextureAtlas& atlas() const noexcept { return *atlas_; }

    // Number of distinct slots populated so far.
    [[nodiscard]] std::uint32_t slot_count() const noexcept { return static_cast<std::uint32_t>(slots_.size()); }

private:
    quad::TextureAtlas*   atlas_   = nullptr;
    IconSourceSet         sources_;
    std::vector<IconSlot> slots_;            // parallel to the set (a compaction
                                             // of the populated sources, key order)
    std::vector<std::uint32_t> slotKeys_;    // key for each slots_ entry
    std::uint32_t         nextTile_ = 0;     // next free atlas tile index
};

} // namespace vaml::vulkan::icon
