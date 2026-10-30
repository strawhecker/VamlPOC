// S362 (dep:S361) — Style out-of-line definitions. See Style.h.

#include "vaml/core/Style.h"

#include <utility>

namespace vaml {

// ------------------------------------------------------------------ SetterValue
VAML_CORE_EXPORT
SetterValue SetterValue::Literal(Value v) noexcept
{
    SetterValue sv;
    sv.kind_ = Kind::Literal;
    sv.literal_ = std::move(v);
    return sv;
}

VAML_CORE_EXPORT
SetterValue SetterValue::StaticResource(std::string_view key)
{
    SetterValue sv;
    sv.kind_ = Kind::StaticResource;
    sv.ref_ = std::string(key);
    return sv;
}

VAML_CORE_EXPORT
SetterValue SetterValue::DynamicResource(std::string_view key)
{
    SetterValue sv;
    sv.kind_ = Kind::DynamicResource;
    sv.ref_ = std::string(key);
    return sv;
}

VAML_CORE_EXPORT
SetterValue SetterValue::TemplateBinding(std::string_view prop)
{
    SetterValue sv;
    sv.kind_ = Kind::TemplateBinding;
    sv.ref_ = std::string(prop);
    return sv;
}

VAML_CORE_EXPORT
const Value& SetterValue::literal() const noexcept
{
    return literal_;
}

VAML_CORE_EXPORT
std::string_view SetterValue::reference() const noexcept
{
    return ref_;
}

// ------------------------------------------------------------------ Trigger
VAML_CORE_EXPORT
Trigger Trigger::MakeProperty(PropertyTrigger t)
{
    Trigger tr;
    tr.kind = TriggerKind::Property;
    tr.property = std::move(t);
    return tr;
}

VAML_CORE_EXPORT
Trigger Trigger::MakeData(DataTrigger t)
{
    Trigger tr;
    tr.kind = TriggerKind::Data;
    tr.data = std::move(t);
    return tr;
}

VAML_CORE_EXPORT
Trigger Trigger::MakeEvent(EventTrigger t)
{
    Trigger tr;
    tr.kind = TriggerKind::Event;
    tr.event = std::move(t);
    return tr;
}

// ------------------------------------------------------------------ Style
VAML_CORE_EXPORT
Style::Style() = default;

VAML_CORE_EXPORT
Style::~Style() = default;

VAML_CORE_EXPORT
void Style::set_target_type(std::string type)
{
    target_type_ = std::move(type);
}

VAML_CORE_EXPORT
void Style::add_setter(Setter setter)
{
    setters_.push_back(std::move(setter));
}

VAML_CORE_EXPORT
void Style::add_trigger(Trigger t)
{
    triggers_.push_back(std::move(t));
}

// ------------------------------------------------------------------ seams
VAML_CORE_EXPORT
bool style_valid(const Style& style) noexcept
{
    return !style.target_type().empty();
}

VAML_CORE_EXPORT
SetterValue::Kind setter_value_kind(const SetterValue& v) noexcept
{
    return v.kind();
}

VAML_CORE_EXPORT
TriggerKind trigger_kind(const Trigger& t) noexcept
{
    return t.kind;
}

} // namespace vaml
