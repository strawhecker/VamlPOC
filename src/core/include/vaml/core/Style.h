#pragma once
// VAML Style + Setter + BasedOn + PropertyTrigger + DataTrigger + EventTrigger.
// S362 (dep:S361) — Sprint 9, the WPF-faithful style model.
//
// A `Style` is a template of property-setters keyed to a TARGET TYPE NAME
// (e.g. "Button"). It is authored as a resource (an owned Object entry in a
// ResourceDictionary) and APPLIED at runtime by S363's StyleApplier. This step
// (S362) is the DATA MODEL + seams; S363 owns runtime application.
//
// SETTER:
//   A single (PropertyId targetProperty, SetterValue value). The target
//   property is resolved via S043's attached-aware DependencyProperty lookup
//   at apply time. SetterValue is a small tagged value holding EITHER:
//     - Literal          — a plain literal-converted Value (e.g. an Int32, a
//                           Color, a Thickness),
//     - StaticResource   — a {StaticResource <key>} reference,
//     - DynamicResource  — a {DynamicResource <key>} live reference (S361),
//     - TemplateBinding  — a {TemplateBinding <prop>} read from the templated
//                           control's DP (S365). Carried here so the apply time
//                           (S363/S365) resolves the right source.
//
// STYLE:
//   An ordered collection of setters (insertion order = application order) for
//   a single `target_type` name. `BasedOn` is a NON-OWNING pointer to a parent
//   Style* (the parent is built as a resource and outlives the child). The
//   BaseOn chain lets a derived style inherit + override a base style.
//
// TRIGGERS:
//   A style may carry PropertyTrigger, DataTrigger, and EventTrigger records:
//     - PropertyTrigger { PropertyId property; Value value; setters } — fires
//       when the styled element's DP `property` EQUALS `value`.
//     - DataTrigger { PropertyPath path; Value value; setters } — S240's real
//       PropertyPath; fires when the resolved data value (through the binding
//       reader, S243-style) equals `value`. This is the DATA-BOUND trigger.
//     - EventTrigger { eventName; sourceName } — 11k-5: the event-driven
//       trigger AUTHORING CONTAINER. WPF-faithful: it is ACTIONS-based
//       (`[ContentProperty("Actions")]` of TriggerAction — canonical
//       `<EventTrigger RoutedEvent><BeginStoryboard><Storyboard>…`), NOT
//       Setter-based like Property/Data triggers. At 11k-5 only the container
//       (RoutedEvent required + optional SourceName) is authored into the S362
//       model. The concrete `TriggerAction` payload (BeginStoryboard →
//       Storyboard/DoubleAnimation) + the `TriggerAction::Invoke` evaluation
//       (begin the Storyboard when the RoutedEvent fires) are the recorded
//       NO-GAP continuations owned by the 11e/11f/11g forward tokens (roadmap),
//       resolved at S510 — never silent.
//   S363's applier both evaluates these at apply time and (for PropertyTrigger)
//   subscribes the element's DP so a change can re-fire/un-fire. EventTrigger is
//   an authoring record at apply time (the S363 trigger switch's `default:
//   fires=false` holds it dormant until the 11g/S510 Invoke evaluation lands).
//
// SEAMS (headless, exported):
//   - `style_valid(s)`: a Style is authoring-valid iff it names a non-empty
//     target type. Used as the gate before apply/register.
//   - `setter_value_kind(v)`: the SetterValue kind (Literal/Static/Dynamic/
//     TemplateBinding) — the setter-resolution discrimination seam.
//   - `trigger_kind(t)`: Property vs Data vs Event — the trigger discrimination
//     seam.
//   - SetterValue kind predicates: is_literal / is_static_resource /
//     is_dynamic_resource / is_template_binding.
//
// ADR-018: ZERO new locks (pure value-domain data model).

#include "vaml/core/DependencyObject.h" // Style IS a DependencyObject (resource value model)
#include "vaml/core/PropertyId.h"     // PropertyId (target property)
#include "vaml/core/PropertyPath.h"   // PropertyPath (DataTrigger route, S240)
#include "vaml/core/Value.h"          // Value (literal values + trigger match values)

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// SetterValue — the tagged payload of one Setter.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT SetterValue
{
public:
    enum class Kind : std::uint8_t
    {
        Literal          = 0, // a plain literal-converted Value
        StaticResource   = 1, // {StaticResource <key>}
        DynamicResource  = 2, // {DynamicResource <key>} (S361 live link)
        TemplateBinding  = 3, // {TemplateBinding <prop>} (S365 control-DP read)
    };

    SetterValue() = default;

    // ---- Factories ---------------------------------------------------
    [[nodiscard]] static SetterValue Literal(Value v) noexcept;
    [[nodiscard]] static SetterValue StaticResource(std::string_view key);
    [[nodiscard]] static SetterValue DynamicResource(std::string_view key);
    [[nodiscard]] static SetterValue TemplateBinding(std::string_view prop);

    // ---- Introspection ----------------------------------------------
    [[nodiscard]] Kind kind() const noexcept { return kind_; }
    [[nodiscard]] bool is_literal() const noexcept { return kind_ == Kind::Literal; }
    [[nodiscard]] bool is_static_resource() const noexcept { return kind_ == Kind::StaticResource; }
    [[nodiscard]] bool is_dynamic_resource() const noexcept { return kind_ == Kind::DynamicResource; }
    [[nodiscard]] bool is_template_binding() const noexcept { return kind_ == Kind::TemplateBinding; }

    // Literal payload (valid only when is_literal()).
    [[nodiscard]] const Value& literal() const noexcept;

    // The reference token: the resource KEY (Static/Dynamic) or the template
    // PROPERTY NAME (TemplateBinding). Valid only for the three reference kinds.
    [[nodiscard]] std::string_view reference() const noexcept;

private:
    Kind        kind_ = Kind::Literal;
    Value       literal_ = Value::Null();
    std::string ref_; // key / prop payload for the reference kinds
};

// ---------------------------------------------------------------------------
// Setter — ONE property write a style performs.
// ---------------------------------------------------------------------------
struct VAML_CORE_EXPORT Setter
{
    PropertyId  targetProperty; // resolved via S043 attached-aware lookup at apply
    SetterValue value;
};

// ---------------------------------------------------------------------------
// PropertyTrigger — fires when the styled element's DP equals `value`.
// ---------------------------------------------------------------------------
struct VAML_CORE_EXPORT PropertyTrigger
{
    PropertyId          property;            // the DP to watch on the styled element
    Value               value = Value::Null(); // the equality match value
    std::vector<Setter> setters;             // applied when the trigger fires
};

// ---------------------------------------------------------------------------
// DataTrigger — S240's real PropertyPath; fires on a data-value match.
// ---------------------------------------------------------------------------
struct VAML_CORE_EXPORT DataTrigger
{
    PropertyPath        path;     // the S240 parsed route into the data context
    Value               value = Value::Null(); // the equality match value
    std::vector<Setter> setters;  // applied when the trigger fires
};

// ---------------------------------------------------------------------------
// EventTrigger — 11k-5 the event-driven AUTHORING CONTAINER.
// WPF-FAITHFUL: Actions-based, NOT Setter-based. Only the container is modelled
// here (RoutedEvent required + optional SourceName); the `TriggerAction` payload
// (BeginStoryboard → Storyboard/DoubleAnimation) + its `TriggerAction::Invoke`
// evaluation are the recorded NO-GAP continuations owned by the 11e/11f/11g
// forward tokens, resolved at S510 (roadmap + DAG_controls trigger family).
// Names are interned (S140) for stable identity.
// ---------------------------------------------------------------------------
struct VAML_CORE_EXPORT EventTrigger
{
    std::string_view eventName;   // the routed event name (e.g. "Button.Click")
    std::string_view sourceName;  // optional: which named source to listen on
};

// ---------------------------------------------------------------------------
// Trigger — a discriminated PropertyTrigger | DataTrigger | EventTrigger. The
// S362 discrimination seam is `trigger_kind(t)`.
// ---------------------------------------------------------------------------
enum class TriggerKind : std::uint8_t
{
    None   = 0, // unset / default
    Property = 1, // a PropertyTrigger
    Data     = 2, // a DataTrigger
    Event    = 3, // an EventTrigger (11k-5)
};

struct VAML_CORE_EXPORT Trigger
{
    TriggerKind kind = TriggerKind::None;
    PropertyTrigger property; // valid when kind == Property
    DataTrigger     data;     // valid when kind == Data
    EventTrigger    event;    // valid when kind == Event

    [[nodiscard]] static Trigger MakeProperty(PropertyTrigger t);
    [[nodiscard]] static Trigger MakeData(DataTrigger t);
    [[nodiscard]] static Trigger MakeEvent(EventTrigger t);
};

// ---------------------------------------------------------------------------
// Style — the ordered setter collection + BasedOn chain + triggers.
// ---------------------------------------------------------------------------
class VAML_CORE_EXPORT Style : public DependencyObject
{
public:
    Style();
    // The destructor must be out-of-line so the resource-value model (owned
    // unique_ptr<DependencyObject>) works with polymorphic deletion.
    ~Style() override;

    // ---- Target type -------------------------------------------------
    void set_target_type(std::string type);
    [[nodiscard]] const std::string& target_type() const noexcept { return target_type_; }

    // ---- Setters (insertion order) -----------------------------------
    void add_setter(Setter setter);
    [[nodiscard]] const std::vector<Setter>& setters() const noexcept { return setters_; }

    // ---- BasedOn (NON-OWNING parent Style*; must outlive the child) ---
    void set_based_on(Style* base) noexcept { based_on_ = base; }
    [[nodiscard]] Style* based_on() const noexcept { return based_on_; }

    // ---- Triggers ----------------------------------------------------
    void add_trigger(Trigger t);
    [[nodiscard]] const std::vector<Trigger>& triggers() const noexcept { return triggers_; }

private:
    std::string           target_type_;
    std::vector<Setter>   setters_;
    Style*                based_on_ = nullptr; // non-owning
    std::vector<Trigger>  triggers_;
};

// ---------------------------------------------------------------------------
// Seams (headless, exported).
// ---------------------------------------------------------------------------

// A Style is authoring-valid iff it names a non-empty target type. S363 gates
// apply/register on this.
[[nodiscard]] VAML_CORE_EXPORT bool style_valid(const Style& style) noexcept;

// The SetterValue kind — the setter-resolution discrimination seam.
[[nodiscard]] VAML_CORE_EXPORT SetterValue::Kind setter_value_kind(const SetterValue& v) noexcept;

// The Trigger kind — Property vs Data vs Event — the trigger discrimination seam.
[[nodiscard]] VAML_CORE_EXPORT TriggerKind trigger_kind(const Trigger& t) noexcept;

} // namespace vaml
