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
    // Rebuild the size-family bookkeeping (the per-key candidate lists). Family
    // order is deterministic (insertion order of the first add_icon per key).
    families_.clear();
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

    // --- 5) record into the key's SIZE-FAMILY (one candidate per add_icon) ---
    // Keys may be any uint32; to keep the family store compact we search the
    // families_ list linearly (source sets are small, import-time only). A same-key
    // repeat at a different size APPENDS a DISTINCT candidate (the size family);
    // a same-key repeat at the SAME authored size updates that candidate in place.
    std::uint32_t familyIdx = kNoSlot;
    for (std::uint32_t i = 0; i < families_.size(); ++i)
    {
        if (families_[i].key == key)
        {
            familyIdx = i;
            break;
        }
    }
    if (familyIdx == kNoSlot)
    {
        familyIdx = static_cast<std::uint32_t>(families_.size());
        KeyFamily fam;
        fam.key = key;
        families_.push_back(std::move(fam));
    }

    // Append-or-update: find an existing candidate with the same authored side
    // (same sizeW×sizeH), else append a new family member.
    auto& fam = families_[familyIdx];
    IconCandidate cand;
    cand.tileIndex  = tile;
    cand.sizeW      = raster.w;
    cand.sizeH      = raster.h;
    cand.texelCount = raster.texelCount;
    bool updated = false;
    for (IconCandidate& c : fam.candidates)
    {
        if (c.sizeW == cand.sizeW && c.sizeH == cand.sizeH)
        {
            c = cand;   // re-rasterized at the same size: refresh the tile/tag
            updated = true;
            break;
        }
    }
    if (!updated)
        fam.candidates.push_back(cand);

    if (renderW)
        *renderW = raster.w;
    if (renderH)
        *renderH = raster.h;
    return true;
}

IconSlot IconAtlas::slot_for(std::uint32_t key) const noexcept
{
    // BACKWARD-COMPATIBLE: the "1:1 slot" read returns the LARGEST candidate's
    // tile + texelCount (the pre-size-family contract for a single-candidate key).
    IconSlot best;
    std::uint32_t bestSize = 0;
    for (const KeyFamily& fam : families_)
    {
        if (fam.key != key)
            continue;
        for (const IconCandidate& c : fam.candidates)
        {
            if (c.texelCount > bestSize)
            {
                bestSize    = c.texelCount;
                best.tileIndex  = c.tileIndex;
                best.texelCount = c.texelCount;
            }
        }
    }
    return best;
}

bool IconAtlas::has_slot(std::uint32_t key) const noexcept
{
    for (const KeyFamily& fam : families_)
    {
        if (fam.key == key && !fam.candidates.empty())
            return true;
    }
    return false;
}

std::span<const IconCandidate> IconAtlas::candidates_for(std::uint32_t key) const noexcept
{
    for (const KeyFamily& fam : families_)
    {
        if (fam.key == key)
            return fam.candidates;
    }
    return {};
}

} // namespace vaml::vulkan::icon
