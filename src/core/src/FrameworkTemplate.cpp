// VAML FrameworkTemplate — the shared template base (WPF
// System.Windows.FrameworkTemplate). 11n-3. ADR-018: ZERO new locks (a
// template is a DependencyObject resource, read/applied on the affinity
// thread).

#include "vaml/core/FrameworkTemplate.h"

#include <utility>

namespace vaml {

VAML_CORE_EXPORT
void FrameworkTemplate::set_target_type(std::string type)
{
    target_type_ = std::move(type);
}

} // namespace vaml
