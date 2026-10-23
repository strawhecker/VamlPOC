// S213_1 — DPI-aware icon best-fit selection (per ADR-015): the SELECTION half the
// S202_1 plan deferred ("the DPI best-fit SELECTION POLICY stays fully with S213_1").
// Consumes the S202_1 per-slot candidate list (the size family) + the S213 derived
// framebuffer scale (dpiScale) — the S213_1 dpiScale input from the host.
//
// POLICY MODEL (recorded, NOT silent): the candidate is identified by its authored
// SIDE in px (icons rasterize square tiles), and N_physical = logical_size × dpiScale
// is a physical SIDE. "Largest texel set ≤ N" is therefore read as the largest
// authored SIDE ≤ N_physical (the honest comparison against a physical side, NOT an
// area comparison — an area reading of "texel set ≤ N" would double-shrink). The S202_1
// texelCount tag (== side²) is carried for parity with S202_1 + S400_1's reads.
//
// THREADING (ADR-018): ZERO new locks — the cache is single-threaded, once-at-layout.

#include <vaml/vulkan/icon/IconBestFit.h>

#include <algorithm>
#include <cmath>

namespace vaml::vulkan::icon {

std::uint32_t physical_n_for(float logicalSize, float dpiScale) noexcept
{
    if (logicalSize <= 0.0f || !std::isfinite(logicalSize) ||
        dpiScale <= 0.0f || !std::isfinite(dpiScale))
        return 1u;  // the minimum physical side (best-fit still targets something)
    const float n = std::round(logicalSize * dpiScale);
    // A non-positive product collapses to the minimum.
    return n >= 1.0f ? static_cast<std::uint32_t>(n) : 1u;
}

std::optional<BestFitResult>
select_best_fit(ScalingPolicy policy,
                const std::vector<IconCandidate>& candidates,
                std::uint32_t nPhysical) noexcept
{
    if (candidates.empty())
        return std::nullopt;

    // Scan the candidate family (any order): the largest authored SIDE ≤ N_physical
    // (bestLte) and the smallest authored SIDE > N_physical (bestAbove — the closest
    // larger set). nPhysical is clamped ≥ 1 so the selection never under-targets.
    const std::int32_t none = -1;
    std::int32_t bestLte   = none;  // index of largest size ≤ N
    std::uint32_t bestLteSize = 0;
    std::int32_t bestAbove = none;  // index of smallest size > N
    std::uint32_t bestAboveSize = 0;

    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        const std::uint32_t s = candidates[i].side();
        if (s <= nPhysical)
        {
            if (bestLte == none || s > bestLteSize)
            {
                bestLte = static_cast<std::int32_t>(i);
                bestLteSize = s;
            }
        }
        else
        {
            if (bestAbove == none || s < bestAboveSize)
            {
                bestAbove = static_cast<std::int32_t>(i);
                bestAboveSize = s;
            }
        }
    }

    BestFitResult r;
    switch (policy)
    {
    case ScalingPolicy::None:
        // NEVER resample: use the largest set ≤ N (may be smaller than the slot). If
        // none is ≤ N, use the SMALLEST (closest) authored set with NO resample — it
        // may overflow/clip its slot, matching "none never resamples" (D1).
        if (bestLte != none)
        {
            r.candidateIndex = bestLte;
            r.resampleToN    = false;
            r.targetN        = bestLteSize;
        }
        else
        {
            r.candidateIndex = bestAbove;
            r.resampleToN    = false;
            r.targetN        = bestAboveSize;
        }
        break;

    case ScalingPolicy::DownOnly:
        // May sample a LARGER set DOWN to exactly N, never up: when a set is ≤ N use
        // the largest such with NO resample (never up); only when all are larger may
        // the closest one be down-sampled to exactly N.
        if (bestLte != none)
        {
            r.candidateIndex = bestLte;
            r.resampleToN    = false;  // never up
            r.targetN        = bestLteSize;
        }
        else
        {
            r.candidateIndex = bestAbove;
            r.resampleToN    = true;  // down-sample the larger set to N
            r.targetN        = nPhysical;
        }
        break;

    case ScalingPolicy::Both:
        // Permits either direction (blur possible): prefer the largest ≤ N with NO
        // resample (clearest); only when nothing is ≤ N sample the closest to N.
        if (bestLte != none)
        {
            r.candidateIndex = bestLte;
            r.resampleToN    = false;  // exact candidate side, no blur
            r.targetN        = bestLteSize;
        }
        else
        {
            r.candidateIndex = bestAbove;
            r.resampleToN    = true;  // up-sample (blur possible)
            r.targetN        = nPhysical;
        }
        break;
    }

    return r;
}

IconBestFitCache::IconBestFitCache(ScalingPolicy policy) noexcept
    : policy_(policy)
{
}

std::optional<BestFitResult>
IconBestFitCache::resolve(std::uint32_t key, std::uint32_t logicalSize,
                          float dpiScale,
                          const std::vector<IconCandidate>& candidates) noexcept
{
    // A dpiScale change invalidates the WHOLE cache (D3): physical N depends on
    // dpiScale, so every prior decision is stale.
    if (dpiScale != dpiScale_)
    {
        dpiScale_ = dpiScale;
        cache_.clear();
    }

    const CacheKey k{ key, logicalSize };
    const auto it = cache_.find(k);
    if (it != cache_.end())
        return it->second;

    const std::uint32_t nPhysical = physical_n_for(static_cast<float>(logicalSize), dpiScale);
    auto decided = select_best_fit(policy_, candidates, nPhysical);
    if (decided)
        cache_.emplace(k, *decided);
    return decided;
}

void IconBestFitCache::set_scaling_policy(ScalingPolicy policy) noexcept
{
    if (policy != policy_)
    {
        policy_ = policy;
        cache_.clear();  // the decision depends on the policy
    }
}

void IconBestFitCache::clear() noexcept
{
    cache_.clear();
}

} // namespace vaml::vulkan::icon
