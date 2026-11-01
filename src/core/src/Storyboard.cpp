// S510 — Storyboard out-of-line definitions: start (capture + resolve), tick
// (change-driven evaluation), complete/stop (FillBehavior). See Storyboard.h.

#include "vaml/core/Storyboard.h"

#include "vaml/core/DependencyObject.h" // DependencyObject (target Set/Get)
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <cmath>
#include <limits>

namespace vaml {

// ---------------------------------------------------------------------------
// ParallelTimeline
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void ParallelTimeline::add_child(std::unique_ptr<AnimationTimeline> child)
{
    if (child)
    {
        children_.push_back(std::move(child));
    }
}

// ---------------------------------------------------------------------------
// Storyboard
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void Storyboard::start(UIElement* root, NameResolver resolve)
{
    started_ = true;
    active_.clear();
    if (root == nullptr)
    {
        return;
    }
    // Resolve each child's target by name; capture its base value/transform.
    for (const auto& child : children())
    {
        AnimationTimeline* anim = child.get();
        if (anim == nullptr)
        {
            continue;
        }
        // Resolve the target: the name resolver first, else fall back to the
        // root when the animation targets the root itself (empty target name).
        UIElement* target = nullptr;
        if (resolve)
        {
            target = resolve(anim->target_name());
        }
        if (target == nullptr && anim->target_name().empty())
        {
            target = root;
        }
        if (target == nullptr)
        {
            // Unresolvable target: skip (recorded, not silent). The animation
            // simply does not bind to a live element.
            continue;
        }
        Active a;
        a.anim   = anim;
        a.target = target;
        a.begin  = anim->begin_time(); // negative offset (start-late)
        a.baseMtx = target->render_transform();
        if (anim->target_kind() == AnimationTargetKind::DoubleProperty)
        {
            const PropertyId id = anim->target_property();
            try
            {
                const Value v = target->Get(id);
                a.base = v.isNull() ? 0.0 : v.get<double>();
            }
            catch (...)
            {
                a.base = 0.0;
            }
        }
        else
        {
            a.base = 0.0;
        }
        active_.push_back(std::move(a));
    }
}

VAML_CORE_EXPORT
void Storyboard::apply(const Active& a, double elapsed)
{
    // The clock current time for THIS child = elapsed - its begin offset.
    const double local = elapsed - a.begin;
    // Run the clock-progress authority: active/Inactive + the within-cycle
    // fraction. FillBehavior Stop restores the base when the fill region is
    // reached (returned active=false).
    double fraction = 0.0;
    const bool active = timeline_clock_progress(
        local, a.anim->duration(), a.anim->auto_reverse(), a.anim->repeat_behavior(), fraction);
    if (!active)
    {
        // Fill region. HoldEnd keeps the last interpolated value (fraction is
        // clamped to the terminal cycle); Stop restores base.
        if (a.anim->fill_behavior() == FillBehavior::Stop)
        {
            if (a.anim->target_kind() == AnimationTargetKind::DoubleProperty)
            {
                (void)a.target->Set(a.anim->target_property(), Value::Double(a.base));
            }
            else
            {
                (void)a.target->set_render_transform(a.baseMtx);
            }
        }
        else
        {
            // HoldEnd: write the terminal interpolated value.
            const double v = a.anim->value_at(a.base, fraction);
            if (a.anim->target_kind() == AnimationTargetKind::DoubleProperty)
            {
                (void)a.target->Set(a.anim->target_property(), Value::Double(v));
            }
            else
            {
                const Matrix2D result = multiply(a.baseMtx, channel_matrix(a.anim->transform_channel(), v));
                (void)a.target->set_render_transform(result);
            }
        }
        return;
    }
    // Active cycle: write the interpolated value.
    const double v = a.anim->value_at(a.base, fraction);
    if (a.anim->target_kind() == AnimationTargetKind::DoubleProperty)
    {
        (void)a.target->Set(a.anim->target_property(), Value::Double(v));
    }
    else
    {
        const Matrix2D result = multiply(a.baseMtx, channel_matrix(a.anim->transform_channel(), v));
        (void)a.target->set_render_transform(result);
    }
}

VAML_CORE_EXPORT
void Storyboard::tick(double elapsedSeconds)
{
    if (!started_)
    {
        return;
    }
    for (const Active& a : active_)
    {
        apply(a, elapsedSeconds);
    }
}

VAML_CORE_EXPORT
void Storyboard::complete(bool holdEnd)
{
    if (!started_)
    {
        return;
    }
    for (const Active& a : active_)
    {
        if (holdEnd)
        {
            // HoldEnd: write the terminal interpolated value (fraction=1) onto
            // the target, honoring the child's own duration/autoReverse shape
            // via value_at at the last cycle's terminal fraction.
            double frac = 1.0;
            if (a.anim->auto_reverse())
            {
                // The last cycle of an autoReverse ping-pong ends at fraction 0.
                frac = (a.anim->repeat_behavior().forever())
                           ? 1.0
                           : (static_cast<long long>(std::floor(
                                  a.anim->repeat_behavior().count)) & 1)
                                 ? 0.0 : 1.0;
            }
            const double v = a.anim->value_at(a.base, frac);
            if (a.anim->target_kind() == AnimationTargetKind::DoubleProperty)
            {
                (void)a.target->Set(a.anim->target_property(), Value::Double(v));
            }
            else
            {
                const Matrix2D result =
                    multiply(a.baseMtx, channel_matrix(a.anim->transform_channel(), v));
                (void)a.target->set_render_transform(result);
            }
        }
        else
        {
            // Stop: restore the captured base value/transform.
            if (a.anim->target_kind() == AnimationTargetKind::DoubleProperty)
            {
                (void)a.target->Set(a.anim->target_property(), Value::Double(a.base));
            }
            else
            {
                (void)a.target->set_render_transform(a.baseMtx);
            }
        }
    }
}

} // namespace vaml
