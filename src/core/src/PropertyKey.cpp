// S032 — PropertyKey out-of-line definitions: constructor, equality, and the
// human-readable describe() used in error diagnostics. PropertyKey is
// immutable; all mutation happens at construction.

#include "vaml/core/PropertyKey.h"

#include <sstream>
#include <utility>

namespace vaml {

VAML_CORE_EXPORT
PropertyKey::PropertyKey(std::string name, ValueType type, std::string ownerTypeId, PropertyFlags flags)
    : name_(std::move(name))
    , type_(type)
    , ownerTypeId_(std::move(ownerTypeId))
    , flags_(flags)
{
}

VAML_CORE_EXPORT
bool PropertyKey::operator==(const PropertyKey& other) const noexcept
{
    return name_ == other.name_
        && type_ == other.type_
        && ownerTypeId_ == other.ownerTypeId_
        && flags_ == other.flags_;
}

VAML_CORE_EXPORT
std::string PropertyKey::describe() const
{
    std::ostringstream os;
    os << name_ << " (";

    // Stable type tag name for diagnostics (mirrors Value.cpp's enum order).
    switch (type_)
    {
        case ValueType::Null:       os << "Null";       break;
        case ValueType::Bool:       os << "Bool";       break;
        case ValueType::Int32:      os << "Int32";      break;
        case ValueType::Int64:      os << "Int64";      break;
        case ValueType::Float:      os << "Float";      break;
        case ValueType::Double:     os << "Double";     break;
        case ValueType::String:     os << "String";     break;
        case ValueType::Color:      os << "Color";      break;
        case ValueType::Thickness:  os << "Thickness";  break;
        case ValueType::BrushId:    os << "BrushId";    break;
        case ValueType::UntypedPtr: os << "UntypedPtr"; break;
        default:                    os << "<invalid>";  break;
    }

    os << ", owner '" << ownerTypeId_ << "'";

    // S818: full arm serialization — stable order mirrors the WPF ordinals
    // (Affects* low bits first), then the inheritance/binding arms, then the
    // render gate. Asserted-by-symbol in the S041 describe() test; order here
    // is a stable diagnostic choice (not the raw bit value).
    if (flags_ != PropertyFlags::None)
    {
        os << " [";
        bool first = true;
        auto append = [&os, &first](const char* arm)
        {
            os << (!first ? "|" : "") << arm;
            first = false;
        };

        if (has_flag(flags_, PropertyFlags::AffectsMeasure))            { append("AffectsMeasure"); }
        if (has_flag(flags_, PropertyFlags::AffectsArrange))            { append("AffectsArrange"); }
        if (has_flag(flags_, PropertyFlags::AffectsParentMeasure))      { append("AffectsParentMeasure"); }
        if (has_flag(flags_, PropertyFlags::AffectsParentArrange))      { append("AffectsParentArrange"); }
        if (has_flag(flags_, PropertyFlags::AffectsRender))             { append("AffectsRender"); }
        if (has_flag(flags_, PropertyFlags::Inherits))                  { append("Inherits"); }
        if (has_flag(flags_, PropertyFlags::OverridesInheritanceBehavior)) { append("OverridesInheritanceBehavior"); }
        if (has_flag(flags_, PropertyFlags::NotDataBindable))           { append("NotDataBindable"); }
        if (has_flag(flags_, PropertyFlags::BindsTwoWayByDefault))      { append("BindsTwoWayByDefault"); }
        if (has_flag(flags_, PropertyFlags::ReadOnly))                  { append("ReadOnly"); }
        if (has_flag(flags_, PropertyFlags::Journal))                   { append("Journal"); }
        if (has_flag(flags_, PropertyFlags::SubPropertiesDoNotAffectRender)) { append("SubPropertiesDoNotAffectRender"); }
        os << "]";
    }

    os << ')';
    return os.str();
}

} // namespace vaml
