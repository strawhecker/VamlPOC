// S202_1 — IconAtlas implementation: binds the icon::raster_icon import/raster
// seam to the S202 quad::TextureAtlas. add_icon() rasterizes a packaged source
// ONCE, uploads it as one tile via the S202 sub-region upload (dirtying ONLY
// that tile), and records the slot tagged with the raster's texel count.
//
// THE ADR-015 STRUCTURAL RULE is enforced here: add_icon() only consumes from
// the IconSourceSet it was given (set_sources); any other key is rejected as
// OutOfSet BEFORE any decode — a raw path/URI/script is structurally
// unrepresentable and therefore unreachable.
//
// THREADING (ADR-018): single-threaded, once-at-import, zero locks.

#include <vaml/vulkan/icon/IconAtlas.h>

#include <utility>

namespace vaml::vulkan::icon {

namespace {

constexpr std::uint32_t kNoSlot = 0xFFFFFFFFu;

} // namespace

IconAtlas::IconAtlas(quad::TextureAtlas& atlas) noexcept
    : atlas_(&atlas)
{
}

void IconAtlas::set_sources(const IconSourceSet& sources) noexcept
{
    sources_ = sources;
    // Rebuild the slot bookkeeping: keep key order deterministic (a stable,
    // sorted key order so tiles are assigned deterministically across runs).
    slots_.clear();
    slotKeys_.clear();
    nextTile_ = 0;
}

bool IconAtlas::add_icon(std::uint32_t key,
                         std::uint32_t sizeW, std::uint32_t sizeH,
                         IconErrorKind* outError,
                         std::uint32_t* renderW,
                         std::uint32_t* renderH) noexcept
{
    if (outError)
        *outError = IconErrorKind::None;

    // --- 1) ADR-015 structural rule: only in-set keys are consumable ---
    IconSource source;
    if (!source_by_key(sources_, key, source))
    {
        if (outError)
            *outError = IconErrorKind::OutOfSet;
        return false;
    }

    // --- 2) rasterize ONCE (the pure seam; never per-frame) ---
    IconRaster raster;
    IconErrorKind err = IconErrorKind::None;
    if (!raster_icon(source, sizeW, sizeH, raster, &err))
    {
        if (outError)
            *outError = err;
        return false;
    }

    // --- 3) find a free tile on the S202 atlas ---
    const std::uint32_t tileCount = atlas_tile_count(atlas_->layout());
    if (tileCount == 0u || nextTile_ >= tileCount)
    {
        if (outError)
            *outError = IconErrorKind::Unsupported;  // no free tile
        return false;
    }
    const std::uint32_t tile = nextTile_++;
    if (raster.rgba.size() != atlas_tile_bytes(atlas_->layout()))
    {
        // The raster dims must exactly fill one atlas tile (the exact-grid
        // contract); reject rather than silently mis-upload.
        if (outError)
            *outError = IconErrorKind::Unsupported;
        --nextTile_;
        return false;
    }

    // --- 4) upload_tile → S202 SUB-REGION INVALIDATION (only this tile dirties) ---
    if (!atlas_->upload_tile(tile, raster.rgba))
    {
        if (outError)
            *outError = IconErrorKind::Unsupported;
        --nextTile_;
        return false;
    }

    // --- 5) record/overwrite the slot TAGGED with the texel count (S213_1) ---
    // Keys may be any uint32; to keep slot records compact we search the slotKeys
    // list linearly (source sets are small, import-time only).
    std::uint32_t slotIdx = kNoSlot;
    for (std::uint32_t i = 0; i < slotKeys_.size(); ++i)
    {
        if (slotKeys_[i] == key)
        {
            slotIdx = i;
            break;
        }
    }
    if (slotIdx == kNoSlot)
    {
        slotIdx = static_cast<std::uint32_t>(slotKeys_.size());
        slotKeys_.push_back(key);
        slots_.emplace_back();
    }
    slots_[slotIdx].tileIndex  = tile;
    slots_[slotIdx].texelCount = raster.texelCount;

    if (renderW)
        *renderW = raster.w;
    if (renderH)
        *renderH = raster.h;
    return true;
}

IconSlot IconAtlas::slot_for(std::uint32_t key) const noexcept
{
    for (std::uint32_t i = 0; i < slotKeys_.size(); ++i)
    {
        if (slotKeys_[i] == key)
            return slots_[i];
    }
    return {};
}

bool IconAtlas::has_slot(std::uint32_t key) const noexcept
{
    for (const std::uint32_t k : slotKeys_)
    {
        if (k == key)
            return true;
    }
    return false;
}

} // namespace vaml::vulkan::icon
