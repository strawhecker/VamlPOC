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
// S244 (this step, dep:S243) added the TARGET->SOURCE WRITE-BACK of a TwoWay
// binding + the convertBack reverse wire + the WriteBackRejected emission (the
// S240_1 forward arm): the forward (source->target) wire is now armed for BOTH
// OneWay and TwoWay (shared S243 layer), and a TwoWay target-change (a TextBox
// edit landing on the target DP) writes the target value back to the source.
// See BindingExpression.h for the full contract.
//
// S811 (this corrective, dep:S810) RECASTS the S244 target->source write-back
// from a per-property subscriber subscription on the target DependencyObject
// (DependencyObject::add_property_changed — REMOVED) onto the WPF-faithful
// EXPRESSION-AS-VALUE primitive: BindingExpression implements
// DependencyPropertyExpression, registers itself as the target DP's current
// expression, and intercepts a subsequent target Set() via SetValue →
// transfer_write_back (WPF SetValueCommon DO.cs:705-708 →
// BindingExpressionBase.SetValue BindingExpressionBase.cs:397-414). See
// BindingExpression.h for the full contract.

#include "vaml/core/BindingExpression.h"
#include "vaml/core/DependencyProperty.h" // S803 composite target-DP resolution (ResolveForElement)

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

// S803 composite target-DP resolution. Historically the binding built
// `PropertyId::of(targetProp_, targetOwner_)` verbatim, which MISSES a composite
// key authored as a DOTTED targetProp with an EMPTY targetOwner (e.g.
// `TextBox.Text` → the DP lives under ("Text","TextBox")). WPF walks the owning
// Type (FromName); this resolves:
//   - non-empty targetOwner → the explicit composite pair (unchanged);
//   - empty owner + dotted `A.B` → (name=B, owner=A);
//   - empty owner + bare name → the guarded unique-name bridge on the universal
//     UIElement base (single-owner synthetic DPs resolve; ambiguous stays null);
//   - otherwise → the verbatim pair (fallback, keeps owner-carrying specs).
PropertyId resolve_target_id(const std::string& prop, const std::string& owner)
{
    const std::string_view propView = prop;
    const std::string_view ownerView = owner;
    if (!ownerView.empty())
    {
        return PropertyId::of(propView, ownerView);
    }
    const std::size_t dot = propView.rfind('.');
    if (dot != std::string_view::npos && dot != 0 && dot + 1 < propView.size())
    {
        return PropertyId::of(propView.substr(dot + 1), propView.substr(0, dot));
    }
    if (const vaml::DependencyProperty* dp =
            vaml::DependencyProperty::ResolveForElement(propView, "UIElement"))
    {
        return dp->property_id();
    }
    return PropertyId::of(propView, ownerView);
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
      targetOwner_(std::move(other.targetOwner_)),
      converter_(std::move(other.converter_)),
      converterName_(std::move(other.converterName_)),
      fallback_(std::move(other.fallback_)),
      sourceType_(std::move(other.sourceType_)),
      targetType_(std::move(other.targetType_)),
      path_(std::move(other.path_)),
      subscribed_(other.subscribed_),
      liveToken_(other.liveToken_),
      writeback_(other.writeback_),
      detached_(other.detached_),
      detachedEmitted_(other.detachedEmitted_)
{
    // Transfer the live wires onto THIS instance so a moved binding stays live.
    // Each handler / expression register captures `this`, so it must point at
    // the moved-to object, never the dangling moved-from: remove the old
    // (moved-from-capturing) tokens from the UNCHANGED source/target, then
    // re-arm new ones capturing `this`. Mark the moved-from inert so its dtor
    // does not double-remove.
    other.subscribed_ = false;
    other.liveToken_  = 0;
    other.writeback_  = false;
    other.detached_   = false;
    other.detachedEmitted_ = false;

    if (!detached_)
    {
        std::string_view forwardKey{};
        if (subscribed_ && source_ != nullptr)
        {
            forwardKey = first_segment_name(path_);
            source_->remove_property_changed(liveToken_);
            liveToken_ = source_->add_property_changed(
                PropertyId::of(forwardKey, "ObservableObject"),
                [this](PropertyId, const Value&, const Value&) { transfer_one_way(); });
        }
        // S811: re-REGISTER this expression as the target DP's current value
        // (the write-back seam — was: target_->add_property_changed).
        if (writeback_ && target_ != nullptr)
        {
            target_->set_current_expression(resolve_target_id(targetProp_, targetOwner_), this);
        }
        // Re-arm the S245 destruction wire: a moved binding keeps auto-detaching
        // when its target element is destroyed.
        if (target_ != nullptr)
        {
            target_->remove_destroyed(other.destroyedToken_); // old (moved-from-capturing) token
            destroyedToken_ = target_->add_destroyed([this]() { on_target_destroyed(); });
            destroyedArmed_ = true;
        }
    }
    other.destroyedArmed_ = false;
    other.destroyedToken_ = 0;
}

BindingExpression& BindingExpression::operator=(BindingExpression&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    unsubscribe();
    unsubscribe_writeback();
    unsubscribe_destroyed();
    bindingId_    = other.bindingId_;
    mode_         = other.mode_;
    policy_       = other.policy_;
    source_       = other.source_;
    target_       = other.target_;
    sourcePath_   = std::move(other.sourcePath_);
    targetProp_   = std::move(other.targetProp_);
    targetOwner_  = std::move(other.targetOwner_);
    converter_    = std::move(other.converter_);
    converterName_= std::move(other.converterName_);
    fallback_     = std::move(other.fallback_);
    sourceType_   = std::move(other.sourceType_);
    targetType_   = std::move(other.targetType_);
    path_         = std::move(other.path_);
    subscribed_   = other.subscribed_;
    liveToken_    = other.liveToken_;
    writeback_    = other.writeback_;
    detached_     = other.detached_;
    detachedEmitted_ = other.detachedEmitted_;
    other.subscribed_ = false;
    other.liveToken_  = 0;
    other.writeback_  = false;
    other.detached_   = false;
    other.detachedEmitted_ = false;
    if (!detached_)
    {
        if (subscribed_ && source_ != nullptr)
        {
            source_->remove_property_changed(liveToken_);
            liveToken_ = source_->add_property_changed(
                PropertyId::of(first_segment_name(path_), "ObservableObject"),
                [this](PropertyId, const Value&, const Value&) { transfer_one_way(); });
        }
        if (writeback_ && target_ != nullptr)
        {
            target_->set_current_expression(resolve_target_id(targetProp_, targetOwner_), this);
        }
        if (target_ != nullptr)
        {
            target_->remove_destroyed(other.destroyedToken_);
            destroyedToken_ = target_->add_destroyed([this]() { on_target_destroyed(); });
            destroyedArmed_ = true;
        }
    }
    other.destroyedArmed_ = false;
    other.destroyedToken_ = 0;
    return *this;
}

BindingExpression::~BindingExpression()
{
    unsubscribe();        // S243 forward wire
    unsubscribe_writeback(); // S244 reverse wire (S811: expression registration)
    unsubscribe_destroyed(); // S245 destruction wire
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
    e->targetOwner_ = spec.targetOwner;
    e->converter_   = spec.converter;
    e->converterName_ = spec.converterName;
    e->fallback_    = spec.fallback;
    e->sourceType_  = spec.sourceType;
    e->targetType_  = spec.targetType;
    e->path_        = std::move(path);
    e->arm_subscription();   // S243 forward wire (OneWay OR TwoWay in S244)
    e->arm_writeback();      // S244 reverse wire (TwoWay only), S811-recast
    e->arm_destroyed();      // S245 destruction wire (detach on element destroy)
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
        PropertyId::of(leading, "ObservableObject"),
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
    // S811: the write-back is the expression-as-value registration. Removing it
    // makes the target treat the property as an unbound local again (no
    // SetValue interception). Idempotent.
    if (writeback_ && target_ != nullptr)
    {
        target_->set_current_expression(resolve_target_id(targetProp_, targetOwner_), nullptr);
    }
    writeback_ = false;
}

// --- S245 detach-on-element-destroy lifecycle ------------------------------

void BindingExpression::arm_destroyed()
{
    // S245: whenever there is a target ELEMENT, subscribe to its destruction
    // notification (DependencyObject::add_destroyed). Firing means the target is
    // being destroyed — the binding detaches BEFORE the target's memory is
    // reclaimed, so no later write can dereference a freed target. Idempotent;
    // no-op when already armed, when there is no target, or when detached.
    if (detached_ || destroyedArmed_ || target_ == nullptr)
    {
        return;
    }
    destroyedToken_ = target_->add_destroyed([this]() { on_target_destroyed(); });
    destroyedArmed_ = true;
}

void BindingExpression::unsubscribe_destroyed()
{
    if (destroyedArmed_ && target_ != nullptr)
    {
        // Idempotent, global token-based (mirrors remove_property_changed).
        target_->remove_destroyed(destroyedToken_);
    }
    destroyedArmed_ = false;
    destroyedToken_ = 0;
}

void BindingExpression::on_target_destroyed()
{
    // The target ELEMENT is being destroyed (fired from its destructor, before
    // member teardown). Detach so this binding never writes to a freed target.
    // Detach() is idempotent, no-throw, and emits nothing (normal teardown).
    Detach();
}

void BindingExpression::Detach()
{
    // Idempotent: a detached binding stays detached. Normal teardown — emits
    // NOTHING here (the `Detached` lifecycle emission is gated to an ATTEMPTED
    // WRITE on an already-detached binding, never to this call).
    if (detached_)
    {
        return;
    }
    unsubscribe();          // S243 forward wire
    unsubscribe_writeback(); // S244 reverse wire (S811: expression registration)
    unsubscribe_destroyed(); // S245 destruction wire
    detached_ = true;
}

void BindingExpression::emit_detached()
{
    // The S240_1 `Detached` FORWARD ARM (Class B, a SILENCED non-error), bound
    // here: emitted ONCE per binding when a write is attempted on an
    // already-detached binding (report only so a subscribed sink can observe
    // the lifecycle — never control-flow per ADR-016, never a throw).
    if (detachedEmitted_)
    {
        return;
    }
    detachedEmitted_ = true;
    BindingDiagnostic d;
    d.bindingId  = bindingId_;
    d.sourcePath = sourcePath_;
    d.targetProp = targetProp_;
    d.direction  = to_binding_direction(mode_);
    d.reasonCode = BindingReasonCode::Detached;
    d.sourceType = sourceType_;
    d.targetType = targetType_;
    d.converter  = converterName_;
    d.timestamp  = std::chrono::system_clock::now();
    binding_diagnostics().emit(d);
}

void BindingExpression::arm_writeback()
{
    // S811-recast: only a TwoWay binding registers itself as the target DP's
    // current expression (the write-back seam). OneWay/OneTime never write back
    // (their writes are one direction only) — they are NOT registered, so a
    // target Set is NOT routed to them. The FORWARD wire for TwoWay is armed
    // separately by arm_subscription (the shared S243 layer).
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
        return; // no target property (nothing to write back from)
    }
    // Register this expression as the target DP's current value. A subsequent
    // target_->Set(...) (a TextBox edit) is offered to SetValue → write-back.
    target_->set_current_expression(resolve_target_id(targetProp_, targetOwner_), this);
    writeback_ = true;
}

bool BindingExpression::SetValue(const Value& value)
{
    // S811: the WPF BindingExpressionBase.SetValue (BindingExpressionBase.cs:397-414)
    // "handles" a target Set when the binding is reflective: the incoming VALUE
    // (a TextBox edit) is written back to the source. In VAML a binding is
    // reflective (writes back to the source) ONLY when it is TwoWay. For a
    // non-reflective (OneWay/OneTime) binding, return false so the caller
    // (DependencyObject::Set) falls through to the plain store and detaches
    // this expression (WPF's `return false` branch, BindingExpressionBase.cs:412).
    if (mode_ != BindingMode::TwoWay)
    {
        return false;
    }
    if (detached_)
    {
        emit_detached();
        // Detached: the binding is inert — decline so the caller stores the
        // value directly (and detaches the expression).
        return false;
    }
    // Handle: write the incoming target-side value back to the source.
    transfer_write_back(value);
    return true;
}

void BindingExpression::transfer_write_back(const Value& incoming)
{
    // The S244 TARGET->SOURCE write-back body. Runs when the target DP fires a
    // Set() that was routed through SetValue (a TextBox edit landing on the
    // target DP); `incoming` is the NEW value from that edit. Class B: a
    // rejection is emitted (WriteBackRejected), NEVER thrown — the target value
    // is not written to the source on a rejection.

    // IsInTransfer-equivalent guard: while a forward source->target transfer
    // is in flight, a target change is NOT written back (authoritative WPF
    // primitive — suppresses the non-injective round-trip, e.g. a forward push
    // that lands on the target with a value different from the user's typed
    // value). Suppressed here, never emitted, never stored.
    if (transferring_)
    {
        return;
    }

    // 1. The value to write back is the incoming edit value (not a stale re-read
    //    of the target store — the store has not been updated yet for this edit).
    Value value = incoming;

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
        (void)source_->Set(PropertyId::of(key, "ObservableObject"), value);
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

    const PropertyId id = resolve_target_id(targetProp_, targetOwner_);
    try
    {
        const SetResult r = target_->Set(id, value);
        switch (r)
        {
            case SetResult::APPLIED:   return TransferResult::Applied;
            case SetResult::NO_CHANGE: return TransferResult::NoChange;
            case SetResult::REJECTED:  return TransferResult::TargetRejected; // S810_1: kept only as a compile-time-known unreachable
                // arm — the Set pipeline no longer RETURNS REJECTED for a
                // validate refusal (it throws InvalidPropertyValueException,
                // caught below). S811 removed SetResult::QUEUED (S037's
                // wrong-thread value queueing tail).
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
    catch (const InvalidPropertyValueException&)
    {
        // S810_1: the core's out-of-range VALIDATE refusal arrives as a thrown
        // InvalidPropertyValueException (WPF ArgumentException(InvalidPropertyValue)
        // at DO.cs:653/1692). This is the S242 binding-boundary conversion
        // (ADR-016): we CATCH the core throw and map it to
        // TransferResult::TargetRejected — the binding's reporting contract
        // (TransferResult::TargetRejected) is preserved. The throw is converted
        // HERE at the binding boundary, never suppressed in the core. No
        // SetTargetFailed emission (S243's arm).
        return TransferResult::TargetRejected;
    }
    catch (const WrongThreadException&)
    {
        // S811: a wrong-thread target Set THROWS (WPF VerifyAccess). Map the
        // throw to TargetRejected at the binding boundary like any other target
        // rejection (Class B, observation never control-flow).
        return TransferResult::TargetRejected;
    }
    return TransferResult::TargetRejected;
}

TransferResult BindingExpression::TransferSourceToTarget()
{
    // S245: a DETACHED binding writes NOTHING to the target (or source). Emit
    // Detached (Class B, silenced non-error) ONCE and skip WITHOUT writing —
    // never dereference a target that may already be destroyed. This guard runs
    // BEFORE the transferring_ guard so a detached binding is fully inert.
    if (detached_)
    {
        emit_detached();
        return TransferResult::TargetRejected;
    }

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

    // S811: the forward source->target push is the BINDING's OWN write — it must
    // store the resolved value in the target's local store, NOT be intercepted
    // by the expression's SetValue (which is for EXTERNAL edits → write-back).
    // Temporarily detach the expression registration for the duration of the
    // transfer and restore it on scope exit (RAII covers every return path).
    // (WPF: the binding's UpdateTarget sets the expression's value directly; it
    // does not route through the object's SetValueCommon.)
    struct WritebackRestoreGuard
    {
        BindingExpression& self;
        const bool armed;
        explicit WritebackRestoreGuard(BindingExpression& e)
            : self(e), armed(e.live_writeback())
        {
            if (armed) { self.unsubscribe_writeback(); }
        }
        ~WritebackRestoreGuard()
        {
            if (armed) { self.arm_writeback(); }
        }
    } wbRestore(*this);

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
