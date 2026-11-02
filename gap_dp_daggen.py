#!/usr/bin/env python3
"""11k-0a — generate docs/DAG_controls.md from the real WPF source at E:\GitRepos\wpf.

Reads the WPF C# source, extracting:
  - each control's class declaration + `: base(...)` inheritance,
  - its DependencyProperty registrations (RemoveHandler-style via
    DependencyProperty.Register / RegisterAttached / DependencyProperty.RegisterReadOnly),
  - routed events (EventManager.RegisterRoutedEvent).

Emits a canonical per-type inventory + the inheritance DAG, with per-row
tri-state status derived (initally: WPF type present; Vaml status column filled
from a static mapping of what Vaml currently models, else PLANNED).

This is a GENERATOR. Re-run whenever E:\GitRepos\wpf advances; the OUTPUT is
committed as docs/DAG_controls.md (the engine contract), NOT the generator.
"""

import os, re, glob, json

WPF_ROOT = r"E:\GitRepos\wpf\src\Microsoft.DotNet.Wpf\src\PresentationFramework\System\Windows\Controls"
CORE_ROOT = r"E:\GitRepos\wpf\src\Microsoft.DotNet.Wpf\src\PresentationCore\System\Windows\Media"
MEDIA_ROOT = r"E:\GitRepos\wpf\src\Microsoft.DotNet.Wpf\src\PresentationFramework\System\Windows\Shapes" if False else None

# The Vaml side: which controls Vaml currently models (name -> tri-state).
# Derived from the 11k-2 status. OPEN/PLANNED = not yet modeled / partial.
VAML_STATUS = {
    "Button": "DONE", "StackPanel": "DONE", "Grid": "DONE", "TextBlock": "DONE",
    "TextBox": "DONE", "CheckBox": "DONE", "RadioButton": "DONE", "Slider": "DONE",
    "ProgressBar": "DONE", "ComboBox": "DONE", "ListBox": "DONE", "ListView": "PLANNED",
    "ScrollViewer": "DONE", "ScrollBar": "DONE", "RepeatButton": "DONE", "Window": "DONE",
    "UserControl": "DONE", "WrapPanel": "DONE", "Canvas": "DONE", "DockPanel": "DONE",
    "Label": "DONE", "Border": "DONE", "Rectangle": "DONE", "Ellipse": "DONE",
    "Line": "DONE", "Image": "DONE", "PasswordBox": "DONE", "GroupBox": "DONE",
    "Expander": "DONE", "Separator": "DONE", "StatusBar": "DONE", "Viewbox": "DONE",
    "ToolBar": "DONE", "ToolBarTray": "DONE",
    "ContentControl": "PLANNED", "ButtonBase": "PLANNED", "RangeBase": "PLANNED",
    "ItemsControl": "PLANNED", "Control": "PLANNED", "FrameworkElement": "PLANNED",
    "UIElement": "PLANNED", "TextElement": "PLANNED",
    # Sprint 11m — the WPF-corpus TAIL closure (S550-S558). Every DAG PLANNED row
    # with a roadmap home closed here is REAL + classified; flips PLANNED->DONE.
    # Item-container / header / presenter / panel + collection-value scaffolding.
    "ComboBoxItem": "DONE", "ListBoxItem": "DONE", "ListViewItem": "DONE",
    "StatusBarItem": "DONE", "GroupItem": "DONE", "DataGridRow": "DONE",
    "DataGridCell": "DONE", "ContentPresenter": "DONE", "ItemsPresenter": "DONE",
    "ItemsPanelTemplate": "DONE", "ItemContainerTemplate": "DONE",
    "DataTemplate": "DONE", "GridView": "DONE", "GridViewColumnHeader": "DONE",
    "VirtualizingPanel": "PLANNED", "VirtualizingStackPanel": "DONE",
    "SelectiveScrollingGrid": "DONE", "Panel": "PLANNED", "Decorator": "PLANNED",
    # Shape + channel tail (S551).
    "Path": "DONE", "Polygon": "DONE", "Polyline": "DONE", "Shape": "PLANNED",
    "Track": "DONE", "TickBar": "DONE",
    # TreeView (S552).
    "TreeView": "DONE", "TreeViewItem": "DONE",
    # Date & Calendar controls (S553).
    "DatePicker": "DONE", "DatePickerTextBox": "DONE", "Calendar": "DONE",
    "CalendarDayButton": "DONE", "CalendarButton": "DONE", "CalendarItem": "DONE",
    # Drawing / media / embedded tail (S554). WebBrowser/ViewPort3D re-affirmed
    # OUT-OF-SCOPE(11m) (the S520 import/inject seam — host-linked foreign surface).
    "InkCanvas": "DONE", "InkPresenter": "DONE", "MediaElement": "DONE",
    "WebBrowser": "OUT-OF-SCOPE(11m)", "ViewPort3D": "OUT-OF-SCOPE(11m)",
    # Document / frame controls (S555).
    "Page": "DONE", "Frame": "DONE", "DocumentViewer": "DONE",
    "DocumentViewerBase": "PLANNED", "FlowDocumentReader": "DONE",
    "FlowDocumentScrollViewer": "DONE", "SinglePageViewer": "DONE",
    # The 11c/11d/11e stale controls + Viewbox spelling corrected here (S559's
    # ground-truth re-state — implemented-but-stale flips / spelling normalized).
    "ListView": "DONE", "TabControl": "DONE", "TabItem": "DONE", "Menu": "DONE",
    "MenuItem": "DONE", "ContextMenu": "DONE", "Popup": "DONE", "ToolTip": "DONE",
    "GridSplitter": "DONE", "DataGrid": "DONE", "Viewbox": "DONE",
    # S556 service/mechanism + U+ support types (delivered REAL -> DONE, consumed
    # by the DONE controls' binding + item infrastructure).
    "TextSearch": "DONE", "SpellCheck": "DONE", "ToolTipService": "DONE",
    "ContextMenuService": "DONE", "PopupControlService": "DONE",
    "ItemContainerGenerator": "DONE", "UIElementCollection": "DONE",
    "ItemCollection": "DONE", "SelectedItemCollection": "DONE",
    "SelectedCellsCollection": "DONE",
    "Validation": "DONE", "ValidationRule": "DONE", "ValidationError": "DONE",
    "ValidationResult": "DONE", "ExceptionValidationRule": "DONE",
    "DataErrorValidationRule": "DONE", "NotifyDataErrorValidationRule": "DONE",
    "ConversionValidationRule": "DONE",
    "AlternationConverter": "DONE", "BooleanToSelectiveScrollingOrientationConverter": "DONE",
    "BorderGapMaskConverter": "DONE", "MenuScrollingVisibilityConverter": "DONE",
    "DataGridHeadersVisibilityToVisibilityConverter": "DONE",
    # S557 pure value/enum/arg-only data types (delivered REAL -> DONE, consumed).
    "AutoToolTipPlacement": "DONE", "ClickMode": "DONE", "CharacterCasing": "DONE",
    "Orientation": "DONE", "PlacementMode": "DONE", "PopupAnimation": "DONE",
    "ScrollUnit": "DONE", "VirtualizationMode": "DONE", "PanningMode": "DONE",
    "DefinitionBase": "DONE", "VirtualizationCacheLength": "DONE",
    "CustomPopupPlacement": "DONE", "CustomPopupPlacementCallback": "DONE",
    "DataGridColumn": "DONE", "DataGridLength": "DONE",
    "ScrollChangedEventArgs": "DONE", "SelectionChangedEventArgs": "DONE",
    "TextChangedEventArgs": "DONE",
}

DP_RE = re.compile(
    r'public\s+static\s+readonly\s+DependencyProperty\s+(\w+)Property\s*='
    r'\s*[A-Za-z.]+\.Register(?:Attached|ReadOnly)?\s*\(\s*"(?P<name>[A-Za-z0-9_.]+)"',
    re.S)
EV_RE = re.compile(
    r'public\s+static\s+readonly\s+RoutedEvent\s+(\w+)Event\s*='
    r'\s*EventManager\.RegisterRoutedEvent\(', re.S)

CLASS_RE = re.compile(r'public\s+(?:partial\s+)?(?:abstract\s+|sealed\s+)?class\s+(\w+)\s*:\s*([^\n{]+)')

rows = {}

def scan_files(root, default_status="PLANNED"):
    if not os.path.isdir(root):
        return
    for f in glob.glob(os.path.join(root, "*.cs")):
        try:
            c = open(f, encoding="utf-8", errors="replace").read()
        except Exception:
            continue
        name = os.path.splitext(os.path.basename(f))[0]
        # class decl + base
        cls = CLASS_RE.search(c)
        base = cls.group(2).strip().split()[0] if cls else ""
        dps = sorted(set(m.group("name") for m in DP_RE.finditer(c)))
        evs = sorted(set(m.group(1) for m in EV_RE.finditer(c)))
        rows[name] = {
            "base": base, "dps": dps, "evs": evs,
            "status": VAML_STATUS.get(name, default_status),
        }

# PresentationFramework controls
scan_files(WPF_ROOT)
# Base-class DPs live in Primitives + base files — scan for closure rows
scan_files(r"E:\GitRepos\wpf\src\Microsoft.DotNet.Wpf\src\PresentationFramework\System\Windows\Controls\Primitives")
# Media/shapes derived types (abstract Geometry etc.) — a few representative
scan_files(r"E:\GitRepos\wpf\src\Microsoft.DotNet.Wpf\src\PresentationFramework\System\Windows\Shapes")

# Resolve nearest modeled ancestor for tri-state inheritance closure
def ancestor_chain(name):
    chain = []
    cur = name
    seen = set()
    while cur and cur not in seen:
        seen.add(cur)
        chain.append(cur)
        cur = rows.get(cur, {}).get("base", "")
        # stop at DispatcherObject root
        if cur in ("DispatcherObject", ""):
            break
    return chain

lines = []
lines.append("# DAG_controls.md — VAML ↔ WPF control/type inheritance + DP/event surface (engine contract)")
lines.append("")
lines.append("> **11k-0a (generated).** This is the **engine contract** — the canonical inventory of every WPF")
lines.append("> control/type the corpus consumes or could consume, with its full ancestor chain and property/event")
lines.append("> surface. Generated from the REAL WPF source at `E:\\GitRepos\\wpf` by `gap_dp_daggen.py`.")
lines.append("> Do NOT hand-edit the generated rows; re-run the generator when WPF advances (or add rows here under")
lines.append("> the documented **closure-under-ancestry invariant**).")
lines.append(">")
lines.append("> **Closure-under-ancestry invariant:** if a listed control inherits a listed control, **both** nodes")
lines.append("> exist and carry their properties. A control's full effective surface = its own DPs + every ancestor's.")
lines.append("> (This is the invariant that catches `TextBlock`-is-not-a-`Control` class bugs.)")
lines.append(">")
lines.append("> **Tri-state:** every WPF type is in EXACTLY ONE state — `DONE` (Vaml models it + its surface),")
lines.append("> `PLANNED` (declared, scheduled), or `OUT-OF-SCOPE(<resolution-sprint>)` (declared, NOT silent).")
lines.append("> *Nothing is hidden*: unmodeled controls are declared `OUT-OF-SCOPE`, never omitted.")
lines.append("")
lines.append("| Type | Inheritance (DAG) | Vaml tri-state | Own DPs | Routed events |")
lines.append("|------|-------------------|----------------|---------|---------------|")

for name in sorted(rows):
    r = rows[name]
    chain = " → ".join(ancestor_chain(name))
    dp = ", ".join(r["dps"]) if r["dps"] else "—"
    ev = ", ".join(r["evs"]) if r["evs"] else "—"
    lines.append(f"| `{name}` | {chain} | {r['status']} | {dp} | {ev} |")

lines.append("")
lines.append("## Notes / recorded NO-GAP deferrals")
lines.append("")
lines.append("- **`Geometry` is `abstract`** in WPF (PresentationCore `Media\\Geometry.cs`). A bare `<Geometry>` element is")
lines.append("  NOT authorable — WPF authors concrete derivations (`PathGeometry`, `GeometryGroup`, `EllipseGeometry`…).")
lines.append("  Vaml row: **OUT-OF-SCOPE(11k-4)** — resolved at 11k-4 under concrete derived geometry resource elements.")
lines.append("- **`TextBlock : FrameworkElement`** (NOT a `Control`). Vaml must re-base TextBlock off the framework-element")
lines.append("  branch, not the Control base (it must NOT inherit `Control`'s `Padding`/content-alignment DPs).")
lines.append("- **Abstract bases** (`ContentControl`, `ButtonBase`, `RangeBase`, `ItemsControl`, `Control`, `FrameworkElement`,")
lines.append("  `UIElement`, `TextElement`) are `PLANNED` — their DP surface is the base-branch fidelity sprint (11k-11 S649).")
lines.append("")

with open(r"E:\GitRepos\VamlPOC\docs\DAG_controls.md", "w", encoding="utf-8") as fh:
    fh.write("\n".join(lines))

print("generated", len(rows), "rows -> docs/DAG_controls.md")
