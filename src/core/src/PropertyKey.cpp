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

    if (flags_ != PropertyFlags::None)
    {
        os << " [";
        bool first = true;
        if (has_flag(flags_, PropertyFlags::Inherits))
        {
            os << "Inherits"; first = false;
        }
        if (has_flag(flags_, PropertyFlags::ReadOnly))
        {
            os << (!first ? "|" : "") << "ReadOnly"; first = false;
        }
        if (has_flag(flags_, PropertyFlags::BindsTwoWayByDefault))
        {
            os << (!first ? "|" : "") << "BindsTwoWayByDefault";
        }
        if (has_flag(flags_, PropertyFlags::InheritOnly))
        {
            os << (!first ? "|" : "") << "InheritOnly";
        }
        os << "]";
    }

    os << ')';
    return os.str();
}

} // namespace vaml
