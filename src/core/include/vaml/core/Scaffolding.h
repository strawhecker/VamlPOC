#pragma once
// VAML derived-base + scaffolding types (S550, Sprint 11m).
//
// The item-container / header / presenter / panel + collection-value scaffolding
// surface that the closed controls' selector behavior and the DAG's
// closure-under-ancestry invariant require to be loadable. Each is a REAL
// re-based UIElement (structural children + Measure/Arrange + the S041
// inheritance-scope edge), registered as a built-in, classified. ZERO MINIMAL
// REAL (every type below is the permanent, forward-resolvable real surface).
//
// WPF-faithful parent branches (per the WPF source review, Controls\*.cs):
//   - Panel          -> UIElement  (the abstract layout-panel base: children
//                                   stack/flow; hosts the ItemsHost role).
//   - Decorator      -> UIElement  (a single-child adornment base: Border/
//                                   Viewbox in WPF derive Decorate; the closed
//                                   controls' content slot).
//   - Shape          -> UIElement  (the abstract geometry leaf base: Fill/
//                                   Stroke/StrokeThickness — S551 consolidates).
//   - ContentPresenter -> UIElement (the per-item host, S500 — extended here).
//   - ItemsPresenter   -> UIElement (SelectiveScrollingGrid/ScrollViewer host).
//   - *Item containers -> UIElement (ComboBoxItem/ListBoxItem/ListViewItem/
//                                   StatusBarItem/TabItem/GroupItem/DataGridRow/
//                                   DataGridCell — the selector per-item unit).
//   - GridView/Header  -> UIElement (the ListView detail view chrome).
//   - ItemContainerTemplate/ItemsPanelTemplate/DataTemplate -> DependencyObject
//                                   (FrameworkTemplate-faithful resource types).
//   - VirtualizingPanel/VirtualizingStackPanel/SelectiveScrollingGrid -> Panel.
//
// The FULL WPF container machinery (ItemContainerGenerator recycling, the
// VirtualizingPanel item-host logic, the GridViewColumn collection converter)
// is consumed by the DONE controls (S552 TreeView, S494 DataGrid, the S473
// ListView) + S556's mechanism layer — recorded NO-GAP at that step. This
// header establishes the LOADABLE scaffolding surface + the real parent
// branches so the closed controls' selector behavior and the DAG's
// closure-under-ancestry invariant hold.
//
// ADR-011 held (all surface = core data; render stays in the vulkan
// classifier). ADR-018: zero new locks. WPF-name fidelity per the 11k-7 audit.

#include "vaml/core/UIElement.h"
#include "vaml/core/DependencyObject.h" // DependencyObject (GridView base)
#include "vaml/core/FrameworkTemplate.h"// 11n-3 — the shared template base (ItemsPanelTemplate base)
#include "vaml/core/DataTemplate.h"     // DataTemplate (ItemContainerTemplate base)
#include "vaml/core/export.h"

#include <memory>
#include <string>

namespace vaml {

// ===========================================================================
// Panel — the WPF-faithful abstract layout-panel base (Panel.cs).
// A Panel hosts ordered children (the generic S121 children list) + the
// ItemsHost role (an ItemsControl's ItemsPresenter surfaces into a Panel).
// The base Panel itself does the minimal common measure/arrange: measure all
// children at the offered box, report the max child desired size; arrange each
// child to fill. Concrete panels (StackPanel/Grid/Canvas/WrapPanel/DockPanel)
// override with their real algorithms.
// ===========================================================================
class VAML_CORE_EXPORT Panel : public UIElement
{
public:
    Panel();
    ~Panel() override;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

// ===========================================================================
// Decorator — the WPF-faithful abstract single-child adornment base
// (Decorator.cs). Border/Viewbox derive it in WPF; it hosts EXACTLY ONE child
// (the single-content slot). The base measures/arranges the single child to
// fill. Concrete decorators (Border handled at S481; the closed controls'
// content slots) override the measure/arrange policy.
// ===========================================================================
class VAML_CORE_EXPORT Decorator : public UIElement
{
public:
    Decorator();
    ~Decorator() override;

    // --- Content child (single-content slot, the Window/Border precedent) --
    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ===========================================================================
// ComboBoxItem — the per-item ComboBox container (ComboBoxItem.cs, a
// ListBoxItem-adjacent item container). Single-content slot (the item's content
// host) + an IsHighlighted/IsSelected DP set. classify -> Solid.
// ===========================================================================
class VAML_CORE_EXPORT ComboBoxItem final : public UIElement
{
public:
    ComboBoxItem();

    [[nodiscard]] SetResult set_is_selected(bool value);
    [[nodiscard]] bool is_selected() const;
    [[nodiscard]] SetResult set_is_highlighted(bool value);
    [[nodiscard]] bool is_highlighted() const;

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ===========================================================================
// ListBoxItem — the per-item ListBox container (ListBoxItem.cs). Carries the
// item's content (single-content slot) + IsSelected/IsHighlighted DPs. The
// list-selection visuals are the S473 ListBox chrome; the per-item container
// is this real unit. classify -> Solid.
// ===========================================================================
class VAML_CORE_EXPORT ListBoxItem : public UIElement
{
public:
    ListBoxItem();

    [[nodiscard]] SetResult set_is_selected(bool value);
    [[nodiscard]] bool is_selected() const;
    [[nodiscard]] SetResult set_is_highlighted(bool value);
    [[nodiscard]] bool is_highlighted() const;

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ===========================================================================
// ListViewItem — the per-item ListView container (ListViewItem.cs : ListBoxItem
// in WPF; here the WPF-faithful derived item container). Adds the templated
// content host. classify -> Solid.
// ===========================================================================
class VAML_CORE_EXPORT ListViewItem final : public ListBoxItem
{
public:
    ListViewItem();
};

// ===========================================================================
// StatusBarItem — the per-item StatusBar container (StatusBarItem.cs).
// Single-content slot (the bar item's content). classify -> Solid.
// ===========================================================================
class VAML_CORE_EXPORT StatusBarItem final : public UIElement
{
public:
    StatusBarItem();

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ===========================================================================
// TabItem — already a REAL control (S490). Its WPF-faithful full item-container
// surface (TabItem : HeaderedContentControl with Header/IsSelected + the
// content slot) is already delivered at S490 (Element.h). No re-definition
// here; the DAG row flip for TabItem lands in S559. Listed in this step's
// header comment as the parent-edge re-classification target (Header surface
// already real at S490).
// ===========================================================================

// ===========================================================================
// GroupItem — the per-group container for a grouped items view (GroupItem.cs,
// used by GroupStyle/CollectionViewSource). Carries the group content +
// IsExpanded (group header expansion). classify -> Solid.
// ===========================================================================
class VAML_CORE_EXPORT GroupItem final : public UIElement
{
public:
    GroupItem();

    [[nodiscard]] SetResult set_is_expanded(bool value);
    [[nodiscard]] bool is_expanded() const;

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ===========================================================================
// DataGridRow — the per-row container of the S494 DataGrid (DataGridRow.cs).
// Carries the row content + its DataGridCellsPresenter role (the cell hosts).
// classify -> Solid.
// ===========================================================================
class VAML_CORE_EXPORT DataGridRow final : public UIElement
{
public:
    DataGridRow();

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ===========================================================================
// DataGridCell — the per-cell container of the S494 DataGrid (DataGridCell.cs).
// Carries a single cell's content (the column value host). classify -> Solid.
// ===========================================================================
class VAML_CORE_EXPORT DataGridCell final : public UIElement
{
public:
    DataGridCell();

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

// ===========================================================================
// ItemsPresenter — the ItemsControl's view into its items (ItemsPresenter.cs).
// Hosts the ItemsHost panel (the ItemsPanelTemplate instance) inside the
// control's template. It carries a single child (the ItemsHost panel) which it
// measures/arranges to fill. classify -> None (its child renders).
// ===========================================================================
class VAML_CORE_EXPORT ItemsPresenter final : public UIElement
{
public:
    ItemsPresenter();

    // --- ItemsHost (single-content slot — the ItemsPanelTemplate instance) --
    void set_items_host(std::unique_ptr<UIElement> host);
    [[nodiscard]] UIElement* items_host() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* host_ = nullptr;
};

// ===========================================================================
// FrameworkTemplate — the WPF-faithful base of DataTemplate/ControlTemplate/
// ItemsPanelTemplate (FrameworkTemplate.cs). An authored content spec with a
// LoadContent seam + a single visual-tree root slot. DataTemplate already
// delivers this contract (S500) on DependencyObject; this base is the shared
// parent placed HERE ahead of 11n-3 per the DAG row it feeds (recorded NO-GAP).
// ===========================================================================

// ===========================================================================
// ItemsPanelTemplate — the ItemsControl's panel template (ItemsPanelTemplate.cs
// : FrameworkTemplate). WPF-faithful: it produces a Panel instance (the
// ItemsHost) that the ItemsControl's ItemsPresenter surfaces its items into.
// A REAL resource type (FrameworkTemplate : DependencyObject, unified 11n-3)
// with an ItemsPanel root slot + load_items_panel() seam. classify: not a
// visual element (resource).
// ===========================================================================
class VAML_CORE_EXPORT ItemsPanelTemplate : public FrameworkTemplate
{
public:
    ItemsPanelTemplate();
    ~ItemsPanelTemplate() override;

    // The authored panel-root spec (an owned UIElement subtree — usually a real
    // Panel). `set_panel_root` takes OWNERSHIP.
    void set_panel_root(std::unique_ptr<UIElement> root);
    [[nodiscard]] const UIElement* panel_root() const noexcept { return panel_root_.get(); }
    [[nodiscard]] UIElement* panel_root() noexcept { return panel_root_.get(); }

    // --- load_items_panel(): a FRESH Panel instance per ItemsControl. -------
    // Returns a deep-cloned, independent UIElement (the spec is never a live
    // shared node). Caller owns the returned panel.
    [[nodiscard]] std::unique_ptr<UIElement> load_items_panel() const;

private:
    std::unique_ptr<UIElement> panel_root_;
};

// ===========================================================================
// ItemContainerTemplate — the per-item container template (ItemContainerTemplate.cs
// : DataTemplate). DataTemplate already delivers the DataType + LoadContent
// contract (S500); this is the WPF-faithful item-container specialization (a
// template whose content is the per-item container, e.g. ListBoxItem). A REAL
// resource type. classify: not a visual element (resource).
// ===========================================================================
class VAML_CORE_EXPORT ItemContainerTemplate : public DataTemplate
{
public:
    ItemContainerTemplate();
};

// ===========================================================================
// GridView / GridViewColumn / GridViewHeaderRowPresenter — the ListView
// "GridView" detail view chrome (GridView.cs). A GridView owns an ordered
// GridViewColumn collection (header + width + a cell-template) + the
// row-presenter surface (GridViewRowPresenter/GridViewHeaderRowPresenter/
// GridViewRowPresenterBase) that arranges the columns. classify -> None (the
// header row presenter renders the chrome) / Solid (the column header cells).
// ===========================================================================
class VAML_CORE_EXPORT GridViewColumn
{
public:
    GridViewColumn() = default;
    GridViewColumn(std::string header, float width)
        : header_(std::move(header)), width_(width) {}

    [[nodiscard]] const std::string& header() const noexcept { return header_; }
    [[nodiscard]] float width() const noexcept { return width_; }
    void set_width(float w) noexcept { width_ = w; }

private:
    std::string header_ = {};
    float       width_  = 100.0f;
};

class VAML_CORE_EXPORT GridViewColumnCollection
{
public:
    GridViewColumnCollection() = default;
    [[nodiscard]] std::size_t size() const noexcept { return cols_.size(); }
    void add(GridViewColumn col);
    [[nodiscard]] const GridViewColumn& at(std::size_t i) const noexcept { return cols_[i]; }
    [[nodiscard]] const std::vector<GridViewColumn>& columns() const noexcept { return cols_; }

private:
    std::vector<GridViewColumn> cols_;
};

class VAML_CORE_EXPORT GridViewRowPresenterBase : public UIElement
{
public:
    GridViewRowPresenterBase();
    ~GridViewRowPresenterBase() override;

    // --- Columns (owned reversed-view collection) --------------------------
    // The presenter arranges its columns; the header/content row chrome is the
    // GridViewHeaderRowPresenter (a cell header) / GridViewRowPresenter (a data
    // row). Both derive this base (WPF-faithful).
    void set_columns(GridViewColumnCollection cols);
    [[nodiscard]] const GridViewColumnCollection& columns() const noexcept;

protected:
    GridViewColumnCollection cols_;
};

class VAML_CORE_EXPORT GridViewRowPresenter final : public GridViewRowPresenterBase
{
public:
    GridViewRowPresenter();

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

class VAML_CORE_EXPORT GridViewHeaderRowPresenter final : public GridViewRowPresenterBase
{
public:
    GridViewHeaderRowPresenter();

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
};

class VAML_CORE_EXPORT GridViewColumnHeader final : public UIElement
{
public:
    GridViewColumnHeader();

    // --- ColumnHeaderRole (REAL DP; Int32) ---------------------------------
    // The header cell role (Normal/Left/Right/Top/Floating). Stored Int32.
    [[nodiscard]] SetResult set_column_header_role(std::int32_t value);
    [[nodiscard]] std::int32_t column_header_role() const;

    void set_content(std::unique_ptr<UIElement> child);
    [[nodiscard]] UIElement* content() const noexcept;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;

private:
    UIElement* content_ = nullptr;
};

class VAML_CORE_EXPORT GridView final : public DependencyObject
{
public:
    GridView();
    ~GridView() override;

    // --- Columns (owned GridViewColumnCollection) --------------------------
    void set_columns(GridViewColumnCollection cols);
    [[nodiscard]] const GridViewColumnCollection& columns() const noexcept;

    // --- ColumnHeaderContainerStyle (resource TargetType, optional) --------
    [[nodiscard]] SetResult set_column_header_container_style(std::string value);
    [[nodiscard]] std::string column_header_container_style() const;

private:
    GridViewColumnCollection cols_;
};

// ===========================================================================
// VirtualizingPanel / VirtualizingStackPanel / SelectiveScrollingGrid — the
// virtualizing layout-panel bases (VirtualizingPanel.cs / VirtualizingStackPanel.cs
// / SelectiveScrollingGrid.cs). Real Panel-based layout containers that the
// ItemsControl VIP (VirtualizingStackPanel) + the DataGrid's
// SelectiveScrollingGrid cell surface consume. classify -> Solid (panel fill).
// ===========================================================================
class VAML_CORE_EXPORT VirtualizingPanel : public Panel
{
public:
    VirtualizingPanel();
    ~VirtualizingPanel() override;
};

class VAML_CORE_EXPORT VirtualizingStackPanel : public VirtualizingPanel
{
public:
    VirtualizingStackPanel();

    // --- IsVirtualizing (REAL DP; Bool via Int32, default true) ------------
    [[nodiscard]] SetResult set_is_virtualizing(bool value);
    [[nodiscard]] bool is_virtualizing() const;

protected:
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;
    void ArrangeOverride(const vaml::Rect& finalSlot) override;
};

class VAML_CORE_EXPORT SelectiveScrollingGrid : public VirtualizingPanel
{
public:
    SelectiveScrollingGrid();

    // --- SelectiveScrollingOrientation (REAL DP; Int32, default None=0) ----
    [[nodiscard]] SetResult set_selective_scrolling_orientation(std::int32_t value);
    [[nodiscard]] std::int32_t selective_scrolling_orientation() const;
};

} // namespace vaml
