// S550 (Sprint 11m) — the derived-base + scaffolding surface.
//
// item-container / header / presenter / panel + collection-value scaffolding
// that the closed controls' selector behavior and the DAG's closure-under-
// ancestry invariant require to be loadable. Each is a REAL re-based UIElement
// (or DependencyObject resource type) registered as a built-in, classified.
// ZERO MINIMAL REAL.
//
// WPF-faithful parent branches per the WPF source review (see Scaffolding.h).
// The FULL WPF container machinery (ItemContainerGenerator recycling, the
// VirtualizingPanel item-host logic, the GridViewColumn collection converter)
// is consumed by the DONE control steps (S552/S494/S473) + S556's mechanism
// layer — recorded NO-GAP at that step.
//
// ADR-011 held (all surface = core data; render stays in the vulkan
// classifier). ADR-018: zero new locks.

#include "vaml/core/Scaffolding.h"

#include "vaml/core/DataTemplate.h" // DataTemplate for the ItemContainerTemplate base
#include "vaml/core/DependencyProperty.h" // DependencyProperty::Register (the DP closure)
#include "vaml/core/Element.h"      // PropertyId helpers
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// Panel — the abstract layout-panel base.
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
Panel::Panel() = default;
VAML_CORE_EXPORT
Panel::~Panel() = default;

VAML_CORE_EXPORT
Size Panel::MeasureOverride(const Size& available) const
{
    // The base panel: measure each child at the offered box, report the max
    // child desired size (the common panel fallback). Concrete panels override.
    float maxW = 0.0f, maxH = 0.0f;
    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* c = child_at(i);
        c->Measure(available);
        if (c->desired_size().width > maxW)  maxW = c->desired_size().width;
        if (c->desired_size().height > maxH) maxH = c->desired_size().height;
    }
    return Size{ maxW, maxH };
}

VAML_CORE_EXPORT
void Panel::ArrangeOverride(const vaml::Rect& finalSlot)
{
    // The base panel: arrange each child to FILL the panel's box.
    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        child_at(i)->Arrange(finalSlot);
    }
}

// ---------------------------------------------------------------------------
// Decorator — the abstract single-child adornment base.
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
Decorator::Decorator() = default;
VAML_CORE_EXPORT
Decorator::~Decorator() = default;

VAML_CORE_EXPORT
void Decorator::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child)
    {
        InvalidateMeasure();
        InvalidateArrange();
        return;
    }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}

VAML_CORE_EXPORT
UIElement* Decorator::content() const noexcept { return content_; }

VAML_CORE_EXPORT
Size Decorator::MeasureOverride(const Size& available) const
{
    if (content_)
    {
        content_->Measure(available);
        return content_->desired_size();
    }
    return Size{ 0.0f, 0.0f };
}

VAML_CORE_EXPORT
void Decorator::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_)
    {
        content_->Arrange(finalSlot);
    }
}

// ---------------------------------------------------------------------------
// ComboBoxItem
// ---------------------------------------------------------------------------
namespace {
void ensure_comboboxitem_properties_registered()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "IsSelected", ValueType::Int32, "ComboBoxItem",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "IsHighlighted", ValueType::Int32, "ComboBoxItem",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
ComboBoxItem::ComboBoxItem() { ensure_comboboxitem_properties_registered(); }

VAML_CORE_EXPORT
SetResult ComboBoxItem::set_is_selected(bool value)
{
    return Set(PropertyId::of("IsSelected","ComboBoxItem"), Value::Int32(value ? 1 : 0));
}
VAML_CORE_EXPORT
bool ComboBoxItem::is_selected() const
{
    return Get(PropertyId::of("IsSelected","ComboBoxItem")).get<int>() != 0;
}
VAML_CORE_EXPORT
SetResult ComboBoxItem::set_is_highlighted(bool value)
{
    return Set(PropertyId::of("IsHighlighted","ComboBoxItem"), Value::Int32(value ? 1 : 0));
}
VAML_CORE_EXPORT
bool ComboBoxItem::is_highlighted() const
{
    return Get(PropertyId::of("IsHighlighted","ComboBoxItem")).get<int>() != 0;
}
VAML_CORE_EXPORT
void ComboBoxItem::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* ComboBoxItem::content() const noexcept { return content_; }
VAML_CORE_EXPORT
Size ComboBoxItem::MeasureOverride(const Size& available) const
{
    if (content_) { content_->Measure(available); return content_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void ComboBoxItem::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_) content_->Arrange(finalSlot);
}

// ---------------------------------------------------------------------------
// ListBoxItem
// ---------------------------------------------------------------------------
namespace {
void ensure_listboxitem_properties_registered()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "IsSelected", ValueType::Int32, "ListBoxItem",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
    (void)DependencyProperty::Register(
        "IsHighlighted", ValueType::Int32, "ListBoxItem",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
ListBoxItem::ListBoxItem() { ensure_listboxitem_properties_registered(); }

VAML_CORE_EXPORT
SetResult ListBoxItem::set_is_selected(bool value)
{
    return Set(PropertyId::of("IsSelected","ListBoxItem"), Value::Int32(value ? 1 : 0));
}
VAML_CORE_EXPORT
bool ListBoxItem::is_selected() const
{
    return Get(PropertyId::of("IsSelected","ListBoxItem")).get<int>() != 0;
}
VAML_CORE_EXPORT
SetResult ListBoxItem::set_is_highlighted(bool value)
{
    return Set(PropertyId::of("IsHighlighted","ListBoxItem"), Value::Int32(value ? 1 : 0));
}
VAML_CORE_EXPORT
bool ListBoxItem::is_highlighted() const
{
    return Get(PropertyId::of("IsHighlighted","ListBoxItem")).get<int>() != 0;
}
VAML_CORE_EXPORT
void ListBoxItem::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* ListBoxItem::content() const noexcept { return content_; }
VAML_CORE_EXPORT
Size ListBoxItem::MeasureOverride(const Size& available) const
{
    if (content_) { content_->Measure(available); return content_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void ListBoxItem::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_) content_->Arrange(finalSlot);
}

// ---------------------------------------------------------------------------
// ListViewItem
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
ListViewItem::ListViewItem() = default;

// ---------------------------------------------------------------------------
// StatusBarItem
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
StatusBarItem::StatusBarItem() = default;
VAML_CORE_EXPORT
void StatusBarItem::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* StatusBarItem::content() const noexcept { return content_; }
VAML_CORE_EXPORT
Size StatusBarItem::MeasureOverride(const Size& available) const
{
    if (content_) { content_->Measure(available); return content_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void StatusBarItem::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_) content_->Arrange(finalSlot);
}

// ---------------------------------------------------------------------------
// GroupItem
// ---------------------------------------------------------------------------
namespace {
void ensure_groupitem_properties_registered()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "IsExpanded", ValueType::Int32, "GroupItem",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
GroupItem::GroupItem() { ensure_groupitem_properties_registered(); }

VAML_CORE_EXPORT
SetResult GroupItem::set_is_expanded(bool value)
{
    return Set(PropertyId::of("IsExpanded","GroupItem"), Value::Int32(value ? 1 : 0));
}
VAML_CORE_EXPORT
bool GroupItem::is_expanded() const
{
    return Get(PropertyId::of("IsExpanded","GroupItem")).get<int>() != 0;
}
VAML_CORE_EXPORT
void GroupItem::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* GroupItem::content() const noexcept { return content_; }
VAML_CORE_EXPORT
Size GroupItem::MeasureOverride(const Size& available) const
{
    if (content_) { content_->Measure(available); return content_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void GroupItem::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_) content_->Arrange(finalSlot);
}

// ---------------------------------------------------------------------------
// DataGridRow / DataGridCell
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
DataGridRow::DataGridRow() = default;
VAML_CORE_EXPORT
void DataGridRow::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* DataGridRow::content() const noexcept { return content_; }
VAML_CORE_EXPORT
Size DataGridRow::MeasureOverride(const Size& available) const
{
    if (content_) { content_->Measure(available); return content_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void DataGridRow::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_) content_->Arrange(finalSlot);
}

VAML_CORE_EXPORT
DataGridCell::DataGridCell() = default;
VAML_CORE_EXPORT
void DataGridCell::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* DataGridCell::content() const noexcept { return content_; }
VAML_CORE_EXPORT
Size DataGridCell::MeasureOverride(const Size& available) const
{
    if (content_) { content_->Measure(available); return content_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void DataGridCell::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_) content_->Arrange(finalSlot);
}

// ---------------------------------------------------------------------------
// ItemsPresenter
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
ItemsPresenter::ItemsPresenter() = default;
VAML_CORE_EXPORT
void ItemsPresenter::set_items_host(std::unique_ptr<UIElement> host)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    host_ = nullptr;
    if (!host) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = host.get();
    add_child(std::move(host));
    host_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* ItemsPresenter::items_host() const noexcept { return host_; }
VAML_CORE_EXPORT
Size ItemsPresenter::MeasureOverride(const Size& available) const
{
    if (host_) { host_->Measure(available); return host_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void ItemsPresenter::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (host_) host_->Arrange(finalSlot);
}

// ---------------------------------------------------------------------------
// ItemsPanelTemplate
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
ItemsPanelTemplate::ItemsPanelTemplate() = default;
VAML_CORE_EXPORT
ItemsPanelTemplate::~ItemsPanelTemplate() = default;

VAML_CORE_EXPORT
void ItemsPanelTemplate::set_panel_root(std::unique_ptr<UIElement> root)
{
    panel_root_ = std::move(root);
}

VAML_CORE_EXPORT
std::unique_ptr<UIElement> ItemsPanelTemplate::load_items_panel() const
{
    if (!panel_root_)
    {
        return nullptr;
    }
    // Deep-clone the authored panel spec into a FRESH, independent panel
    // (the template is a spec, never a live shared node). The S500 generic
    // clone_element reconstructs the authored UIElement subtree by concrete
    // type + copies the authored DPs + recursively clones children.
    return vaml::clone_element(*panel_root_);
}

// ---------------------------------------------------------------------------
// ItemContainerTemplate
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
ItemContainerTemplate::ItemContainerTemplate() = default;

// ---------------------------------------------------------------------------
// GridView signature types (value types — implement inline where small)
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
void GridViewColumnCollection::add(GridViewColumn col)
{
    cols_.push_back(std::move(col));
}

VAML_CORE_EXPORT
GridViewRowPresenterBase::GridViewRowPresenterBase() = default;
VAML_CORE_EXPORT
GridViewRowPresenterBase::~GridViewRowPresenterBase() = default;
VAML_CORE_EXPORT
void GridViewRowPresenterBase::set_columns(GridViewColumnCollection cols)
{
    cols_ = std::move(cols);
}
VAML_CORE_EXPORT
const GridViewColumnCollection& GridViewRowPresenterBase::columns() const noexcept
{
    return cols_;
}

VAML_CORE_EXPORT
GridViewRowPresenter::GridViewRowPresenter() = default;
VAML_CORE_EXPORT
Size GridViewRowPresenter::MeasureOverride(const Size& available) const
{
    // The data-row presenter measures as wide as the sum of its columns (within
    // the offered width). No per-cell children here (cells are authored under
    // the row); the column widths drive the row extent.
    float w = 0.0f;
    for (std::size_t i = 0; i < cols_.size(); ++i)
    {
        w += cols_.at(i).width();
    }
    if (available.width < w) w = available.width;
    return Size{ w, available.height };
}

VAML_CORE_EXPORT
GridViewHeaderRowPresenter::GridViewHeaderRowPresenter() = default;
VAML_CORE_EXPORT
Size GridViewHeaderRowPresenter::MeasureOverride(const Size& available) const
{
    float w = 0.0f;
    for (std::size_t i = 0; i < cols_.size(); ++i)
    {
        w += cols_.at(i).width();
    }
    if (available.width < w) w = available.width;
    return Size{ w, available.height };
}

namespace {
void ensure_gridviewcolumnheader_properties_registered()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "ColumnHeaderRole", ValueType::Int32, "GridViewColumnHeader",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
GridViewColumnHeader::GridViewColumnHeader() { ensure_gridviewcolumnheader_properties_registered(); }
VAML_CORE_EXPORT
SetResult GridViewColumnHeader::set_column_header_role(std::int32_t value)
{
    return Set(PropertyId::of("ColumnHeaderRole","GridViewColumnHeader"), Value::Int32(value));
}
VAML_CORE_EXPORT
std::int32_t GridViewColumnHeader::column_header_role() const
{
    return Get(PropertyId::of("ColumnHeaderRole","GridViewColumnHeader")).get<int>();
}
VAML_CORE_EXPORT
void GridViewColumnHeader::set_content(std::unique_ptr<UIElement> child)
{
    while (child_count() > 0)
    {
        [[maybe_unused]] std::size_t remaining = remove_child(0);
    }
    content_ = nullptr;
    if (!child) { InvalidateMeasure(); InvalidateArrange(); return; }
    UIElement* raw = child.get();
    add_child(std::move(child));
    content_ = raw;
    InvalidateMeasure();
    InvalidateArrange();
}
VAML_CORE_EXPORT
UIElement* GridViewColumnHeader::content() const noexcept { return content_; }
VAML_CORE_EXPORT
Size GridViewColumnHeader::MeasureOverride(const Size& available) const
{
    if (content_) { content_->Measure(available); return content_->desired_size(); }
    return Size{ 0.0f, 0.0f };
}
VAML_CORE_EXPORT
void GridViewColumnHeader::ArrangeOverride(const vaml::Rect& finalSlot)
{
    if (content_) content_->Arrange(finalSlot);
}

namespace {
void ensure_gridview_properties_registered()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "ColumnHeaderContainerStyle", ValueType::String, "GridView",
        PropertyMetadata::WithDefault(Value::String(std::string{}), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
GridView::GridView() { ensure_gridview_properties_registered(); }
VAML_CORE_EXPORT
GridView::~GridView() = default;
VAML_CORE_EXPORT
void GridView::set_columns(GridViewColumnCollection cols)
{
    cols_ = std::move(cols);
}
VAML_CORE_EXPORT
const GridViewColumnCollection& GridView::columns() const noexcept { return cols_; }
VAML_CORE_EXPORT
SetResult GridView::set_column_header_container_style(std::string value)
{
    return Set(PropertyId::of("ColumnHeaderContainerStyle","GridView"), Value::String(std::move(value)));
}
VAML_CORE_EXPORT
std::string GridView::column_header_container_style() const
{
    return Get(PropertyId::of("ColumnHeaderContainerStyle","GridView")).get<std::string>();
}

// ---------------------------------------------------------------------------
// VirtualizingPanel / VirtualizingStackPanel / SelectiveScrollingGrid
// ---------------------------------------------------------------------------
VAML_CORE_EXPORT
VirtualizingPanel::VirtualizingPanel() = default;
VAML_CORE_EXPORT
VirtualizingPanel::~VirtualizingPanel() = default;

namespace {
void ensure_virtualizingstackpanel_properties_registered()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "IsVirtualizing", ValueType::Int32, "VirtualizingStackPanel",
        PropertyMetadata::WithDefault(Value::Int32(1), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
VirtualizingStackPanel::VirtualizingStackPanel() { ensure_virtualizingstackpanel_properties_registered(); }
VAML_CORE_EXPORT
SetResult VirtualizingStackPanel::set_is_virtualizing(bool value)
{
    return Set(PropertyId::of("IsVirtualizing","VirtualizingStackPanel"), Value::Int32(value ? 1 : 0));
}
VAML_CORE_EXPORT
bool VirtualizingStackPanel::is_virtualizing() const
{
    return Get(PropertyId::of("IsVirtualizing","VirtualizingStackPanel")).get<int>() != 0;
}
VAML_CORE_EXPORT
Size VirtualizingStackPanel::MeasureOverride(const Size& available) const
{
    // WPF-faithful: a vertical stack (the default VIP). Real measure + arrange
    // so the ItemsHost surfaces the children; the recycler (S556) drives the
    // real virtualization. The stack matches StackPanel's vertical algorithm.
    float maxW = 0.0f, maxH = 0.0f;
    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* c = child_at(i);
        c->Measure(Size{ available.width, Size::Infinite().height });
        if (c->desired_size().width > maxW) maxW = c->desired_size().width;
        maxH += c->desired_size().height;
    }
    return Size{ maxW, maxH };
}
VAML_CORE_EXPORT
void VirtualizingStackPanel::ArrangeOverride(const vaml::Rect& finalSlot)
{
    float y = finalSlot.y;
    const std::size_t n = child_count();
    for (std::size_t i = 0; i < n; ++i)
    {
        UIElement* c = child_at(i);
        const float h = c->desired_size().height;
        c->Arrange(Rect{ finalSlot.x, y, finalSlot.width, h });
        y += h;
    }
}

namespace {
void ensure_selectivescrollinggrid_properties_registered()
{
    static bool registered = false;
    if (registered) return;
    registered = true;
    (void)DependencyProperty::Register(
        "SelectiveScrollingOrientation", ValueType::Int32, "SelectiveScrollingGrid",
        PropertyMetadata::WithDefault(Value::Int32(0), PropertyFlags::None));
}
} // namespace

VAML_CORE_EXPORT
SelectiveScrollingGrid::SelectiveScrollingGrid() { ensure_selectivescrollinggrid_properties_registered(); }
VAML_CORE_EXPORT
SetResult SelectiveScrollingGrid::set_selective_scrolling_orientation(std::int32_t value)
{
    return Set(PropertyId::of("SelectiveScrollingOrientation","SelectiveScrollingGrid"),
               Value::Int32(value));
}
VAML_CORE_EXPORT
std::int32_t SelectiveScrollingGrid::selective_scrolling_orientation() const
{
    return Get(PropertyId::of("SelectiveScrollingOrientation","SelectiveScrollingGrid")).get<int>();
}

} // namespace vaml
