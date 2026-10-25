// S360_1 (dep:S360) — IconKey resolution definitions. See IconKey.h.

#include "vaml/core/IconKey.h"

namespace vaml {

VAML_CORE_EXPORT
IconAssetRef resolve_icon(const UIElement& element, IconKey key) noexcept
{
    const ResourceValue* rv = find_for_element(element, key.value);
    if (rv == nullptr)
    {
        return IconAssetRef{ false, key.value, nullptr, ResourceLookupError::MissingKey };
    }
    // An icon-asset resource is an Object entry (the S202_1/S400_1 raster is
    // the owned DependencyObject). A Plain entry is NOT an icon asset — report
    // MissingKey (the key resolved to something that is not an asset), the
    // honest structured outcome for an invalid icon key.
    if (rv->kind() != ResourceValue::Kind::Object)
    {
        return IconAssetRef{ false, key.value, nullptr, ResourceLookupError::MissingKey };
    }
    return IconAssetRef{ true, key.value, rv->object(), ResourceLookupError::None };
}

} // namespace vaml
