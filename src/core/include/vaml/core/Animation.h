#pragma once
// VAML Animation — the S510 animation engine (dep:S503, Sprint 11g).
//
// WPF-FAITHFUL three-layer model (user-accepted rev 3): a Timeline is a
// DESCRIPTION of motion (immutable, re-usable); a Clock is the live instance.
// This POC FUSES the live clock into the Storyboard (start() yields a live
// instance, tick() advances it) — a legitimately de-scoped model, recorded
// NO-GAP at the DAG `Clock` row (named forward token, NOT silent). Everything
// else below is genuine WPF semantics:
//   * BeginTime is a NEGATIVE OFFSET — clock current time = now - beginTime
//     (0 while in the BeforeBegin delay -> base value).
//   * `By` is a delta from the BASE value (value at animation start):
//     To = From==null ? base + By : From + By.
//   * AutoReverse ping-pongs over the duration (fraction >0.5 maps to
//     1-fraction); RepeatBehavior (Count/Forever) drives progress-to-fill;
//     FillBehavior HoldEnd (default) holds the endpoint, Stop restores the
//     base value on fill.
//   * Keyframes: DoubleAnimationUsingKeyFrames = ordered DoubleKeyFrame(s)
//     keyed by KeyTime. Linear interpolates the enclosing segment; Discrete
//     hops (holds until the next keyframe). KeyTime forms 0:0:1 (time span)
//     / 50% (fraction). Out-of-order keyframe = error (fail loud).
//
// This header owns the VALUE-DOMAIN primitives + the concrete animation types:
//   - FillBehavior / RepeatBehavior / KeyTime / parse_key_time /
//     KeyTime::to_fraction — the pure clock parameter types;
//   - timeline_clock_progress(elapsed, duration, autoReverse, repeat) — the
//     pure triangle/repeat fraction authority (ADR-018: pure, no locks);
//   - Timeline (abstract base: BeginTime/Duration/AutoReverse/RepeatBehavior/
//     FillBehavior) -> AnimationTimeline (adds the per-animation TARGET
//     binding + the pure value_at(baseValue, fraction) seam);
//   - DoubleAnimation : AnimationTimeline — From/To/By, at a target property
//     OR a transform channel (the RenderTransform component drive);
//   - TransformAnimation : DoubleAnimation — the named convenience for a
//     channel-targeting DoubleAnimation (drives the S208 RenderTransform path);
//   - DoubleAnimationUsingKeyFrames + DoubleKeyFrame /
//     LinearDoubleKeyFrame / DiscreteDoubleKeyFrame, + keyframe_value_at;
//
// Spline/Easing keyframes = recorded NO-GAP (named forward token), NOT silent.
// The document-ordered DAG rows (DoubleAnimation, keyframes, Timeline) are
// flipped PLANNED->DONE at S512a. ADR-018: ZERO new locks (all pure).

#include "vaml/core/PropertyId.h"   // PropertyId (a Double DP target)
#include "vaml/core/Transform.h"    // Matrix2D / TransformChannel compose (S208)
#include "vaml/core/export.h"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// Clock parameter value types.
// ---------------------------------------------------------------------------

// WPF FillBehavior: what happens after the active period completes.
enum class FillBehavior : std::uint8_t
{
    HoldEnd = 0, // (default) hold the end value
    Stop    = 1, // restore the base (pre-animation) value
};

// WPF RepeatBehavior: finite Count or Forever.
struct RepeatBehavior
{
    enum class Kind : std::uint8_t { Count = 0, Forever = 1 };
    Kind   kind  = Kind::Count;
    double count = 1.0; // valid when kind == Count (WPF default: one iteration)

    [[nodiscard]] static constexpr RepeatBehavior repeat_forever() noexcept
    {
        return { Kind::Forever, 0.0 };
    }
    [[nodiscard]] static constexpr RepeatBehavior repeat_for(double n) noexcept
    {
        return { Kind::Count, n };
    }
    [[nodiscard]] constexpr bool forever() const noexcept { return kind == Kind::Forever; }
};

// WPF KeyTime unit: a TimeSeconds span (0:0:1) or a Fraction of duration (50%).
enum class KeyTimeUnit : std::uint8_t { TimeSeconds = 0, Fraction = 1 };

struct KeyTime
{
    double      value = 0.0;
    KeyTimeUnit unit  = KeyTimeUnit::TimeSeconds;

    [[nodiscard]] static constexpr KeyTime Time(double s) noexcept
    {
        return { s, KeyTimeUnit::TimeSeconds };
    }
    [[nodiscard]] static constexpr KeyTime Fraction(double f) noexcept
    {
        return { f, KeyTimeUnit::Fraction };
    }

    // The keyframe's position as a fraction of the total timeline duration
    // (seconds). A TimeSeconds unit divides by total; a Fraction is passed
    // through. totalSeconds <= 0 yields 0 (guard, never NaN).
    [[nodiscard]] double to_fraction(double totalSeconds) const noexcept
    {
        if (unit == KeyTimeUnit::Fraction)
        {
            return value;
        }
        return (totalSeconds > 0.0) ? (value / totalSeconds) : 0.0;
    }
};

// Parses a KeyTime literal: a "0:0:1" time-span form (H:M:S / M:S / S) or a
// "50%" fraction form. Returns true on success (out written); false + out
// unchanged on a malformed literal (never throws). Out-of-range (negative)
// fraction is rejected (false).
VAML_CORE_EXPORT bool parse_key_time(std::string_view s, KeyTime& out) noexcept;

// The pure clock/progress authority (WPF-faithful triangle + repeat).
// `elapsed` is the clock current time relative to the animation's OWN begin
// (>= 0 in the active/fill region, < 0 in the BeforeBegin delay).
// `duration` is the one-cycle duration (>= 0). Compute the per-cycle fraction
//  in [0,1] honoring AutoReverse ping-pong and RepeatBehavior:
//   - BeforeBegin (elapsed < 0):        state.active=false, fraction=0.
//   - Fill region reached (finite repeat exhausted): active=false; fraction is
//     clamped so a HoldEnd writer can hold the terminal interpolated value.
//   - Otherwise: active=true, fraction = the current within-cycle progress
//     (autoReverse reflected).
// The caller owns the FillBehavior (write the held value vs restore base).
VAML_CORE_EXPORT bool timeline_clock_progress(double elapsed, double duration,
                                              bool autoReverse,
                                              const RepeatBehavior& repeat,
                                              double& fraction) noexcept;

// ---------------------------------------------------------------------------
// Timeline base — the animation DESCRIPTION.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT Timeline
{
public:
    virtual ~Timeline() = default;

    // BeginTime (the negative-offset "start late" semantics; see header).
    void set_begin_time(double s) noexcept { beginTime_ = s; }
    [[nodiscard]] double begin_time() const noexcept { return beginTime_; }

    void set_duration(double s) noexcept { duration_ = s; }
    [[nodiscard]] double duration() const noexcept { return duration_; }

    void set_auto_reverse(bool b) noexcept { autoReverse_ = b; }
    [[nodiscard]] bool auto_reverse() const noexcept { return autoReverse_; }

    void set_repeat_behavior(RepeatBehavior r) noexcept { repeat_ = r; }
    [[nodiscard]] const RepeatBehavior& repeat_behavior() const noexcept { return repeat_; }

    void set_fill_behavior(FillBehavior f) noexcept { fill_ = f; }
    [[nodiscard]] FillBehavior fill_behavior() const noexcept { return fill_; }

protected:
    Timeline() = default;

private:
    double         beginTime_  = 0.0;
    double         duration_   = 0.0;
    bool           autoReverse_ = false;
    RepeatBehavior repeat_{};
    FillBehavior   fill_ = FillBehavior::HoldEnd;
};

// ---------------------------------------------------------------------------
// The enum of RenderTransform COMPONENTS a TransformAnimation may drive.
// "Animate the transform's component channel + REBUILD the Matrix2D" — the
// WPF MatrixTransform-equivalent (NOT a matrix-dp animation; WPF has none).
// ---------------------------------------------------------------------------
enum class TransformChannel : std::uint8_t
{
    TranslateX = 0,
    TranslateY = 1,
    ScaleX     = 2,
    ScaleY     = 3,
    Rotate     = 4,
};

// The per-animation TARGET: what the animated double is written onto.
enum class AnimationTargetKind : std::uint8_t
{
    None            = 0, // no target configured yet
    DoubleProperty  = 1, // an element's Double dependency property (PropertyId)
    TransformChannel = 2, // an element's S208 RenderTransform component
};

// AnimationTimeline : Timeline — adds the TARGET binding + the pure
// value_at(baseValue, fraction) seam. The Storyboard (Storyboard.h) resolves
// the target element by name and writes the produced value.
class VAML_CORE_EXPORT AnimationTimeline : public Timeline
{
public:
    ~AnimationTimeline() override = default;

    // ---- Target binding ---------------------------------------------
    void set_target_name(std::string name) { targetName_ = std::move(name); }
    [[nodiscard]] const std::string& target_name() const noexcept { return targetName_; }

    // Route this animation's value onto a Double DP of the named target.
    void set_target_property(PropertyId id)
    {
        targetProp_ = id;
        targetKind_ = AnimationTargetKind::DoubleProperty;
    }

    // Route this animation's value onto a RenderTransform component.
    void set_transform_channel(TransformChannel c)
    {
        channel_    = c;
        targetKind_ = AnimationTargetKind::TransformChannel;
    }

    [[nodiscard]] AnimationTargetKind target_kind() const noexcept { return targetKind_; }
    [[nodiscard]] PropertyId target_property() const noexcept { return targetProp_; }
    [[nodiscard]] TransformChannel transform_channel() const noexcept { return channel_; }

    // The pure value at a within-cycle fraction in [0,1]. The concrete
    // subclass resolves From/To/By (DoubleAnimation) or the enclosing keyframe
    // segment (UsingKeyFrames), against the captured baseValue where relevant.
    virtual double value_at(double baseValue, double fraction) const = 0;

protected:
    AnimationTimeline() = default;

private:
    std::string         targetName_;
    AnimationTargetKind targetKind_ = AnimationTargetKind::None;
    PropertyId          targetProp_;
    TransformChannel    channel_ = TransformChannel::TranslateX;
};

// ---------------------------------------------------------------------------
// DoubleAnimation — From/To/By at a Double DP or a transform channel.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT DoubleAnimation : public AnimationTimeline
{
public:
    DoubleAnimation() = default;

    void set_from(double v) noexcept { from_ = v; hasFrom_ = true; }
    void set_to(double v) noexcept { to_ = v; hasTo_ = true; }
    void set_by(double v) noexcept { by_ = v; hasBy_ = true; }

    [[nodiscard]] bool has_from() const noexcept { return hasFrom_; }
    [[nodiscard]] bool has_to() const noexcept { return hasTo_; }
    [[nodiscard]] bool has_by() const noexcept { return hasBy_; }
    [[nodiscard]] double from() const noexcept { return from_; }
    [[nodiscard]] double to() const noexcept { return to_; }
    [[nodiscard]] double by() const noexcept { return by_; }

    // WPF-faithful From/To/By at a fraction. effectiveFrom/effectiveTo
    // default to baseValue (captured at start) when unset. `By` resolves
    // To = (hasFrom ? From : base) + By.
    double value_at(double baseValue, double fraction) const override;

private:
    double from_ = 0.0, to_ = 0.0, by_ = 0.0;
    bool   hasFrom_ = false, hasTo_ = false, hasBy_ = false;
};

// ---------------------------------------------------------------------------
// TransformAnimation — the named convenience for a channel-targeting
// DoubleAnimation (drives the S208 RenderTransform path). Thin + real: set the
// channel once, the S510 machinery rebuilds the Matrix2D each tick.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT TransformAnimation : public DoubleAnimation
{
public:
    TransformAnimation() = default;
    void set_channel(TransformChannel c) { set_transform_channel(c); }
};

// ---------------------------------------------------------------------------
// Keyframes — DoubleAnimationUsingKeyFrames + the key frame types.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT DoubleKeyFrame
{
public:
    DoubleKeyFrame() = default;
    DoubleKeyFrame(double value, KeyTime kt) : value_(value), keyTime_(kt) {}
    virtual ~DoubleKeyFrame() = default;

    [[nodiscard]] double value() const noexcept { return value_; }
    [[nodiscard]] KeyTime key_time() const noexcept { return keyTime_; }

    // true = Linear (interpolate the enclosing segment); false = Discrete (hop).
    [[nodiscard]] virtual bool is_linear() const noexcept = 0;

private:
    double  value_;
    KeyTime keyTime_;
};

class VAML_CORE_EXPORT LinearDoubleKeyFrame final : public DoubleKeyFrame
{
public:
    using DoubleKeyFrame::DoubleKeyFrame;
    [[nodiscard]] bool is_linear() const noexcept override { return true; }
};

class VAML_CORE_EXPORT DiscreteDoubleKeyFrame final : public DoubleKeyFrame
{
public:
    using DoubleKeyFrame::DoubleKeyFrame;
    [[nodiscard]] bool is_linear() const noexcept override { return false; }
};

class VAML_CORE_EXPORT DoubleAnimationUsingKeyFrames : public AnimationTimeline
{
public:
    DoubleAnimationUsingKeyFrames() = default;

    // Adds an OWNED key frame in authored order. A key frame whose KeyTime
    // fraction is EARLIER than a previously added frame is an authoring error
    // (out-of-order, WPF-illegal) -> throws std::invalid_argument (fail loud,
    // never silent).
    void add_key_frame(std::unique_ptr<DoubleKeyFrame> frame);

    [[nodiscard]] std::size_t key_frame_count() const noexcept { return frames_.size(); }

    // Keyframe value at a whole-timeline fraction in [0,1] (the fraction the
    // Storyboard computes over the animation's duration). Linear interpolates
    // the enclosing segment; Discrete holds until the next keyframe's KeyTime.
    double keyframe_value_at(double fraction) const;

    double value_at(double baseValue, double fraction) const override;

private:
    std::vector<std::unique_ptr<DoubleKeyFrame>> frames_;
};

// ---------------------------------------------------------------------------
// Channel -> Matrix2D compose seam (pure; the S208 path).
// ---------------------------------------------------------------------------
// Builds the matrix for ONE animated channel at `value` (translations in
// layout px, ScaleX/Y as multipliers, Rotate in degrees). The Storyboard
// composes it with the captured base transform (multiply(base, channelM)).
// Exported so tests assert the per-channel math + the composed result.
VAML_CORE_EXPORT Matrix2D channel_matrix(TransformChannel channel, double value) noexcept;

} // namespace vaml
