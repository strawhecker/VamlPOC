#pragma once
// VAML FrameworkTemplate — the shared base of the WPF template family
// (WPF System.Windows.FrameworkTemplate). 11n-3.
//
// WPF-faithful: FrameworkTemplate is the abstract base shared by
// ControlTemplate / DataTemplate / ItemsPanelTemplate (verified from
// E:\GitRepos\wpf\...\System\Windows\FrameworkTemplate.cs). It is a
// DependencyObject (a valid owned resource / x:Key target in the VAML
// resource model, matching the existing S364 ControlTemplate + S500
// DataTemplate mapping) and carries the template TARGET-TYPE name that unifies
// how each template is selected/applied.
//
// The VAML templates that derive from FrameworkTemplate:
//   - ControlTemplate  (S364/S365) — a control's INTERNAL tree + visual states;
//   - DataTemplate     (S500) — a per-item authored content root + load_content;
//   - ItemsPanelTemplate (11n-3, NEW) — the ItemsControl items-host panel recipe
//     (e.g. VirtualizingStackPanel), which ItemsControl.ItemsPanel resolves.
//
// ADR-018: ZERO new locks (a template is a DependencyObject resource, read/
// applied on the affinity thread).

#include "vaml/core/DependencyObject.h"  // FrameworkTemplate IS a DependencyObject (resource model)
#include "vaml/core/PropertyId.h"
#include "vaml/core/export.h"

#include <cstddef>
#include <string>

namespace vaml {

// The common template base (WPF System.Windows.FrameworkTemplate).
class VAML_CORE_EXPORT FrameworkTemplate : public DependencyObject
{
public:
    // The template TARGET-TYPE name (e.g. "Button" for a ControlTemplate, the
    // item type for a DataTemplate / ItemsPanelTemplate). WPF-faithful.
    void set_target_type(std::string type);
    [[nodiscard]] const std::string& target_type() const noexcept { return target_type_; }

protected:
    FrameworkTemplate() = default;
    ~FrameworkTemplate() override = default;

    FrameworkTemplate(const FrameworkTemplate&) = delete;
    FrameworkTemplate& operator=(const FrameworkTemplate&) = delete;

private:
    std::string target_type_;
};

} // namespace vaml
