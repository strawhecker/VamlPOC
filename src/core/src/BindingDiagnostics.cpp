// S240_1 — BindingDiagnostics record + sink (dep:S240). Sprint 6.
// Implements the Class A/B tagging authority (binding_class_of), the two record
// factories (diagnostic_from_property_read / diagnostic_from_path_error) and
// the sink accessors (default no-op + atomic current sink). See
// BindingDiagnostics.h for the full contract.

#include "vaml/core/BindingDiagnostics.h"

namespace vaml {

namespace {

// The shared NO-OP default sink. Function-local static: one instance for the
// whole process (ADR-018 — no locks, immutable after init).
IBindingDiagnostics& noop_sink() noexcept
{
    static NullBindingDiagnostics sink;
    return sink;
}

// The current sink pointer. Written EXACTLY ONCE by set_binding_diagnostics()
// (startup, before bindings), read locklessly by binding_diagnostics()
// (ADR-018: no new locks). Acquire/release so the emitting thread sees the
// installed sink's vtable reliably. Never null — defaults to noop_sink().
std::atomic<IBindingDiagnostics*> current_sink{&noop_sink()};

} // namespace

BindingClass binding_class_of(BindingReasonCode code) noexcept
{
    switch (code)
    {
        // Class A — StaticAuthoring.
        case BindingReasonCode::EmptyPath:
        case BindingReasonCode::MalformedPath:
        case BindingReasonCode::ConverterSignatureMismatch:
            return BindingClass::StaticAuthoring;

        // Class B — DefinedRuntimeOutcome.
        case BindingReasonCode::NullIntermediate:
        case BindingReasonCode::MissingProperty:
        case BindingReasonCode::NotReadable:
        case BindingReasonCode::ConverterFailed:
        case BindingReasonCode::FallbackApplied:
        case BindingReasonCode::SetTargetFailed:
        case BindingReasonCode::WriteBackRejected:
        case BindingReasonCode::Detached:
            return BindingClass::DefinedRuntimeOutcome;
    }
    // Unreachable for any enumerator; keep the compiler's -Wswitch happy without
    // a default-label fallthrough warning.
    return BindingClass::DefinedRuntimeOutcome;
}

void NullBindingDiagnostics::emit(const BindingDiagnostic&)
{
    // NO-OP. The app opted into observation by installing a real sink; until
    // then, emit is a no-op with zero release cost.
}

IBindingDiagnostics& default_binding_diagnostics() noexcept
{
    return noop_sink();
}

IBindingDiagnostics& binding_diagnostics() noexcept
{
    return *current_sink.load(std::memory_order_acquire);
}

IBindingDiagnostics& set_binding_diagnostics(IBindingDiagnostics& sink) noexcept
{
    IBindingDiagnostics* prev = current_sink.exchange(&sink, std::memory_order_acq_rel);
    return *prev;
}

BindingDiagnostic diagnostic_from_property_read(
    std::uint64_t bindingId, std::string targetProp, BindingDirection direction,
    std::string sourceType, std::string targetType, std::string converter,
    const PropertyPath& path, const PropertyRead& read,
    std::string detail, std::string sourceFile, std::uint32_t sourceLine)
{
    BindingReasonCode reason = BindingReasonCode::NotReadable; // default fallback
    if (!read.ok)
    {
        switch (read.error)
        {
        case PropertyPathError::Empty:
            reason = BindingReasonCode::EmptyPath; // degenerate invalid/empty path
            break;
        case PropertyPathError::NullIntermediate:
            reason = BindingReasonCode::NullIntermediate;
            break;
        case PropertyPathError::MissingProperty:
            reason = BindingReasonCode::MissingProperty;
            break;
        default:
            // NotReadable (and any reader-specific Class B failure).
            reason = BindingReasonCode::NotReadable;
            break;
        }
    }
    else
    {
        // A successful read is not a failure diagnostic this factory is asked
        // to build; callers use it for failures. Keep a defined (harmless) value.
        reason = BindingReasonCode::EmptyPath;
    }

    BindingDiagnostic d;
    d.bindingId    = bindingId;
    d.sourcePath   = path.original();
    d.targetProp   = std::move(targetProp);
    d.direction    = direction;
    d.reasonCode   = reason;
    d.sourceType   = std::move(sourceType);
    d.targetType   = std::move(targetType);
    d.converter    = std::move(converter);
    d.detail       = std::move(detail);
    d.timestamp    = std::chrono::system_clock::now();
    d.sourceFile   = std::move(sourceFile);
    d.sourceLine   = sourceLine;
    return d;
}

BindingDiagnostic diagnostic_from_path_error(
    std::uint64_t bindingId, std::string sourcePath, std::string targetProp,
    BindingDirection direction, std::string sourceType, std::string targetType,
    PropertyPathError error, std::string detail,
    std::string sourceFile, std::uint32_t sourceLine)
{
    BindingReasonCode reason =
        (error == PropertyPathError::Empty) ? BindingReasonCode::EmptyPath
                                            : BindingReasonCode::MalformedPath;

    BindingDiagnostic d;
    d.bindingId    = bindingId;
    d.sourcePath   = std::move(sourcePath);
    d.targetProp   = std::move(targetProp);
    d.direction    = direction;
    d.reasonCode   = reason;
    d.sourceType   = std::move(sourceType);
    d.targetType   = std::move(targetType);
    d.converter    = {}; // a path-error diagnostic has no converter involved
    d.detail       = std::move(detail);
    d.timestamp    = std::chrono::system_clock::now();
    d.sourceFile   = std::move(sourceFile);
    d.sourceLine   = sourceLine;
    return d;
}

} // namespace vaml
