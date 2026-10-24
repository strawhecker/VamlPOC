// S242 — BindingExpression (dep:S241). Sprint 6.
// Implements the S242 headline: the binding expression's mode, the converter
// wire, the Class B fallback, and the first source->target transfer. The
// transfer runs the S240 evaluator over the S241 ObservableObject reader,
// applies the converter, applies the fallback on a Class B outcome or converter
// failure, then pushes to the target DP. Live subscription (S243 one-way, S244
// two-way) is a forward step (NO-GAP, recorded); the Strict policy arm (S242_1)
// is REAL here. See BindingExpression.h for the full contract.

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

BindingExpression::BindingExpression(BindingExpression&&) noexcept = default;
BindingExpression& BindingExpression::operator=(BindingExpression&&) noexcept = default;
BindingExpression::~BindingExpression() = default;

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
    return e;
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
