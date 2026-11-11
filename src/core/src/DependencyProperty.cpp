// S039 — DependencyProperty out-of-line definitions: the Register landing
// site + global companion table + the read path. This is the authored,
// resolved S036-re-scope global static registry surface (recorded NOT silent).
// S801 re-keys the companion GlobalTable + read path onto the COMPOSITE
// (name, owner) pair (WPF `FromNameKey(name, ownerType)` ground truth; S033/S039
// name-only drift resolved at its source).
//
// S146 (refactor gate): the pimpl (class Impl) is COLLAPSED — DependencyProperty
// now holds its definitional data (name_, metadata_, ownerTypeId_) as plain
// by-value members. The companion GlobalTable below keeps the S143 content-keyed
// vaml::detail::NameTable, re-keyed S801 onto the (name, owner) composite.

#include "vaml/core/DependencyProperty.h"

#include "vaml/core/PropertyRegistry.h"
#include "vaml/core/detail/NameTable.h" // S143 (B4): content-keyed probe store for GlobalTable

#include <string>
#include <utility>
#include <vector> // S803: owner-chain resolution (std::vector<owner> chain)

namespace vaml {

VAML_CORE_EXPORT
DependencyProperty::DependencyProperty(std::string name,
                                       PropertyMetadata metadata, std::string ownerTypeId)
    : name_(std::move(name))
    , metadata_(std::move(metadata))
    , ownerTypeId_(std::move(ownerTypeId))
{
}

VAML_CORE_EXPORT
DependencyProperty::DependencyProperty(DependencyProperty&&) noexcept = default;

VAML_CORE_EXPORT
DependencyProperty& DependencyProperty::operator=(DependencyProperty&&) noexcept = default;

VAML_CORE_EXPORT
DependencyProperty::~DependencyProperty() = default;

// The companion static table: (name, owner) -> the full DependencyProperty
// definition. Lives for the process lifetime alongside PropertyRegistry::global().
// Function-local static => no static-init-order fiasco.
//
// S143 (B4) re-keys GlobalTable OFF std::unordered_map and ONTO the S141-pattern
// content-keyed probe store (NameTable), which OWNS its stable key nodes (process
// lifetime) and matches BY CONTENT with NO transient std::string materialized on
// lookup. S801 re-keys that store's key onto the (name, owner) COMPOSITE (the
// S140 DECISION 1 resolution for a GLOBAL table — self-owned stable composite
// storage, content-keyed, ADR-018: no lock added).
namespace {

struct GlobalTable
{
    vaml::detail::NameTable<std::unique_ptr<DependencyProperty>,
                            vaml::detail::CompositeKey> byName;

    static GlobalTable& instance()
    {
        static GlobalTable table;
        return table;
    }
};

} // namespace

VAML_CORE_EXPORT
const DependencyProperty& DependencyProperty::Register(
    std::string_view name, ValueType type, std::string_view ownerTypeId, PropertyMetadata metadata)
{
    // Type coherence: never register a key/metadata pair that disagree on the
    // bound value type (NO-LAZY guard).
    if (type != metadata.boundType())
    {
        throw ValueTypeMismatch("DependencyProperty::Register: key type does not match metadata's bound value type");
    }

    const std::string ownedName(name);
    const std::string ownedOwner(ownerTypeId);

    // Dedupe on the (name, ownerTypeId) PAIR globally (WPF-style). register_key
    // throws PropertyAlreadyRegistered only on a SAME-PAIR collision and leaves
    // the registry (and thus our companion table) unchanged. A same-name/
    // different-owner pair is DISTINCT.
    PropertyRegistry& registry = PropertyRegistry::global();
    PropertyKey key(ownedName, type, ownedOwner, metadata.flags());
    (void)registry.register_key(std::move(key));

    // Insert the full (name+metadata) definition into the companion table under
    // the (name, owner) pair. If the pair is somehow already present here
    // (cannot happen given registry dedupe above), keep the EXISTING
    // registration (store unchanged).
    GlobalTable& table = GlobalTable::instance();
    auto owner = std::unique_ptr<DependencyProperty>(
        new DependencyProperty(ownedName, std::move(metadata), ownedOwner));
    std::unique_ptr<DependencyProperty>* stored =
        table.byName.insert(ownedName, ownedOwner, std::move(owner));
    if (stored == nullptr)
    {
        // Already present (defensive, cannot happen given the registry dedupe
        // above): keep the EXISTING registration (store unchanged).
        const std::unique_ptr<DependencyProperty>* existing =
            table.byName.find(ownedName, ownedOwner);
        return *existing->get();
    }
    return **stored;
}

VAML_CORE_EXPORT
const std::string& DependencyProperty::owner_type_id() const noexcept
{
    return ownerTypeId_;
}

VAML_CORE_EXPORT
const DependencyProperty* DependencyProperty::TryGet(std::string_view name,
                                                     std::string_view owner) noexcept
{
    // S143 B4 + S801 composite: content lookup with NO transient std::string
    // materialized. NEVER an owner-less lookup — the owner is always carried.
    GlobalTable& table = GlobalTable::instance();
    const std::unique_ptr<DependencyProperty>* found = table.byName.find(name, owner);
    if (found == nullptr || *found == nullptr)
    {
        return nullptr;
    }
    return found->get();
}


// ---- S803 OWNER-CHAIN table (single authority; shared by the S2 reader, the
// ---- S7 compiler/validators, and the release VBamlReader). --------------
// The DECLARED OWNER-CHAIN for an ELEMENT TYPE-NAME: the ordered declaring-
// owner-name list (most-derived first) a BARE property name on that element is
// resolved against. Mirrors WPF's XAML type-inheritance DP resolution (a DP's
// owner is its declaring type, so `Padding=5` on a Button hits owner "Control",
// `Text=...` on a TextBlock hits owner "TextBlock"). The CONCRETE owner (the
// S803 re-keyed control-specific DPs under, WPF-faithful — e.g. ("Value","Slider"),
// ("Row","Grid"), ("FillRule","PathGeometry")) is FIRST; then the DECLARED
// base-owner labels the shared DPs are REGISTERED under ("Control"/
// "FrameworkElement"/"UIElement"/"Selector"/...). Unknown/missing types still
// resolve the universal base owners, so a test-authored or base-typed owner
// label keeps working.
namespace {

void push_concrete_owner(std::string_view typeName, std::vector<std::string_view>& out)
{
    if (typeName == "TextBlock")            { out.push_back("TextBlock"); return; }
    if (typeName == "Button")               { out.push_back("Button"); return; }
    if (typeName == "RepeatButton")         { out.push_back("RepeatButton"); return; }
    if (typeName == "Grid")                 { out.push_back("Grid"); return; }
    if (typeName == "Canvas")               { out.push_back("Canvas"); return; }
    if (typeName == "StackPanel")           { out.push_back("StackPanel"); return; }
    if (typeName == "WrapPanel")            { out.push_back("WrapPanel"); return; }
    if (typeName == "DockPanel")            { out.push_back("DockPanel"); return; }
    if (typeName == "RadioButton")          { out.push_back("RadioButton"); return; }
    if (typeName == "ComboBox")             { out.push_back("ComboBox"); return; }
    if (typeName == "ListBox" ||
        typeName == "ListView")             { out.push_back("ListBox"); return; }
    if (typeName == "TextBox")              { out.push_back("TextBox"); return; }
    if (typeName == "Icon")                 { out.push_back("Icon"); return; }
    if (typeName == "Slider")               { out.push_back("Slider"); return; }
    if (typeName == "CheckBox")             { out.push_back("CheckBox"); return; }
    if (typeName == "TreeView")             { out.push_back("TreeView"); return; }
    if (typeName == "TreeViewItem")         { out.push_back("TreeViewItem"); return; }
    if (typeName == "CalendarDayButton")    { out.push_back("CalendarDayButton"); return; }
    if (typeName == "CalendarButton")       { out.push_back("CalendarButton"); return; }
    if (typeName == "CalendarItem")         { out.push_back("CalendarItem"); return; }
    if (typeName == "Calendar")             { out.push_back("Calendar"); return; }
    if (typeName == "DatePickerTextBox")    { out.push_back("DatePickerTextBox"); return; }
    if (typeName == "DatePicker")           { out.push_back("DatePicker"); return; }
    if (typeName == "InkCanvas")            { out.push_back("InkCanvas"); return; }
    if (typeName == "InkPresenter")         { out.push_back("InkPresenter"); return; }
    if (typeName == "MediaElement")         { out.push_back("MediaElement"); return; }
    if (typeName == "Page")                 { out.push_back("Page"); return; }
    if (typeName == "Frame")                { out.push_back("Frame"); return; }
    if (typeName == "DocumentViewerBase")   { out.push_back("DocumentViewerBase"); return; }
    if (typeName == "SinglePageViewer")     { out.push_back("SinglePageViewer"); return; }
    if (typeName == "Label")                { out.push_back("Label"); return; }
    if (typeName == "Border")               { out.push_back("Border"); return; }
    if (typeName == "Rectangle")            { out.push_back("Rectangle"); return; }
    if (typeName == "Ellipse")              { out.push_back("Ellipse"); return; }
    if (typeName == "Line")                 { out.push_back("Line"); return; }
    if (typeName == "Shape")                { out.push_back("Shape"); return; }
    if (typeName == "Path")                 { out.push_back("Path"); return; }
    if (typeName == "Polygon")              { out.push_back("Polygon"); return; }
    if (typeName == "Polyline")             { out.push_back("Polyline"); return; }
    if (typeName == "Track")                { out.push_back("Track"); return; }
    if (typeName == "TickBar")              { out.push_back("TickBar"); return; }
    if (typeName == "Image")                { out.push_back("Image"); return; }
    if (typeName == "ProgressBar")          { out.push_back("ProgressBar"); return; }
    if (typeName == "PasswordBox")          { out.push_back("PasswordBox"); return; }
    if (typeName == "GroupBox")             { out.push_back("GroupBox"); return; }
    if (typeName == "Expander")             { out.push_back("Expander"); return; }
    if (typeName == "Separator")            { out.push_back("Separator"); return; }
    if (typeName == "StatusBar")            { out.push_back("StatusBar"); return; }
    if (typeName == "ScrollBar")            { out.push_back("ScrollBar"); return; }
    if (typeName == "Viewbox")              { out.push_back("Viewbox"); return; }
    if (typeName == "ToolBar")              { out.push_back("ToolBar"); return; }
    if (typeName == "ToolBarTray")          { out.push_back("ToolBarTray"); return; }
    if (typeName == "TabItem")              { out.push_back("TabItem"); return; }
    if (typeName == "TabControl")           { out.push_back("TabControl"); return; }
    if (typeName == "MenuItem")             { out.push_back("MenuItem"); return; }
    if (typeName == "Menu")                 { out.push_back("Menu"); return; }
    if (typeName == "ContextMenu")          { out.push_back("ContextMenu"); return; }
    if (typeName == "ToolTip")              { out.push_back("ToolTip"); return; }
    if (typeName == "Popup")                { out.push_back("Popup"); return; }
    if (typeName == "GridSplitter")         { out.push_back("GridSplitter"); return; }
    if (typeName == "DataGrid")             { out.push_back("DataGrid"); return; }
    if (typeName == "RichTextBox")          { out.push_back("RichTextBox"); return; }
    if (typeName == "Window")               { out.push_back("Window"); return; }
    if (typeName == "UserControl")          { out.push_back("UserControl"); return; }
    if (typeName == "Application")          { out.push_back("Application"); return; }
    // Geometry/drawing/brush/vendor resource owners (authored element-type
    // names) so bare keys on these (FillRule, PenThickness, Offset, Points...)
    // resolve through the standard owner-chain lookup.
    if (typeName == "PathGeometry")         { out.push_back("PathGeometry"); return; }
    if (typeName == "GeometryGroup")        { out.push_back("GeometryGroup"); return; }
    if (typeName == "RectangleGeometry")    { out.push_back("RectangleGeometry"); return; }
    if (typeName == "EllipseGeometry")      { out.push_back("EllipseGeometry"); return; }
    if (typeName == "LineGeometry")         { out.push_back("LineGeometry"); return; }
    if (typeName == "GeometryDrawing")      { out.push_back("GeometryDrawing"); return; }
    if (typeName == "DrawingGroup")         { out.push_back("DrawingGroup"); return; }
    if (typeName == "DrawingBrush")         { out.push_back("DrawingBrush"); return; }
    if (typeName == "DrawingImage")         { out.push_back("DrawingImage"); return; }
    if (typeName == "ImageBrush")           { out.push_back("ImageBrush"); return; }
    if (typeName == "LinearGradientBrush")  { out.push_back("LinearGradientBrush"); return; }
    if (typeName == "GradientStop")         { out.push_back("GradientStop"); return; }
    if (typeName == "Viewport3D")           { out.push_back("Viewport3D"); return; }
    if (typeName == "ModelVisual3D")        { out.push_back("ModelVisual3D"); return; }
    if (typeName == "MeshGeometry3D")       { out.push_back("MeshGeometry3D"); return; }
    if (typeName == "GeometryModel3D")      { out.push_back("GeometryModel3D"); return; }
    if (typeName == "Model3DGroup")         { out.push_back("Model3DGroup"); return; }
    if (typeName == "DiffuseMaterial")      { out.push_back("DiffuseMaterial"); return; }
    if (typeName == "PerspectiveCamera")    { out.push_back("PerspectiveCamera"); return; }
    if (typeName == "OrthographicCamera")   { out.push_back("OrthographicCamera"); return; }
}

void collect_owner_chain(std::string_view typeName, std::vector<std::string_view>& out)
{
    out.reserve(12);
    push_concrete_owner(typeName, out);
    if (typeName == "Button" || typeName == "RepeatButton" || typeName == "MenuItem")
    {
        out.push_back("ButtonBase"); // ICommandSource shared Command surface
    }
    if (typeName == "ListBox" || typeName == "ListView" || typeName == "ComboBox" ||
        typeName == "TabControl" || typeName == "DataGrid")
    {
        out.push_back("Selector"); // Selector / ItemsControl shared surface
    }
    // S803 (verified shared-owner reuse): ScrollBar / ToolBar / ToolBarTray
    // REUSE the single StackPanel-owned "Orientation" DP (their accessors Set/
    // Get PropertyId::of("Orientation","StackPanel") — no per-type registration).
    // A bare `Orientation=` authored on any of these must resolve through the
    // shared owner "StackPanel" in the chain-walk (mirrors the ButtonBase/
    // Selector shared-surface pattern above).
    if (typeName == "ScrollBar" || typeName == "ToolBar" || typeName == "ToolBarTray")
    {
        out.push_back("StackPanel"); // shared re-pointed Orientation owner
    }
    // Universal base-owner labels last (shared Control/FrameworkElement/
    // UIElement DPs resolve on every UI element regardless of the authored
    // type-owner label).
    out.push_back("Control");
    out.push_back("FrameworkElement");
    out.push_back("UIElement");
    out.push_back("DependencyObject");
}

} // namespace

VAML_CORE_EXPORT
const DependencyProperty* DependencyProperty::ResolveForElement(
    std::string_view name, std::string_view elementTypeName) noexcept
{
    // S803 — dotted keys carry their OWN owner explicitly (the `Owner.Property`
    // authored form): split the last dot and TryGet(tail, owner) directly — NO
    // chain walk (the element type is irrelevant; the owner is stated). This
    // makes the single authority resolve BOTH forms exactly as WPF's XAML
    // object/attached-property resolution does, so reader + tool callers (which
    // hand in either a bare attribute key or a dotted one) share one resolver.
    const std::size_t dot = name.rfind('.');
    if (dot != std::string_view::npos)
    {
        const std::string_view owner = name.substr(0, dot);
        const std::string_view tail  = name.substr(dot + 1);
        return TryGet(tail, owner);
    }
    // Bare key: walk the element type's DECLARED OWNER-CHAIN (most-derived
    // first) calling TryGet(name, owner) at each level.
    std::vector<std::string_view> chain;
    collect_owner_chain(elementTypeName, chain);
    for (const std::string_view owner : chain)
    {
        if (const DependencyProperty* dp = TryGet(name, owner))
        {
            return dp;
        }
    }
    return nullptr;
}

VAML_CORE_EXPORT
const DependencyProperty& DependencyProperty::Get(std::string_view name,
                                                  std::string_view owner)
{
    const DependencyProperty* dp = TryGet(name, owner);
    if (dp == nullptr)
    {
        throw PropertyNotFound("DependencyProperty::Get: no property registered for this (name, owner) pair");
    }
    return *dp;
}

VAML_CORE_EXPORT
PropertyId DependencyProperty::property_id() const noexcept
{
    // The id wraps the OWNED name + owner as string_views. Both back the
    // object's OWNED name_ / ownerTypeId_ permanent std::strings, so the views
    // are valid for the process lifetime.
    return PropertyId::of(name_, ownerTypeId_);
}

VAML_CORE_EXPORT
const PropertyMetadata& DependencyProperty::metadata() const noexcept
{
    return metadata_;
}

VAML_CORE_EXPORT
PropertyFlags DependencyProperty::flags() const noexcept
{
    return metadata_.flags();
}

} // namespace vaml
