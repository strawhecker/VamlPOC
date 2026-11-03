#!/usr/bin/env python3
# Sprint 11n 11n-0 — DAG UNCOVER + UPDATE (the DEFINED drift-closure step).
#
# gap_dp_daggen.py is a GENERATOR that OVERWRITES the whole docs/DAG_controls.md
# (the S503a GEN-LOSSY note): re-running it wipes the manual-add rows. This script
# therefore:
#   1) runs the generator (base-regen — the stale-PLANNED killer; 282 rows),
#   2) re-applies the manual-add NOTE blocks verbatim (the S503a GEN-LOSSY escape):
#      the 12k-5 trigger family, the 11f behaviors/DataTemplate family, and the
#      NEW 11n-0 DAG-EXT cluster (the anchors 11n carries).
#   It NEVER copies generated `|` table rows back, so the regenerated DAG keeps
#   EXACTLY-ONE-STATE per row (the DAG contract).
import os, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DAG = os.path.join(HERE, "docs", "DAG_controls.md")

def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()

def write(path, s):
    with open(path, "w", encoding="utf-8") as f:
        f.write(s)

# 1) run the generator (base-regen)
subprocess.run([sys.executable, os.path.join(HERE, "gap_dp_daggen.py")],
               check=True, cwd=HERE)

# The manual-add NOTE blocks re-applied verbatim (the S503a GEN-LOSSY escape).
# These are CONSTANTS authored here — NEVER copied from generated rows. They
# declare the manually-added DAG surface the generator cannot see.
MANUAL_BLOCKS = [
"""\
> **12k-5 trigger family — MANUAL ADD (closure-under-ancestry / GEN-LOSSY escape):**
> the framework `TriggerBase`/`TriggerAction`/`SetterBase` + the concrete
> `BeginStoryboard`/`SoundPlayerAction`/`ControllableStoryboardAction` rows live OUTSIDE the
> generator's `Controls`/`Primitives`/`Shapes` scan scope (they are `System.Windows`
> core trigger/action types). Added under the closure-under-ancestry invariant so
> `SoundPlayerAction -> TriggerAction` (and the trigger action ancestry) is NOT a dangling
> parent edge. Row states: trigger family PLANNED until the trigger/action delivery steps;
> the interactivity EventTrigger (Microsoft.Xaml.Behaviors) is DISTINCT from the
> System.Windows Style.Triggers EventTrigger (S503a).""",

"""\
> **11f behaviors / DataTemplate — MANUAL ADD (DAG-EXT-11f, GEN-LOSSY escape):**
> the Microsoft.Xaml.Behaviors family (`Behavior -> DependencyObject`, `Behavior<T> -> Behavior`,
> `BehaviorCollection`, `TriggerAction -> DependencyObject`, `InvokeCommandAction -> TriggerAction`,
> interactivity `EventTrigger -> TriggerBase` with EventName/SourceObject) + the missing
> `DataTemplate` row (closing the dangling `ItemContainerTemplate -> DataTemplate` parent edge).
> States: the behaviors rows flipped PLANNED->DONE at S503a (11f) — behavior/trigger/action are
> REAL; the System.Windows `EventTrigger` (Style.Triggers) row stays PLANNED (the Style.Triggers
> family, delivered later).""",

"""\
> **11n-0 DAG-EXT cluster — MANUAL ADD (the DAG-anchor closure 11n carries, GEN-LOSSY escape):**
> the 11n sprint's binding/input/observable/3D/visual anchors live OUTSIDE the generator's
> `Controls`/`Primitives`/`Shapes` scan scope (they are `System.Windows.Input`,
> `System.Windows.Data`, `System.Windows.Media`, `System.Windows` core interfaces + base
> classes). Recorded under the closure-under-ancestry invariant so each anchor is a named,
> forward-resolvable DAG row (NO-GAP — never silent). The anchors 11n unifies under + their
> uncover-driven additions (per the 11n-0 roadmap block):
>
> **Unify-under anchors (the shared bases/interfaces 11n's additions implement):**
> - `ICommand` (EXISTING S246 `vaml::ICommand`) — the single command interface; 11n-2 EXTENDS it.
> - `ICommandSource` (the ButtonBase touchpoint) — Command/CommandParameter/CommandTarget.
> - `Freezable` / `Animatable` — immutable/animatable content base; `Animatable -> Freezable`.
> - `BindingBase` / `BindingExpressionBase` / `MarkupExtension` — the binding spine.
> - `FrameworkTemplate` — shared by ControlTemplate/DataTemplate/ItemsPanelTemplate.
> - `DispatcherObject` — the affinity/threading base (S037/S041/S143).
> - `IWeakEventListener` — the subscription-lifetime contract (11n-4 Option A).
> - `ICollectionView` / `INotifyPropertyChanged` / `INotifyCollectionChanged` — the reactive spine.
>
> **Uncover-driven additions (base -> new, each recorded as a named row):**
> - `ICommand` -> `RoutedCommand` + `CommandManager` (REUSES the existing S246 `vaml::ICommand` —
>   NEVER a parallel interface, NEVER a bridge; see the 11n-2 DECISION record).
> - `InputGesture` -> `KeyGesture` / `MouseGesture`.
> - `InputBinding`(ICommandSource) -> `KeyBinding` / `MouseBinding`.
> - `Visual3D`(DependencyObject, NOT Visual) -> `Viewport3D`(FrameworkElement, IAddChild) +
>   `Model3D`(Animatable) + `Geometry3D`(Animatable).
> - `BindingBase` -> `Binding` / `MultiBinding` / `PriorityBinding`.
> - `BindingExpressionBase` -> `BindingExpression` (implements the S240/S242 `vaml::BindingExpression`).
> - `FrameworkTemplate` -> `ItemsPanelTemplate`.
> - `CollectionView : DispatcherObject, ICollectionView, INotifyPropertyChanged`.
>
> Row states: these anchors are the 11n step surface — flipped DONE as each 11n step delivers
> its REAL surface (11n-1 AutomationProperties, 11n-2 ICommand spine, 11n-3 FrameworkTemplate,
> 11n-4 reactive spine, 11n-5 3D, 11n-6 Adorner/VisualTreeHelper); PLANNED until then, never
> silent. ZERO MINIMAL REAL + ZERO invented names (WPF-real type names verified against
> `E:\\GitRepos\\wpf`).""",
]

out = read(DAG).rstrip() + "\n"
out += "\n## Manual-add rows (re-applied verbatim — the S503a GEN-LOSSY escape)\n\n"
out += "\n\n".join(MANUAL_BLOCKS) + "\n"

# --- 11n-7 DAG CLOSE: the 11n anchor inventory as FIRST-CLASS table rows ---
#
# The generator cannot scan System.Windows.Input / System.Windows.Data /
# System.Windows.Media / System.Windows core interfaces + base classes (they are
# OUTSIDE its Controls/Primitives/Shapes scope), yet the closure-under-ancestry
# tri-state contract demands every anchor be a REAL row in EXACTLY ONE state.
# These rows are therefore produced here (the GEN-LOSSY escape) so they survive
# base re-generation. Each 11n body step delivered a REAL permanent surface for
# its anchor(s) (11n-1..11n-6 all shipped + BOTH CI lanes green), so every row is
# DONE at the 11n-7 wrap. ZERO MINIMAL REAL + ZERO invented names (WPF-real type
# names verified against E:\GitRepos\wpf).
ANCHOR_ROWS = [
    # --- Unify-under anchors (shared bases/interfaces 11n's additions implement) ---
    "| `ICommand` | ICommand | DONE | — | CanExecuteChanged |",
    "| `ICommandSource` | ICommandSource | DONE | Command, CommandParameter, CommandTarget | — |",
    "| `Freezable` | Freezable → DependencyObject | DONE | — | — |",
    "| `Animatable` | Animatable → Freezable | DONE | — | — |",
    "| `BindingBase` | BindingBase → MarkupExtension | DONE | — | — |",
    "| `BindingExpressionBase` | BindingExpressionBase → Freezable | DONE | — | — |",
    "| `MarkupExtension` | MarkupExtension | DONE | — | — |",
    "| `FrameworkTemplate` | FrameworkTemplate → DependencyObject | DONE | — | — |",
    "| `DispatcherObject` | DispatcherObject | DONE | — | — |",
    "| `IWeakEventListener` | IWeakEventListener | DONE | — | — |",
    "| `ICollectionView` | ICollectionView | DONE | — | — |",
    "| `INotifyPropertyChanged` | INotifyPropertyChanged | DONE | — | PropertyChanged |",
    "| `INotifyCollectionChanged` | INotifyCollectionChanged | DONE | — | CollectionChanged |",
    # --- Uncover-driven additions (base -> new) ---
    "| `RoutedCommand` | RoutedCommand → ICommand | DONE | Name, OwnerType | CanExecuteChanged |",
    "| `CommandManager` | CommandManager | DONE | — | RequerySuggested |",
    "| `InputGesture` | InputGesture | DONE | — | — |",
    "| `KeyGesture` | KeyGesture → InputGesture | DONE | Key, Modifiers | — |",
    "| `MouseGesture` | MouseGesture → InputGesture | DONE | MouseAction, Modifiers | — |",
    "| `InputBinding` | InputBinding → Freezable, ICommandSource | DONE | Command, CommandParameter, CommandTarget | — |",
    "| `KeyBinding` | KeyBinding → InputBinding | DONE | Gesture | — |",
    "| `MouseBinding` | MouseBinding → InputBinding | DONE | Gesture | — |",
    "| `Visual3D` | Visual3D → DependencyObject | DONE | — | — |",
    "| `Viewport3D` | Viewport3D → FrameworkElement | DONE | Camera, Children | — |",
    "| `Model3D` | Model3D → Animatable | DONE | — | — |",
    "| `Geometry3D` | Geometry3D → Animatable | DONE | — | — |",
    "| `Binding` | Binding → BindingBase | DONE | Path, Mode, Converter, Source, FallbackValue | — |",
    "| `MultiBinding` | MultiBinding → BindingBase | DONE | Bindings, Mode, Converter | — |",
    "| `PriorityBinding` | PriorityBinding → BindingBase | DONE | Bindings | — |",
    "| `BindingExpression` | BindingExpression → BindingExpressionBase | DONE | — | — |",
    "| `CollectionView` | CollectionView → DispatcherObject, ICollectionView, INotifyPropertyChanged | DONE | — | — |",
]

out += "\n## 11n anchor inventory (DONE — first-class DAG rows, re-applied verbatim)\n\n"
out += "> **11n-7 DAG CLOSE.** These anchor rows are the full 11n delta consolidated into the AGENDA table"
out += " (the DAG-anchor inventory). Each row flips from the 11n-0 DAG-EXT cluster note to a REAL DONE row"
out += " as its 11n body step shipped: 11n-1 (AutomationProperties), 11n-2 (ICommand spine: ICommandSource/"
out += " Freezable/RoutedCommand/CommandManager/InputGesture/InputBinding), 11n-3 (FrameworkTemplate —"
out += " and ItemsPanelTemplate, which is ALREADY a DONE generated row re-based onto FrameworkTemplate, so it"
out += " is intentionally NOT duplicated here), 11n-4 (reactive spine: BindingBase/BindingExpressionBase/"
out += " MarkupExtension/IWeakEventListener/ICollectionView/INotifyPropertyChanged/INotifyCollectionChanged/"
out += " + the Binding/MultiBinding/PriorityBinding/BindingExpression + CollectionView additions), 11n-5 (3D:"
out += " Visual3D/Viewport3D/Model3D/Geometry3D).\n\n"
out += "| Type | Inheritance (DAG) | Vaml tri-state | Own DPs | Routed events |\n"
out += "|------|-------------------|----------------|---------|---------------|\n"
out += "\n".join(ANCHOR_ROWS) + "\n"
out += "\n> **11n-7 continuation:** the 3D anchors (Visual3D/Viewport3D/Model3D/Geometry3D) are the 11n-5"
out += " 3D surface (DONE); the Adorner/VisualTreeHelper anchors are the 11n-6 surface (DONE). MultiBinding/"
out += " PriorityBinding live multi-source evaluation beyond the mechanism binding is recorded forward"
out += " (not silent); AdornerHitTest + adorner input routing is the named forward refinement. No silent gaps.\n"

write(DAG, out)
print("11n-7 DAG CLOSE complete;", len(MANUAL_BLOCKS), "manual note-blocks +", len(ANCHOR_ROWS), "anchor DONE rows re-applied.")
