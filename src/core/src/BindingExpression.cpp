// S242 — BindingExpression (dep:S241). Sprint 6.
// Implements the S242 headline: the binding expression's mode, the converter
// wire, the Class B fallback, and the first source->target transfer. The
// transfer runs the S240 evaluator over the S241 ObservableObject reader,
// applies the converter, applies the fallback on a Class B outcome or converter
// failure, then pushes to the target DP.
//
// S243 (dep:S242) added the LIVE ONE-WAY source->target subscription + the
// SetTargetFailed emission (the S240_1 forward arm).
//
// S244 (this step, dep:S243) adds the TARGET->SOURCE WRITE-BACK of a TwoWay
// binding + the convertBack reverse wire + the WriteBackRejected emission (the
// S240_1 forward arm): the forward (source->target) wire is now armed for BOTH
// OneWay and TwoWay (shared S243 layer), and a TwoWay target-change (a TextBox
// edit landing on the target DP) writes the target value back to the source.
// See BindingExpression.h for the full contract.

#include "vaml/core/BindingExpression.h"

#include <string>

namespace vaml {

namespace {

// Human-readable "what" for a BindingPolicyException (the S082/S091/S094
// structured-error convention: name the authored fragment in the message).
std::string policy_what(PropertyPathError kind, const std::string& path)
{
    const char* kindName = nullptr;
    switch (kind)
    {
    case PropertyPathError::Empty:               kindName = "empty property path"; break;
    case PropertyPathError::TrailingDot:         kindName = "trailing dot"; break;
    case PropertyPathError::ConsecutiveDot:      kindName = "consecutive dots"; break;
    case PropertyPathError::UnterminatedIndexer: kindName = "unterminated indexer"; break;
    case PropertyPathError::EmptyIndexer:        kindName = "empty indexer"; break;
    case PropertyPathError::MalformedIndex:      kindName = "malformed index"; break;
    case PropertyPathError::UnclosedQuote:       kindName = "unclosed quote"; break;
    default:                                     kindName = "invalid property path"; break;
    }
    return std::string("BindingPolicyException (Strict): ") + kindName + " in '" + path + "'";
}

// The leading path segment's property name — the key the live one-way
// subscription is armed on (a scalar-led VM path; multi-hop re-observation is
// a S252/S244+ concern, recorded not silent). Returns the empty view when the
// first segment is an indexer (a subscription is then NOT armed — see
// arm_subscription).
std::string_view first_segment_name(const PropertyPath& path)
{
    if (path.segment_count() == 0)
    {
        return {};
    }
    const auto& seg = path.segments().front();
    if (seg.kind != PropertySegmentKind::Property)
    {
        return {};
    }
    return seg.name;
}

} // namespace

BindingPolicyException::BindingPolicyException(PropertyPathError kind, std::string path)
    : std::logic_error(policy_what(kind, path)),
      kind_(kind),
      path_(std::move(path))
{
}

BindingDirection to_binding_direction(BindingMode mode) noexcept
{
    switch (mode)
    {
        case BindingMode::OneWay:  return BindingDirection::OneWay;
        case BindingMode::TwoWay:  return BindingDirection::TwoWay;
        case BindingMode::OneTime: break;
    }
    return BindingDirection::OneTime;
}

BindingExpression::BindingExpression(BindingExpression&& other) noexcept
    : bindingId_(other.bindingId_),
      mode_(other.mode_),
      policy_(other.policy_),
      source_(other.source_),
      target_(other.target_),
      sourcePath_(std::move(other.sourcePath_)),
      targetProp_(std::move(other.targetProp_)),
      converter_(std::move(other.converter_)),
      converterName_(std::move(other.converterName_)),
      fallback_(std::move(other.fallback_)),
      sourceType_(std::move(other.sourceType_)),
      targetType_(std::move(other.targetType_)),
      path_(std::move(other.path_)),
      subscribed_(other.subscribed_),
      liveToken_(other.liveToken_),
      writeback_(other.writeback_),
      targetToken_(other.targetToken_)
{
    // Transfer the live subscriptions onto THIS instance so a moved binding
    // stays live. Each handler captures `this`, so it must point at the
    // moved-to object, never the dangling moved-from: remove the old
    // (moved-from-capturing) tokens from the UNCHANGED source/target, then
    // re-arm new tokens capturing `this`. Mark the moved-from inert so its dtor
    // does not double-remove.
    other.subscribed_ = false;
    other.liveToken_  = 0;
    other.writeback_  = false;
    other.targetToken_= 0;

    std::string_view forwardKey{};
    if (subscribed_ && source_ != nullptr)
    {
        forwardKey = first_segment_name(path_);
        source_->remove_property_changed(liveToken_);
        liveToken_ = source_->add_property_changed(
            PropertyId::of(forwardKey),
            [this](PropertyId, const Value&, const Value&) { transfer_one_way(); });
    }
    if (writeback_ && target_ != nullptr)
    {
        target_->remove_property_changed(targetToken_);
        targetToken_ = target_->add_property_changed(
            PropertyId::of(targetProp_),
            [this](PropertyId, const Value&, const Value&) { transfer_write_back(); });
    }
}

BindingExpression& BindingExpression::operator=(BindingExpression&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    unsubscribe();
    bindingId_    = other.bindingId_;
    mode_         = other.mode_;
    policy_       = other.policy_;
    source_       = other.source_;
    target_       = other.target_;
    sourcePath_   = std::move(other.sourcePath_);
    targetProp_   = std::move(other.targetProp_);
    converter_    = std::move(other.converter_);
    converterName_= std::move(other.converterName_);
    fallback_     = std::move(other.fallback_);
    sourceType_   = std::move(other.sourceType_);
    targetType_   = std::move(other.targetType_);
    path_         = std::move(other.path_);
    subscribed_   = other.subscribed_;
    liveToken_    = other.liveToken_;
    writeback_    = other.writeback_;
    targetToken_  = other.targetToken_;
    other.subscribed_ = false;
    other.liveToken_  = 0;
    other.writeback_  = false;
    other.targetToken_= 0;
    if (subscribed_ && source_ != nullptr)
    {
        source_->remove_property_changed(liveToken_);
        liveToken_ = source_->add_property_changed(
            PropertyId::of(first_segment_name(path_)),
            [this](PropertyId, const Value&, const Value&) { transfer_one_way(); });
    }
    if (writeback_ && target_ != nullptr)
    {
        target_->remove_property_changed(targetToken_);
        targetToken_ = target_->add_property_changed(
            PropertyId::of(targetProp_),
            [this](PropertyId, const Value&, const Value&) { transfer_write_back(); });
    }
    return *this;
}

BindingExpression::~BindingExpression()
{
    unsubscribe();        // S243 forward wire
    unsubscribe_writeback(); // S244 reverse wire
}

namespace {

// The Level 1 reader: an ObservableObject transported as Value::UntypedPtr is
// read through the S241 observable_object_reader(); anything else is
// NotReadable. This is exactly the S240 injectable-seam contract with the VM
// source wired in (the S241 reader maps absent -> MissingProperty, non-final
// null -> NullIntermediate, index -> NotReadable).
PropertyReader source_reader()
{
    return [](const Value& node, const PropertySegment& segment) -> PropertyRead {
        if (node.type() != ValueType::UntypedPtr)
        {
            return PropertyRead{false, PropertyPathError::NotReadable, Value::Null(), 0};
        }
        return vaml::observable_object_reader()(node, segment);
    };
}

// Monotonic binding id. ADR-018: a function-local counter is the only state and
// is not shared across threads (a binding is created and used on one thread).
std::uint64_t next_binding_id() noexcept
{
    static std::uint64_t id = 0;
    ++id;
    return id;
}

} // namespace

std::unique_ptr<BindingExpression> BindingExpression::create(const BindingExpressionSpec& spec)
{
    // --- Class A gate ------------------------------------------------------
    PropertyPath path;
    PropertyPathError parseErr = PropertyPathError::Empty;
    if (!PropertyPath::try_parse(spec.sourcePath, path, &parseErr))
    {
        // Class A never falls back; no bind. ALWAYS emit the diagnostic first
        // (observation, ADR-016 — it is information, never the control flow),
        // then apply the policy:
        //   - Fallback: return nullptr (the S242 behavior).
        //   - Strict: THROW BindingPolicyException (bind-time throw, S242_1).
        // Class B is never reached here (create only parses), so create never
        // throws for Class B under any policy.
        BindingDiagnostic d = diagnostic_from_path_error(
            next_binding_id(), spec.sourcePath, spec.targetProp,
            to_binding_direction(spec.mode), spec.sourceType, spec.targetType,
            parseErr, "BindingExpression::create rejected the path", {}, 0);
        binding_diagnostics().emit(d);

        if (spec.policy == BindingPolicy::Strict)
        {
            throw BindingPolicyException(parseErr, spec.sourcePath);
        }
        return nullptr;
    }

    // --- Live expression ---------------------------------------------------
    std::unique_ptr<BindingExpression> e(new BindingExpression());
    e->bindingId_   = next_binding_id();
    e->mode_        = spec.mode;
    e->policy_      = spec.policy;
    e->source_      = spec.source;
    e->target_      = spec.target;
    e->sourcePath_  = spec.sourcePath;
    e->targetProp_  = spec.targetProp;
    e->converter_   = spec.converter;
    e->converterName_ = spec.converterName;
    e->fallback_    = spec.fallback;
    e->sourceType_  = spec.sourceType;
    e->targetType_  = spec.targetType;
    e->path_        = std::move(path);
    e->arm_subscription();   // S243 forward wire (OneWay OR TwoWay in S244)
    e->arm_writeback();      // S244 reverse wire (TwoWay only)
    return e;
}

void BindingExpression::unsubscribe()
{
    if (subscribed_ && source_ != nullptr)
    {
        // Idempotent, global token-based: removing an already-removed/unknown
        // token is a no-op (S241), so this is safe even if the token was
        // superseded by a move.
        source_->remove_property_changed(liveToken_);
    }
    subscribed_ = false;
    liveToken_  = 0;
}

void BindingExpression::arm_subscription()
{
    // S243 armed OneWay only. S244 broadens the arm to TwoWay too: TwoWay's
    // FORWARD (source->target) wire is the exact S243 layer, reused unchanged
    // (the shared wire — no duplication). OneTime never subscribes.
    if (mode_ != BindingMode::OneWay && mode_ != BindingMode::TwoWay)
    {
        return;
    }
    if (subscribed_)
    {
        return; // already armed (idempotent)
    }
    if (source_ == nullptr)
    {
        return; // no observable source to subscribe to (Class B NotReadable path)
    }
    const std::string_view leading = first_segment_name(path_);
    if (leading.empty())
    {
        return; // empty path or indexer-led: nothing to key the subscription on
    }
    liveToken_ = source_->add_property_changed(
        PropertyId::of(leading),
        [this](PropertyId, const Value&, const Value&) { transfer_one_way(); });
    subscribed_ = true;
}

void BindingExpression::transfer_one_way()
{
    // The S243 live re-push: run the exact S242 pipeline; the SetTargetFailed
    // emission is S243's arm (gated to the live one-way path so S242's OneTime
    // no-emission tests stay green). Class B / target rejection never throws.
    const TransferResult r = TransferSourceToTarget();
    if (r != TransferResult::TargetRejected)
    {
        return;
    }
    BindingDiagnostic d;
    d.bindingId  = bindingId_;
    d.sourcePath = sourcePath_;
    d.targetProp = targetProp_;
    d.direction  = to_binding_direction(mode_);
    d.reasonCode = BindingReasonCode::SetTargetFailed;
    d.sourceType = sourceType_;
    d.targetType = targetType_;
    d.converter  = converterName_;
    d.timestamp  = std::chrono::system_clock::now();
    binding_diagnostics().emit(d);
}

void BindingExpression::unsubscribe_writeback()
{
    if (writeback_ && target_ != nullptr)
    {
        // Idempotent, global token-based: removing an already-removed/unknown
        // token is a no-op (S041), so this is safe even if the token was
        // superseded by a move.
        target_->remove_property_changed(targetToken_);
    }
    writeback_   = false;
    targetToken_ = 0;
}

void BindingExpression::arm_writeback()
{
    // Only TwoWay arms a target->source WRITE-BACK. OneWay/OneTime never write
    // back (their writes are one direction only). The FORWARD wire for TwoWay
    // is armed separately by arm_subscription (the shared S243 layer).
    if (mode_ != BindingMode::TwoWay)
    {
        return;
    }
    if (writeback_)
    {
        return; // already armed (idempotent)
    }
    if (target_ == nullptr || targetProp_.empty())
    {
        return; // no target property to observe (nothing to write back from)
    }
    // No leading source key -> a write-back would have nowhere to land; the
    // arm still holds (the key check is the write-back path's, not the arm's)
    // so the introspection reflects "armed to observe the target".
    targetToken_ = target_->add_property_changed(
        PropertyId::of(targetProp_),
        [this](PropertyId, const Value&, const Value&) { transfer_write_back(); });
    writeback_ = true;
}

void BindingExpression::transfer_write_back()
{
    // The S244 TARGET->SOURCE write-back body. Runs when the target DP fires a
    // REAL change (a TextBox edit landing on the target DP). Class B: a
    // rejection is emitted (WriteBackRejected), NEVER thrown — the target value
    // is not written to the source on a rejection.
    //
    // Loop-safety (the forward direction): writing back to a source that
    // already HOLDS the value makes ObservableObject::Set return false (the
    // real-change discipline, S241) -> no source signal -> the forward wire
    // does not re-fire. And a forward push via TransferValue stays under the
    // `transferring_` guard, so a target change raised WHILE the write-back is
    // landing cannot echo back. Both legs are BOUNDED (proven by a
    // bounded-cycle test).

    // IsInTransfer-equivalent guard: while a forward source->target transfer
    // is in flight, a target change is NOT written back (authoritative WPF
    // primitive — suppresses the non-injective round-trip, e.g. a forward push
    // that lands on the target with a value different from the user's typed
    // value). Suppressed here, never emitted, never stored.
    if (transferring_)
    {
        return;
    }

    // 1. Read the target's current value.
    Value value = Value::Null();
    if (target_ == nullptr || targetProp_.empty())
    {
        emit_write_back_rejected("no target");
        return;
    }
    try
    {
        value = target_->Get(PropertyId::of(targetProp_));
    }
    catch (const PropertyNotFound&)
    {
        emit_write_back_rejected("target property not found");
        return;
    }

    // 2. Reverse converter wire (S244 resolves S242's convertBack seam).
    if (converter_.convertBack)
    {
        BindingConversion conv;
        try
        {
            conv = converter_.convertBack(value);
        }
        catch (const std::exception& ex)
        {
            conv.ok = false;
            conv.detail = ex.what();
        }
        if (!conv.ok)
        {
            emit_write_back_rejected(conv.detail);
            return; // target value NOT written to source
        }
        value = conv.value;
    }

    // 3. Write back to the source via the FIRST PATH SEGMENT's key (a
    //    scalar-led VM path; multi-hop write-back re-observation is a
    //    S252/S244+ concern, recorded not silent).
    if (source_ == nullptr)
    {
        emit_write_back_rejected("no source to write back to");
        return;
    }
    const std::string_view key = first_segment_name(path_);
    if (key.empty())
    {
        emit_write_back_rejected("no source key (indexer/empty path)");
        return;
    }
    try
    {
        (void)source_->Set(PropertyId::of(key), value);
    }
    catch (const ValueTypeMismatch&)
    {
        emit_write_back_rejected("value type mismatch on write-back");
    }
}

void BindingExpression::emit_write_back_rejected(const std::string& detail)
{
    // The S240_1 WriteBackRejected FORWARD ARM, bound here (Class B,
    // observation never control-flow per ADR-016 — never a throw). This is
    // gated to the LIVE TwoWay write-back path: the caller-driven first push
    // and the OneWay/OneTime paths never reach here.
    BindingDiagnostic d;
    d.bindingId  = bindingId_;
    d.sourcePath = sourcePath_;
    d.targetProp = targetProp_;
    d.direction  = to_binding_direction(mode_);
    d.reasonCode = BindingReasonCode::WriteBackRejected;
    d.sourceType = sourceType_;
    d.targetType = targetType_;
    d.converter  = converterName_;
    d.detail     = detail;
    d.timestamp  = std::chrono::system_clock::now();
    binding_diagnostics().emit(d);
}

bool BindingExpression::apply_fallback()
{
    if (!fallback_.has_value())
    {
        return false;
    }

    // Emit FallbackApplied (the Class B failure reason was already emitted by
    // the caller before this path). Carries converter name + detail.
    BindingDiagnostic d;
    d.bindingId  = bindingId_;
    d.sourcePath = sourcePath_;
    d.targetProp = targetProp_;
    d.direction  = to_binding_direction(mode_);
    d.reasonCode = BindingReasonCode::FallbackApplied;
    d.sourceType = sourceType_;
    d.targetType = targetType_;
    d.converter  = converterName_;
    d.timestamp  = std::chrono::system_clock::now();
    binding_diagnostics().emit(d);

    // Apply the fallback to the target. A wrong-typed fallback Set is mapped to
    // TargetRejected WITHOUT a SetTargetFailed emission (S243's arm).
    (void)apply_to_target(*fallback_);
    return true;
}

TransferResult BindingExpression::apply_to_target(const Value& value)
{
    if (target_ == nullptr || targetProp_.empty())
    {
        // No target property to push to -> cannot apply. Not a SetTargetFailed
        // (S243's arm) — a defensive TargetRejected, recorded, never emitted.
        return TransferResult::TargetRejected;
    }

    const PropertyId id = PropertyId::of(targetProp_);
    try
    {
        const SetResult r = target_->Set(id, value);
        switch (r)
        {
            case SetResult::APPLIED:   return TransferResult::Applied;
            case SetResult::NO_CHANGE: return TransferResult::NoChange;
            case SetResult::QUEUED:    return TransferResult::TargetRejected; // wrong-thread queued (S242: no emission)
            case SetResult::REJECTED:  return TransferResult::TargetRejected;
        }
    }
    catch (const ValueTypeMismatch&)
    {
        return TransferResult::TargetRejected; // wrong-typed push (no emission)
    }
    catch (const PropertyNotFound&)
    {
        return TransferResult::TargetRejected; // unregistered target prop (no emission)
    }
    catch (const PropertyReadOnlyException&)
    {
        return TransferResult::TargetRejected; // read-only target (no emission)
    }
    return TransferResult::TargetRejected;
}

TransferResult BindingExpression::TransferSourceToTarget()
{
    // S244 IsInTransfer-equivalent guard: the WHOLE forward transfer (first
    // push AND live re-push) runs under `transferring_`, so a target change
    // raised DURING the forward apply cannot write back to the source (the
    // authoritative WPF primitive; closes the non-injective round-trip gap).
    struct TransferGuard
    {
        bool& flag;
        explicit TransferGuard(bool& f) : flag(f) { flag = true; }
        ~TransferGuard() { flag = false; }
    } guard(transferring_);

    // --- Resolve the source (S240 evaluator over the S241 reader) ----------
    PropertyRead read;
    {
        const Value root = (source_ != nullptr) ? Value::UntypedPtr(source_) : Value::Null();
        read = evaluate_property_path(root, path_, source_reader());
    }

    if (!read.ok)
    {
        // Emit the Class B resolve reason.
        BindingDiagnostic rd = diagnostic_from_property_read(
            bindingId_, targetProp_, to_binding_direction(mode_),
            sourceType_, targetType_, converterName_, path_, read);
        binding_diagnostics().emit(rd);

        // Fallback ONLY on a Class B outcome (never on Class A — create() gated
        // those out). NullIntermediate/MissingProperty/NotReadable are B.
        if (!fallback_.has_value())
        {
            return TransferResult::ResolveFailed;
        }
        if (apply_fallback())
        {
            return TransferResult::FallbackApplied;
        }
        return TransferResult::ResolveFailed;
    }

    // --- Converter (S242 wire: source -> target; convertBack is S244's) -----
    if (converter_.convert)
    {
        BindingConversion conv;
        try
        {
            conv = converter_.convert(read.value);
        }
        catch (const std::exception& ex)
        {
            conv.ok = false;
            conv.detail = ex.what();
        }
        if (!conv.ok)
        {
            BindingDiagnostic cd;
            cd.bindingId  = bindingId_;
            cd.sourcePath = sourcePath_;
            cd.targetProp = targetProp_;
            cd.direction  = to_binding_direction(mode_);
            cd.reasonCode = BindingReasonCode::ConverterFailed;
            cd.sourceType = sourceType_;
            cd.targetType = targetType_;
            cd.converter  = converterName_;
            cd.detail     = conv.detail;
            cd.timestamp  = std::chrono::system_clock::now();
            binding_diagnostics().emit(cd);

            if (!fallback_.has_value())
            {
                return TransferResult::ConverterFailed;
            }
            if (apply_fallback())
            {
                return TransferResult::FallbackApplied;
            }
            return TransferResult::ConverterFailed;
        }
        return apply_to_target(conv.value);
    }

    // --- No converter: push the resolved value directly --------------------
    return apply_to_target(read.value);
}

} // namespace vaml
