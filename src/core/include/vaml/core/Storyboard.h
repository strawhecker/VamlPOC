#pragma once
// VAML Storyboard — the S510 animation engine's container + live clock fusion
// (dep:S510, Sprint 11g).
//
// WPF-FAITHFUL: Storyboard is the top-level timeline a control author composes
// from child animations (all parallel). In WPF the Storyboard is a DESCRIPTION
// and `Begin()` creates a live Clock; THIS POC (user-accepted rev 3) FUSES the
// live clock into the Storyboard: `start(root, resolveName)` captures the base
// transforms + resolves each child's target by name, and `tick(elapsedSeconds)`
// is the change-driven evaluation that writes the animated value onto the
// target's Double DP / RenderTransform component. The true Timeline<->Clock
// split is recorded NO-GAP at the DAG `Clock` row (named forward token).
//
// Semantics honored (see Animation.h):
//   * BeginTime is a negative offset: tick uses `elapsed - child.begin_time()`
//     (0 / base while in the BeforeBegin delay).
//   * By resolves To from the base; AutoReverse ping-pongs; RepeatBehavior
//     drives progress-to-fill; FillBehavior HoldEnd keeps the end value, Stop
//     restores the captured base.
//   * Transform animations compose `multiply(capturedBase, channelMatrix)`
//     per tick and write the S208 Matrix2D (an earlier authored base is
//     preserved while the animated channel drives).
//
// start() is idempotent (an already-started storyboard re-captures + re-begins).
// tick() on a never-started storyboard is a no-op. complete()/stop() finalize
// per FillBehavior. ADR-018: ZERO new locks (single-threaded UI tick).

#include "vaml/core/Animation.h"
#include "vaml/core/Transform.h" // Matrix2D compose
#include "vaml/core/UIElement.h" // the resolved target element
#include "vaml/core/export.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace vaml {

// Forward declarations.
class DependencyObject;
class ParallelTimeline;

// The parallel grouping Timeline (the DAG-truthful base of Storyboard). This
// POC models Storyboard directly; ParallelTimeline is declared so the DAG row
// (`Storyboard -> ParallelTimeline`) is closed truthfully and a future split
// has a home. Storyboard functionally IS a ParallelTimeline with a resolve seam.
class VAML_CORE_EXPORT ParallelTimeline : public Timeline
{
public:
    ~ParallelTimeline() override = default;

    void add_child(std::unique_ptr<AnimationTimeline> child);
    [[nodiscard]] std::size_t child_count() const noexcept { return children_.size(); }
    [[nodiscard]] const std::vector<std::unique_ptr<AnimationTimeline>>& children() const noexcept
    {
        return children_;
    }

protected:
    ParallelTimeline() = default;

private:
    std::vector<std::unique_ptr<AnimationTimeline>> children_;
};

// ---------------------------------------------------------------------------
// Storyboard — the authored animation set + the fused live clock.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT Storyboard : public ParallelTimeline
{
public:
    Storyboard() = default;
    ~Storyboard() override = default;

    // The name resolver: maps an animation's target_name to its live element.
    // The caller (the reader's `named()` registry, or an app-supplied walker)
    // provides it so core stays independent of the S092 name registry.
    using NameResolver = std::function<UIElement*(const std::string&)>;

    // Begins (or re-begins) the storyboard. Captures each element's base
    // transform + current base value, and records the per-child begin offset.
    // `root` is the element whose tree the storyboard animates (used as the
    // fallback resolve root). Idempotent re-begin.
    void start(UIElement* root, NameResolver resolve);

    // Advances the clock to `elapsedSeconds` (absolute seconds since begin).
    // For each child: computes value_at over (elapsed - beginTime) + writes the
    // result onto the target (Double DP via Set, or RenderTransform compose).
    // Never throws for a missing/unresolved target (recorded, not silent: an
    // unresolvable target element is skipped); a malformed KeyTime order was
    // already rejected at authoring. No-op when never started.
    void tick(double elapsedSeconds);

    // Finalizes the storyboard per FillBehavior: HoldEnd keeps the last
    // written value (no-op); Stop restores the captured base value/transform
    // for each child. Idempotent.
    void complete(bool holdEnd = true);

    // Convenience: complete() with FillBehavior::Stop semantics.
    void stop() { complete(false); }

    [[nodiscard]] bool started() const noexcept { return started_; }

private:
    // One resolved, live animation instance (a child + its captured context).
    struct Active
    {
        AnimationTimeline* anim    = nullptr; // non-owning (parent owns)
        UIElement*         target  = nullptr; // resolved element (non-owning)
        double             base    = 0.0;     // base Double value (for By/From defaulting)
        Matrix2D           baseMtx;            // base RenderTransform (for channel compose)
        double             begin   = 0.0;      // this child's begin offset (negative-offset)
    };

    // Applies one active child at `elapsed`; writes the resulting value.
    void apply(const Active& a, double elapsed);

    bool started_ = false;
    std::vector<Active> active_;
};

} // namespace vaml
