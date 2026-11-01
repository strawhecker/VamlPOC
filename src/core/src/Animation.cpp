// S510 — Animation out-of-line definitions: the pure clock/progress authority,
// the DoubleAnimation From/To/By resolution, the keyframe machinery, and the
// channel->Matrix2D compose. See Animation.h for the WPF-faithful semantics.

#include "vaml/core/Animation.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace vaml {

VAML_CORE_EXPORT
bool parse_key_time(std::string_view s, KeyTime& out) noexcept
{
    if (s.empty())
    {
        return false;
    }
    // Fraction form: "50%".
    if (!s.empty() && s.back() == '%')
    {
        const std::string_view num = s.substr(0, s.size() - 1);
        bool ok = false;
        double f = 0.0;
        try
        {
            std::size_t used = 0;
            f = std::stod(std::string(num), &used);
            ok = (used == num.size());
        }
        catch (...)
        {
            ok = false;
        }
        if (!ok || f < 0.0)
        {
            return false;
        }
        out = KeyTime::Fraction(f / 100.0);
        return true;
    }

    // Time-span form: "0:0:1" (H:M:S) / "0:1" (M:S) / "1" (S) / a plain
    // decimal seconds ("0.5").
    double total = 0.0;
    std::string_view rest = s;
    int parts = 0;
    bool progressed = true;
    while (progressed)
    {
        progressed = false;
        const auto colon = rest.find(':');
        const std::string_view token = (colon == std::string_view::npos) ? rest
                                                                        : rest.substr(0, colon);
        bool ok = false;
        double v = 0.0;
        try
        {
            std::size_t used = 0;
            v = std::stod(std::string(token), &used);
            ok = (used == token.size());
        }
        catch (...)
        {
            ok = false;
        }
        if (!ok || v < 0.0)
        {
            return false;
        }
        total = total * 60.0 + v;
        ++parts;
        if (colon != std::string_view::npos)
        {
            rest = rest.substr(colon + 1);
            progressed = true;
        }
        else
        {
            break;
        }
        if (parts > 3)
        {
            return false; // at most H:M:S
        }
    }
    if (parts == 0 || parts > 3)
    {
        return false;
    }
    out = KeyTime::Time(total);
    return true;
}

VAML_CORE_EXPORT
bool timeline_clock_progress(double elapsed, double duration,
                             bool autoReverse,
                             const RepeatBehavior& repeat,
                             double& fraction) noexcept
{
    fraction = 0.0;
    // BeforeBegin delay (negative offset, no autoReverse below zero).
    if (elapsed < 0.0)
    {
        return false; // inactive
    }
    if (duration <= 0.0)
    {
        // A zero-duration animation is a single instant: fraction 1 at elapsed>=0.
        fraction = 1.0;
        return !repeat.forever();
    }
    // AutoReverse doubles the effective duration: one "iteration" = forward +
    // reverse (WPF-faithful — the whole forward+reverse is one RepeatBehavior
    // iteration).
    const double effDur = autoReverse ? 2.0 * duration : duration;
    // The iteration index this elapsed falls in, and the position within it.
    double iter = elapsed / effDur;
    const long long iterIdx = static_cast<long long>(iter);
    const double local = elapsed - static_cast<double>(iterIdx) * effDur;

    if (repeat.forever())
    {
        // Never fills -> always active.
        double f;
        if (autoReverse)
        {
            f = (local < duration) ? (local / duration)
                                   : (2.0 * duration - local) / duration;
        }
        else
        {
            f = local / duration;
        }
        if (f < 0.0) { f = 0.0; }
        if (f > 1.0) { f = 1.0; }
        fraction = f;
        return true;
    }
    if (iter >= repeat.count)
    {
        // Fill region reached: inactive; clamp fraction to the terminal value
        // of the FINAL iteration so a HoldEnd writer holds the last interpolated
        // value.
        const double whole = std::floor(repeat.count);
        const bool fractionalEnd = (repeat.count != whole);
        double f = 1.0;
        if (!fractionalEnd)
        {
            // Whole-count end: the timeline ends at a cycle boundary. For
            // autoReverse the boundary flips each iteration (even=start,
            // odd=end); without it the terminal fraction is 1.
            f = autoReverse ? ((static_cast<long long>(whole) & 1) == 0 ? 0.0 : 1.0) : 1.0;
        }
        else
        {
            // Fractional end: land mid-cycle (half-way when the fractional part
            // is exactly the boundary of the autoReverse forward half, etc.).
            const double rem = repeat.count - whole;
            f = (autoReverse && rem > 0.5) ? (2.0 - rem) : rem;
            if (autoReverse && rem <= 0.5)
            {
                f = rem;
            }
        }
        if (f < 0.0) { f = 0.0; }
        if (f > 1.0) { f = 1.0; }
        fraction = f;
        return false;
    }
    // Active cycle: within-cycle fraction honoring autoReverse.
    double f;
    if (autoReverse)
    {
        f = (local < duration) ? (local / duration)
                               : (2.0 * duration - local) / duration;
    }
    else
    {
        f = local / duration;
    }
    if (f < 0.0) { f = 0.0; }
    if (f > 1.0) { f = 1.0; }
    fraction = f;
    return true;
}

// ---------------------------------------------------------------------------
// DoubleAnimation::value_at — WPF-faithful From/To/By.
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
double DoubleAnimation::value_at(double baseValue, double fraction) const
{
    const double from = hasFrom_ ? from_ : baseValue;
    double to = 0.0;
    if (hasTo_)
    {
        to = to_;
    }
    else if (hasBy_)
    {
        to = from + by_; // By is a DELTA from the effective From/base
    }
    else
    {
        to = baseValue; // neither To nor By: unchanging
    }
    double t = fraction;
    if (t < 0.0) { t = 0.0; }
    if (t > 1.0) { t = 1.0; }
    return from + (to - from) * t;
}

// ---------------------------------------------------------------------------
// DoubleAnimationUsingKeyFrames
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void DoubleAnimationUsingKeyFrames::add_key_frame(std::unique_ptr<DoubleKeyFrame> frame)
{
    if (!frame)
    {
        return;
    }
    // WPF-illegal out-of-order guard: the new frame's KeyTime fraction must be
    // >= the LAST frame's (fail loud, never silent). Compared as fractions of
    // an unspecified total; fraction-vs-seconds are compared by their unit-
    // relative position is not known until the duration, so guard only against
    // an EXPLICIT fraction going strictly backward. A Time-seconds frame is
    // accepted (its ordering is validated at value_at against the total).
    if (!frames_.empty())
    {
        const KeyTime& last = frames_.back()->key_time();
        if (last.unit == KeyTimeUnit::Fraction && frame->key_time().unit == KeyTimeUnit::Fraction &&
            frame->key_time().value < last.value)
        {
            throw std::invalid_argument(
                "DoubleAnimationUsingKeyFrames: out-of-order keyframe KeyTime (WPF-illegal)");
        }
    }
    frames_.push_back(std::move(frame));
}

namespace {

// Converts a KeyTime to a fraction of `total` (the animation duration).
[[nodiscard]] double kt_fraction(const KeyTime& kt, double total)
{
    return kt.to_fraction(total);
}

} // namespace

VAML_CORE_EXPORT
double DoubleAnimationUsingKeyFrames::keyframe_value_at(double fraction) const
{
    if (frames_.empty())
    {
        return 0.0;
    }
    // A single keyframe: its value holds across the whole timeline.
    if (frames_.size() == 1)
    {
        return frames_[0]->value();
    }

    // Convert each frame's KeyTime to a fraction of the timeline. With an
    // unspecified total we use the LAST frame's time as the nominal total when
    // every frame is a time-span; a Fraction unit self-reports.
    double total = 1.0;
    if (const DoubleKeyFrame* last = frames_.back().get();
        last->key_time().unit == KeyTimeUnit::TimeSeconds)
    {
        total = last->key_time().value > 0.0 ? last->key_time().value : 1.0;
    }
    const std::size_t n = frames_.size();
    if (fraction <= 0.0)
    {
        return frames_[0]->value();
    }

    const double target = (fraction > 1.0 ? 1.0 : fraction);

    // Find the enclosing segment [i, i+1) by KeyTime fraction, choosing the
    // LAST frame whose KeyTime <= the target (the segment we are "in").
    // WPF-faithful: Linear interpolates the enclosing segment; Discrete holds
    // the PRIOR frame's value until the NEXT frame's KeyTime is reached.
    std::size_t seg = 0;
    for (std::size_t i = 0; i + 1 < n; ++i)
    {
        const double t1 = kt_fraction(frames_[i + 1]->key_time(), total);
        if (target < t1)
        {
            seg = i;
            break;
        }
        seg = i + 1;
    }
    // `seg` is the segment [seg, seg+1). Find the ACTIVE (owning) frame: the
    // largest frame index whose KeyTime <= target.
    std::size_t owner = 0;
    for (std::size_t i = 0; i < n; ++i)
    {
        if (target >= kt_fraction(frames_[i]->key_time(), total))
        {
            owner = i;
        }
        else
        {
            break;
        }
    }
    if (owner + 1 >= n)
    {
        // Past the last keyframe: hold the last value.
        return frames_.back()->value();
    }
    if (seg >= n - 1)
    {
        // Target reached the last segment's start -> hold the prior value (or
        // the second-to-last frame).
        return frames_.back()->value();
    }
    // The active frame is `owner`; the next frame is owner+1.
    const double t0 = kt_fraction(frames_[owner]->key_time(), total);
    const double t1 = kt_fraction(frames_[owner + 1]->key_time(), total);
    if (frames_[owner]->is_linear() && t1 > t0)
    {
        const double local = (target - t0) / (t1 - t0);
        return frames_[owner]->value() +
               (frames_[owner + 1]->value() - frames_[owner]->value()) * local;
    }
    // Discrete: hold the active frame's value until the next KeyTime.
    return frames_[owner]->value();
}

VAML_CORE_EXPORT
double DoubleAnimationUsingKeyFrames::value_at(double baseValue, double fraction) const
{
    (void)baseValue; // keyframe animations are absolute (not base-relative)
    return keyframe_value_at(fraction);
}

// ---------------------------------------------------------------------------
// channel_matrix — the per-channel Matrix2D builder (the S208 path).
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
Matrix2D channel_matrix(TransformChannel channel, double value) noexcept
{
    switch (channel)
    {
    case TransformChannel::TranslateX:
        return Matrix2D::translation(static_cast<float>(value), 0.0f);
    case TransformChannel::TranslateY:
        return Matrix2D::translation(0.0f, static_cast<float>(value));
    case TransformChannel::ScaleX:
        return Matrix2D::scaling(static_cast<float>(value), 1.0f);
    case TransformChannel::ScaleY:
        return Matrix2D::scaling(1.0f, static_cast<float>(value));
    case TransformChannel::Rotate:
        return Matrix2D::rotation(static_cast<float>(value));
    }
    return Matrix2D::identity();
}

} // namespace vaml
