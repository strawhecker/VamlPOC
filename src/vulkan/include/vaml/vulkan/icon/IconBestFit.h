#pragma once
// S213_1 (dep:S213) — DPI-aware icon best-fit selection (per ADR-015): the scaling
// POLICY { none | down-only | both }, default none. This is the SELECTION half that
// S202_1 deliberately left to S213_1 (its header records "the DPI best-fit SELECTION
// policy stays fully with S213_1"): S202_1 PRODUCES the tagged candidates (per-slot
// texelCount + the size family), S213_1 CHOOSES + CACHES.
//
// SEMANTICS (the S213_1 decision model, per ADR-015):
//   N_physical = logical_size × dpiScale   (the derived framebuffer scale — the S213
//   seam, consumed at runtime from the host's last_framebuffer_scale()).
//   "none"       = NEVER resample; pick the LARGEST authored texel set whose SIDE is
//                  ≤ N_physical (a "texel set of size N" is the N×N set; its defining
//                  number is its side). The sprite may render smaller than its slot.
//   "down-only"  = may sample a LARGER set DOWN to exactly N_physical, never up: if
//                  any authored set is ≤ N_physical, use the largest such WITHOUT
//                  resampling (never up); only when NO set is ≤ N_physical (all are
//                  larger) may the closest one be down-sampled to exactly N_physical.
//   "both"       = permits either direction (blur possible): prefer the largest
//                  authored set ≤ N_physical with NO resample (clearest); only when
//                  nothing is ≤ N_physical do we sample the closest one to exactly
//                  N_physical.
//
// The candidate is identified by its authored SIDE (px) — icons rasterize as square
// tiles (S202_1 raster_icon at sizeW×sizeH), so the side is the honest comparison
// axis against the physical side N_physical; the S202_1 texelCount tag (== side²) is
// carried for parity. Cut a NOTE in the roadmap/progress (recorded decision): the
// side-based reading is the self-consistent one for "texel set ≤ N" where N is a
// physical SIDE (logical_size × dpiScale), NOT an area.
//
// PER-(KEY, LOGICAL SIZE) COMPUTED ONCE AT RUNTIME, THEN CACHED: the policy decision
// is evaluated against framebufferScale ONE time per (IconKey, logical size) and the
// result is cached (IconBestFitCache). Because physical N depends on dpiScale, a
// dpiScale CHANGE (a resolution change) invalidates the whole cache (the "computed
// once against framebufferScale then cached" rule).
//
// THREADING (ADR-018): ZERO new locks. The cache is single-threaded, evaluated
// once-at-layout (the same discipline as import-time raster). The resolved
// {tileIndex, resampleToN, targetN} is consumed by S400_1's Icon control (quad UVs +
// the resample decision).
//
// NO-GAP recorded (not silent): S400_1 (Icon control) consumes IconBestFitCache::
// resolve passing dpiScale = host.last_framebuffer_scale(); S360_1 (IconKey→
// ResourceDictionary) feeds the key space; S528_1 (gallery) re-verifies size-family +
// DPI at the render level. This is real, permanent, kept-and-extended SURFACE — NOT
// a MINIMAL REAL marker, NOT a stub.

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#ifndef VAML_VULKAN_EXPORT
#  define VAML_VULKAN_EXPORT
#endif

namespace vaml::vulkan::icon {

// ---------------------------------------------------------------------------
// The scaling policy (per ADR-015). Default = None.
// ---------------------------------------------------------------------------
enum class ScalingPolicy
{
    None = 0,      // never resample; largest authored set ≤ N (sprite may be smaller)
    DownOnly = 1,  // may sample a larger set down to N; never up
    Both = 2,      // permits either direction (blur possible)
};

// One authored texel set in a size family — the S213_1 candidate. `sizeW`×`sizeH`
// are the authored dimensions (px); icons rasterize square (sizeW == sizeH == the
// SIDE), so `side()` is the best-fit axis against the physical side N_physical.
// `texelCount == sizeW*sizeH` (the S202_1 best-fit tag, carried for parity + S400_1's
// read). `tileIndex` is the S202 atlas tile the set occupies.
struct IconCandidate
{
    std::uint32_t tileIndex  = 0;
    std::uint32_t sizeW      = 0;   // authored width (px)
    std::uint32_t sizeH      = 0;   // authored height (px)
    std::uint32_t texelCount = 0;   // == sizeW*sizeH (the S202_1 best-fit tag)

    // The SIDE (px) the best-fit compares against N_physical. Icons rasterize square
    // (the S202_1 raster_icon contract at sizeW×sizeH), so sizeW is the honest side.
    [[nodiscard]] std::uint32_t side() const noexcept { return sizeW; }
};

// The S213_1 decision for ONE (key, logical size): which candidate to use (by its
// index in the candidate list) + whether the chosen set must be re-sampled to a
// target physical side + what that target is.
//
//   candidateIndex  = index into the candidate list (-1 = never; the caller re-checks
//                     nullopt on empty candidates)
//   resampleToN     = true ONLY when the chosen set must be sampled to exactly
//                     targetN (never true under None; true under DownOnly/Both only
//                     when no set was ≤ N and we sample a larger one down)
//   targetN         = the physical SIDE the chosen sprite renders/resamples at: the
//                     candidate's own side when resampleToN is false, else N_physical.
struct BestFitResult
{
    std::int32_t  candidateIndex = -1;
    bool          resampleToN    = false;
    std::uint32_t targetN        = 0;
};

// N_physical = round(logical_size × dpiScale), clamped to ≥ 1. Guards a non-positive
// logical size or dpiScale → 1 (the minimum so a best-fit still targets something).
// The physical SIDE (px) the best-fit selects against. Pure + headless-testable.
VAML_VULKAN_EXPORT std::uint32_t physical_n_for(float logicalSize, float dpiScale) noexcept;

// The PURE best-fit DECISION SEAM: given a scaling policy, a candidate list (the size
// family, any order — the largest ≤ N and the closest-above-N are found by scanning,
// not by assuming ascending), and the physical side N_physical, returns the chosen
// candidate as a BestFitResult. Returns std::nullopt ONLY for an empty candidate list.
// Pure + headless-testable (this is the whole policy decision, no Vulkan runtime).
VAML_VULKAN_EXPORT std::optional<BestFitResult>
select_best_fit(ScalingPolicy policy,
                const std::vector<IconCandidate>& candidates,
                std::uint32_t nPhysical) noexcept;

// ---------------------------------------------------------------------------
// The CACHED SELECTOR (the "computed once at runtime against framebufferScale, then
// cached" owner). Keyed per (IconKey, logical size); evaluated + cached on the first
// resolve for a given (key, logical size, dpiScale) and reused on repeats. A dpiScale
// CHANGE invalidates the WHOLE cache (D3 — because physical N depends on dpiScale).
// Single-threaded, once-at-layout, ZERO locks (ADR-018).
// ---------------------------------------------------------------------------
class VAML_VULKAN_EXPORT IconBestFitCache
{
public:
    // Constructs with the default policy None (per ADR-015).
    explicit IconBestFitCache(ScalingPolicy policy = ScalingPolicy::None) noexcept;

    // Resolves the best fit for (key, logicalSize) against `candidates` (the size
    // family for `key` from IconAtlas::candidates_for) + `dpiScale`. Computes + caches
    // on the first call for the (key, logicalSize) at the CURRENT dpiScale; a changed
    // dpiScale clears the cache first (D3). Returns the cached/decided BestFitResult
    // (std::nullopt for an empty candidate family).
    std::optional<BestFitResult>
    resolve(std::uint32_t key, std::uint32_t logicalSize, float dpiScale,
            const std::vector<IconCandidate>& candidates) noexcept;

    // A policy change invalidates the cache (the decision depends on the policy).
    void set_scaling_policy(ScalingPolicy policy) noexcept;
    [[nodiscard]] ScalingPolicy scaling_policy() const noexcept { return policy_; }

    // Drops all cached decisions (the next resolve recomputes). Called on a dpiScale
    // change (see resolve) and available to callers that change their candidate set.
    void clear() noexcept;

    // Number of distinct (key, logical size) decisions currently cached.
    [[nodiscard]] std::size_t cached_count() const noexcept { return cache_.size(); }

private:
    struct CacheKey
    {
        std::uint32_t key        = 0;
        std::uint32_t logicalSize = 0;
        bool operator==(const CacheKey& o) const noexcept
        {
            return key == o.key && logicalSize == o.logicalSize;
        }
    };
    struct CacheKeyHash
    {
        std::size_t operator()(const CacheKey& k) const noexcept
        {
            // A split/combine of two uint32 — collision-free enough for a small
            // once-at-layout cache (bounded, single-threaded).
            return (static_cast<std::size_t>(k.key) << 20) ^ k.logicalSize;
        }
    };

    ScalingPolicy                                       policy_ = ScalingPolicy::None;
    float                                               dpiScale_ = 1.0f;
    std::unordered_map<CacheKey, BestFitResult, CacheKeyHash> cache_;
};

} // namespace vaml::vulkan::icon
