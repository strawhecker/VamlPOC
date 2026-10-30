# DAG_controls.md — VAML ↔ WPF control/type inheritance + DP/event surface (engine contract)

> **11k-0a (generated).** This is the **engine contract** — the canonical inventory of every WPF
> control/type the corpus consumes or could consume, with its full ancestor chain and property/event
> surface. Generated from the REAL WPF source at `E:\GitRepos\wpf` by `gap_dp_daggen.py`.
> Do NOT hand-edit the generated rows; re-run the generator when WPF advances (or add rows here under
> the documented **closure-under-ancestry invariant**).
>
> **Closure-under-ancestry invariant:** if a listed control inherits a listed control, **both** nodes
> exist and carry their properties. A control's full effective surface = its own DPs + every ancestor's.
> (This is the invariant that catches `TextBlock`-is-not-a-`Control` class bugs.)
>
> **12k-5 (manual add, trigger family, under the closure-under-ancestry invariant).** The trigger/action
> family rows (`TriggerBase`, `Trigger`, `DataTrigger`, `EventTrigger`, `SetterBase`, `Setter`,
> `TriggerAction`, `BeginStoryboard`, `Storyboard`) were added here manually under the documented
> manual-add escape hatch (the `gap_dp_daggen.py` generator output did not include them; they are the
> `Style/Triggers` authoring family, parent nodes of the already-listed `SoundPlayerAction → TriggerAction`
> — whose dangling parent edge this closes). Follow the DAG contract: row states are flipped PLANNED→DONE at
> their owning sprints (EventTrigger@11e/11f/11g + S559's full re-apply), never silently.
>
> **11f DAG-EXT (manual add, behaviors + DataTemplate family, under the closure-under-ancestry invariant).**
> `gap_dp_daggen.py` scans only `PresentationFramework\System\Windows\Controls`, `Controls\Primitives`, and
> `Shapes` — it **cannot** emit the Microsoft.Xaml.Behaviors type-family (a separate assembly) or the
> `DataTemplate` parent node (a `System.Windows` FrameworkTemplate type outside the scan). Per D1/D3
> (roadmap Sprint 11f), the following rows were added manually under the same escape hatch:
> - **`DataTemplate → FrameworkTemplate`** — closes the dangling parent edge of the already-listed
>   `ItemContainerTemplate → DataTemplate` and `DataTemplateSelector` (S500 owns it).
> - **Microsoft.Xaml.Behaviors family** (`Behavior → DependencyObject`, `Behavior<T> → Behavior`,
>   `BehaviorCollection`, interactivity `TriggerAction → DependencyObject`, `InvokeCommandAction →
>   TriggerAction`, interactivity `EventTrigger → TriggerBase`) — the `i:`/`behaviors:`/`dd:` interaction
>   surface S502/S503a owns (S502 authors the types + behavior path; S503a flips the rows DONE).
> **EventTrigger DISAMBIGUATION (D3):** two DISTINCT EventTrigger classes exist.
> - `System.Windows EventTrigger` (RoutedEvent/SourceName — the `Style.Triggers` family) → stays `PLANNED`
>   (delivered at S559's full re-apply). NOT flipped by 11f.
> - `Microsoft.Xaml.Behaviors EventTrigger` (EventName/SourceObject/Actions) → flipped `DONE` at S503a.
> The S11k-5 EventTrigger container surface was authored at 11e (S495a); its `TriggerAction` payload
> distributes 11e/11f/11g — 11f owns the behaviors action family.
>
> **Tri-state:** every WPF type is in EXACTLY ONE state — `DONE` (Vaml models it + its surface),
> `PLANNED` (declared, scheduled), or `OUT-OF-SCOPE(<resolution-sprint>)` (declared, NOT silent).
> *Nothing is hidden*: unmodeled controls are declared `OUT-OF-SCOPE`, never omitted.

| Type | Inheritance (DAG) | Vaml tri-state | Own DPs | Routed events |
|------|-------------------|----------------|---------|---------------|
| `AVElementHelper` | AVElementHelper | PLANNED | — | — |
| `AccessText` | AccessText → FrameworkElement, | PLANNED | Text | — |
| `AddingNewItemEventArgs` | AddingNewItemEventArgs → EventArgs | PLANNED | — | — |
| `AdornedElementPlaceholder` | AdornedElementPlaceholder → FrameworkElement, | PLANNED | — | — |
| `AlternationConverter` | AlternationConverter → IValueConverter | PLANNED | — | — |
| `AutoToolTipPlacement` | AutoToolTipPlacement | PLANNED | — | — |
| `BooleanToSelectiveScrollingOrientationConverter` | BooleanToSelectiveScrollingOrientationConverter | PLANNED | — | — |
| `BooleanToVisibilityConverter` | BooleanToVisibilityConverter → IValueConverter | PLANNED | — | — |
| `Border` | Border → Decorator → FrameworkElement, | DONE | BorderBrush, BorderThickness, CornerRadius, Padding | — |
| `BorderGapMaskConverter` | BorderGapMaskConverter → IMultiValueConverter | PLANNED | — | — |
| `BulletDecorator` | BulletDecorator → Decorator → FrameworkElement, | PLANNED | — | — |
| `BeginStoryboard` | BeginStoryboard → TriggerAction | PLANNED | Storyboard | — |
| `Behavior` | Behavior → DependencyObject | PLANNED | AssociatedObject (attached), AssociatedType, IsEnabled | — |
| `Behavior<T>` | Behavior<T> → Behavior → DependencyObject | PLANNED | — | — |
| `BehaviorCollection` | BehaviorCollection → IList<Behavior> | PLANNED | — | — |
| `Button` | Button → ButtonBase → ContentControl, | DONE | IsCancel, IsDefault | — |
| `ButtonBase` | ButtonBase → ContentControl, | PLANNED | ClickMode, Command, CommandParameter, CommandTarget | Click |
| `Calendar` | Calendar → Control → FrameworkElement | PLANNED | CalendarButtonStyle, CalendarDayButtonStyle, CalendarItemStyle, DisplayDate, DisplayDateEnd, DisplayDateStart, DisplayMode, FirstDayOfWeek, IsTodayHighlighted, SelectedDate, SelectionMode | SelectedDatesChanged |
| `CalendarBlackoutDatesCollection` | CalendarBlackoutDatesCollection → ObservableCollection<CalendarDateRange> | PLANNED | — | — |
| `CalendarButton` | CalendarButton → Button → ButtonBase → ContentControl, | PLANNED | — | — |
| `CalendarDateChangedEventArgs` | CalendarDateChangedEventArgs → System.Windows.RoutedEventArgs | PLANNED | — | — |
| `CalendarDateRange` | CalendarDateRange → INotifyPropertyChanged | PLANNED | — | — |
| `CalendarDateRangeChangingEventArgs` | CalendarDateRangeChangingEventArgs | PLANNED | — | — |
| `CalendarDayButton` | CalendarDayButton → Button → ButtonBase → ContentControl, | PLANNED | — | — |
| `CalendarItem` | CalendarItem | PLANNED | — | — |
| `CalendarKeyboardHelper` | CalendarKeyboardHelper | PLANNED | — | — |
| `CalendarMode` | CalendarMode | PLANNED | — | — |
| `CalendarModeChangedEventArgs` | CalendarModeChangedEventArgs → System.Windows.RoutedEventArgs | PLANNED | — | — |
| `CalendarSelectionChangedEventArgs` | CalendarSelectionChangedEventArgs | PLANNED | — | — |
| `CalendarSelectionMode` | CalendarSelectionMode | PLANNED | — | — |
| `CalendarVisualStates` | CalendarVisualStates | PLANNED | — | — |
| `Canvas` | Canvas → Panel → FrameworkElement, | DONE | Bottom, Left, Right, Top | — |
| `CharacterCasing` | CharacterCasing | PLANNED | — | — |
| `CheckBox` | CheckBox → ToggleButton → ButtonBase → ContentControl, | DONE | — | — |
| `CleanUpVirtualizedItemEventArgs` | CleanUpVirtualizedItemEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ClickMode` | ClickMode | PLANNED | — | — |
| `ColumnDefinition` | ColumnDefinition → IList<ColumnDefinition> | PLANNED | MaxWidth, MinWidth, Width | — |
| `ColumnDefinitionCollectionConverter` | ColumnDefinitionCollectionConverter | PLANNED | — | — |
| `ComboBox` | ComboBox → Selector → ItemsControl → Control, | DONE | IsDropDownOpen, IsEditable, MaxDropDownHeight, ShouldPreserveUserEnteredPrefix, StaysOpenOnEdit, Text | — |
| `ComboBoxItem` | ComboBoxItem → ListBoxItem → ContentControl → Control, | PLANNED | — | — |
| `ContainerTracking` | ContainerTracking | PLANNED | — | — |
| `ContentControl` | ContentControl → Control, | PLANNED | Content, ContentStringFormat, ContentTemplate, ContentTemplateSelector | — |
| `ContentPresenter` | ContentPresenter → FrameworkElement | PLANNED | ContentSource, ContentStringFormat, RecognizesAccessKey | — |
| `ContextMenu` | ContextMenu → MenuBase → ItemsControl → Control, | PLANNED | — | — |
| `ContextMenuService` | ContextMenuService → RoutedEventArgs | PLANNED | ContextMenu, HasDropShadow, HorizontalOffset, IsEnabled, Placement, PlacementRectangle, PlacementTarget, ShowOnDisabled, VerticalOffset | ContextMenuClosing, ContextMenuOpening |
| `Control` | Control → FrameworkElement | PLANNED | HorizontalContentAlignment, Padding, Template, VerticalContentAlignment | MouseDoubleClick, PreviewMouseDoubleClick |
| `ConversionValidationRule` | ConversionValidationRule | PLANNED | — | — |
| `CustomDictionarySources` | CustomDictionarySources | PLANNED | — | — |
| `CustomPopupPlacement` | CustomPopupPlacement | PLANNED | — | — |
| `CustomPopupPlacementCallback` | CustomPopupPlacementCallback | PLANNED | — | — |
| `DataErrorValidationRule` | DataErrorValidationRule → ValidationRule | PLANNED | — | — |
| `DataGrid` | DataGrid → MultiSelector → Selector → ItemsControl → Control, | PLANNED | AlternatingRowBackground, AreRowDetailsFrozen, AutoGenerateColumns, CanUserAddRows, CanUserDeleteRows, CanUserReorderColumns, CanUserResizeColumns, CanUserResizeRows, CanUserSortColumns, CellStyle, ClipboardCopyMode, ColumnHeaderHeight, ColumnHeaderStyle, ColumnWidth, CurrentCell, CurrentColumn, CurrentItem, DragIndicatorStyle, DropLocationIndicatorStyle, EnableColumnVirtualization, EnableRowVirtualization, FrozenColumnCount, GridLinesVisibility, HeadersVisibility, HorizontalGridLineThickness, HorizontalGridLinesBrush, IsReadOnly, MaxColumnWidth, MinColumnWidth, MinRowHeight, RowBackground, RowDetailsTemplate, RowDetailsTemplateSelector, RowDetailsVisibilityMode, RowHeaderStyle, RowHeaderTemplate, RowHeaderTemplateSelector, RowHeaderWidth, RowHeight, RowStyle, RowStyleSelector, RowValidationErrorTemplate, SelectionMode, SelectionUnit, VerticalGridLineThickness, VerticalGridLinesBrush | — |
| `DataGridAutoGeneratingColumnEventArgs` | DataGridAutoGeneratingColumnEventArgs → EventArgs | PLANNED | — | — |
| `DataGridBeginningEditEventArgs` | DataGridBeginningEditEventArgs → EventArgs | PLANNED | — | — |
| `DataGridBoundColumn` | DataGridBoundColumn → DataGridColumn → DependencyObject | PLANNED | EditingElementStyle, ElementStyle | — |
| `DataGridCell` | DataGridCell → ContentControl, | PLANNED | IsEditing, IsSelected | Selected, Unselected |
| `DataGridCellClipboardEventArgs` | DataGridCellClipboardEventArgs → EventArgs | PLANNED | — | — |
| `DataGridCellEditEndingEventArgs` | DataGridCellEditEndingEventArgs → EventArgs | PLANNED | — | — |
| `DataGridCellInfo` | DataGridCellInfo | PLANNED | — | — |
| `DataGridCellsPanel` | DataGridCellsPanel → VirtualizingPanel → Panel → FrameworkElement, | PLANNED | — | — |
| `DataGridCellsPresenter` | DataGridCellsPresenter → ItemsControl → Control, | PLANNED | — | — |
| `DataGridCheckBoxColumn` | DataGridCheckBoxColumn → DataGridBoundColumn → DataGridColumn → DependencyObject | PLANNED | — | — |
| `DataGridClipboardCellContent` | DataGridClipboardCellContent | PLANNED | — | — |
| `DataGridClipboardCopyMode` | DataGridClipboardCopyMode | PLANNED | — | — |
| `DataGridClipboardHelper` | DataGridClipboardHelper | PLANNED | — | — |
| `DataGridColumn` | DataGridColumn → DependencyObject | PLANNED | CanUserReorder, CanUserResize, CanUserSort, CellStyle, DisplayIndex, DragIndicatorStyle, Header, HeaderStringFormat, HeaderStyle, HeaderTemplate, HeaderTemplateSelector, IsReadOnly, MaxWidth, MinWidth, SortDirection, SortMemberPath, Visibility, Width | — |
| `DataGridColumnCollection` | DataGridColumnCollection | PLANNED | — | — |
| `DataGridColumnDropSeparator` | DataGridColumnDropSeparator | PLANNED | — | — |
| `DataGridColumnEventArgs` | DataGridColumnEventArgs → EventArgs | PLANNED | — | — |
| `DataGridColumnFloatingHeader` | DataGridColumnFloatingHeader | PLANNED | — | — |
| `DataGridColumnHeader` | DataGridColumnHeader → ButtonBase, | PLANNED | SeparatorBrush, SeparatorVisibility | — |
| `DataGridColumnHeaderCollection` | DataGridColumnHeaderCollection | PLANNED | — | — |
| `DataGridColumnHeadersPresenter` | DataGridColumnHeadersPresenter → ItemsControl → Control, | PLANNED | — | — |
| `DataGridColumnReorderingEventArgs` | DataGridColumnReorderingEventArgs → DataGridColumnEventArgs → EventArgs | PLANNED | — | — |
| `DataGridComboBoxColumn` | DataGridComboBoxColumn → DataGridColumn → DependencyObject | PLANNED | — | — |
| `DataGridDetailsPresenter` | DataGridDetailsPresenter → ContentPresenter → FrameworkElement | PLANNED | — | — |
| `DataGridEditAction` | DataGridEditAction | PLANNED | — | — |
| `DataGridEditingUnit` | DataGridEditingUnit | PLANNED | — | — |
| `DataGridGridLinesVisibility` | DataGridGridLinesVisibility | PLANNED | — | — |
| `DataGridHeadersVisibility` | DataGridHeadersVisibility | PLANNED | — | — |
| `DataGridHeadersVisibilityToVisibilityConverter` | DataGridHeadersVisibilityToVisibilityConverter | PLANNED | — | — |
| `DataGridHelper` | DataGridHelper | PLANNED | — | — |
| `DataGridHyperlinkColumn` | DataGridHyperlinkColumn → DataGridBoundColumn → DataGridColumn → DependencyObject | PLANNED | — | — |
| `DataGridItemAttachedStorage` | DataGridItemAttachedStorage | PLANNED | — | — |
| `DataGridLength` | DataGridLength | PLANNED | — | — |
| `DataGridLengthConverter` | DataGridLengthConverter → TypeConverter | PLANNED | — | — |
| `DataGridLengthUnitType` | DataGridLengthUnitType | PLANNED | — | — |
| `DataGridNotificationTarget` | DataGridNotificationTarget | PLANNED | — | — |
| `DataGridPreparingCellForEditEventArgs` | DataGridPreparingCellForEditEventArgs → EventArgs | PLANNED | — | — |
| `DataGridRow` | DataGridRow → Control → FrameworkElement | PLANNED | DetailsTemplate, DetailsTemplateSelector, DetailsVisibility, Header, HeaderStyle, HeaderTemplate, HeaderTemplateSelector, Item, ValidationErrorTemplate | — |
| `DataGridRowClipboardEventArgs` | DataGridRowClipboardEventArgs → EventArgs | PLANNED | — | — |
| `DataGridRowDetailsEventArgs` | DataGridRowDetailsEventArgs → EventArgs | PLANNED | — | — |
| `DataGridRowDetailsVisibilityMode` | DataGridRowDetailsVisibilityMode | PLANNED | — | — |
| `DataGridRowEventArgs` | DataGridRowEventArgs → EventArgs | PLANNED | — | — |
| `DataGridRowHeader` | DataGridRowHeader → ButtonBase → ContentControl, | PLANNED | SeparatorBrush, SeparatorVisibility | — |
| `DataGridRowsPresenter` | DataGridRowsPresenter → VirtualizingStackPanel → VirtualizingPanel, | PLANNED | — | — |
| `DataGridSelectionMode` | DataGridSelectionMode | PLANNED | — | — |
| `DataGridSelectionUnit` | DataGridSelectionUnit | PLANNED | — | — |
| `DataGridSortingEventArgs` | DataGridSortingEventArgs → DataGridColumnEventArgs → EventArgs | PLANNED | — | — |
| `DataGridSortingEventHandler` | DataGridSortingEventHandler | PLANNED | — | — |
| `DataGridTemplateColumn` | DataGridTemplateColumn → DataGridColumn → DependencyObject | PLANNED | CellEditingTemplate, CellEditingTemplateSelector, CellTemplate, CellTemplateSelector | — |
| `DataGridTextColumn` | DataGridTextColumn → DataGridBoundColumn → DataGridColumn → DependencyObject | PLANNED | — | — |
| `DataGridrowEditEndingEventArgs` | DataGridrowEditEndingEventArgs → EventArgs | PLANNED | — | — |
| `DataTemplate` | DataTemplate → FrameworkTemplate | PLANNED | DataType | — |
| `DataTemplateSelector` | DataTemplateSelector | PLANNED | — | — |
| `DataTrigger` | DataTrigger → TriggerBase | PLANNED | Binding, Value | — |
| `DatePicker` | DatePicker → Control → FrameworkElement | PLANNED | CalendarStyle, DisplayDate, DisplayDateEnd, DisplayDateStart, FirstDayOfWeek, IsDropDownOpen, IsTodayHighlighted, SelectedDate, SelectedDateFormat, Text | SelectedDateChanged |
| `DatePickerDateValidationErrorEventArgs` | DatePickerDateValidationErrorEventArgs → EventArgs | PLANNED | — | — |
| `DatePickerFormat` | DatePickerFormat | PLANNED | — | — |
| `DatePickerTextBox` | DatePickerTextBox | PLANNED | — | — |
| `DateTimeHelper` | DateTimeHelper | PLANNED | — | — |
| `Decorator` | Decorator → FrameworkElement, | PLANNED | — | — |
| `DeferredRunTextReference` | DeferredRunTextReference | PLANNED | — | — |
| `DeferredSelectedIndexReference` | DeferredSelectedIndexReference | PLANNED | — | — |
| `DeferredTextReference` | DeferredTextReference | PLANNED | — | — |
| `DefinitionBase` | DefinitionBase → FrameworkContentElement | PLANNED | SharedSizeGroup | — |
| `DockPanel` | DockPanel → Panel → FrameworkElement, | DONE | Dock, LastChildFill | — |
| `DocumentPageView` | DocumentPageView → FrameworkElement, | PLANNED | PageNumber | — |
| `DocumentViewer` | DocumentViewer → DocumentViewerBase → Control, | PLANNED | HorizontalOffset, HorizontalPageSpacing, MaxPagesAcross, ShowPageBorders, VerticalOffset, VerticalPageSpacing, Zoom | — |
| `DocumentViewerBase` | DocumentViewerBase → Control, | PLANNED | Document, IsMasterPage | — |
| `DragCompletedEventArgs` | DragCompletedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `DragDeltaEventArgs` | DragDeltaEventArgs → RoutedEventArgs | PLANNED | — | — |
| `DragStartedEventArgs` | DragStartedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `Ellipse` | Ellipse → Shape → FrameworkElement | DONE | — | — |
| `EventTrigger` | **System.Windows** EventTrigger → TriggerBase (the `Style.Triggers` family; NOT the Microsoft.Xaml.Behaviors one) | PLANNED | RoutedEvent, SourceName | — |
| `EventTrigger` (MS.Xaml.Behaviors) | Microsoft.Xaml.Behaviors `EventTrigger` → TriggerBase (interactivity family; the `i:` surface) | PLANNED | EventName, SourceObject, Actions | — |
| `ExceptionRoutedEventArgs` | ExceptionRoutedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ExceptionValidationRule` | ExceptionValidationRule → ValidationRule | PLANNED | — | — |
| `Expander` | Expander → HeaderedContentControl → ContentControl → Control, | DONE | ExpandDirection, IsExpanded | Collapsed, Expanded |
| `FlowDocumentReader` | FlowDocumentReader → Control, | PLANNED | Document, IsFindEnabled, IsPageViewEnabled, IsPrintEnabled, IsScrollViewEnabled, IsTwoPageViewEnabled, ViewingMode | — |
| `FlowDocumentScrollViewer` | FlowDocumentScrollViewer → Control, | PLANNED | Document, IsSelectionEnabled, IsToolBarVisible | — |
| `Frame` | Frame → ContentControl, | PLANNED | JournalOwnership, NavigationUIVisibility, SandboxExternalContent, Source | — |
| `Grid` | Grid → Panel, | DONE | Column, ColumnSpan, IsSharedSizeScope, Row, RowSpan, ShowGridLines | — |
| `GridSplitter` | GridSplitter → Thumb → Control → FrameworkElement | PLANNED | DragIncrement, KeyboardIncrement, PreviewStyle, ResizeBehavior, ResizeDirection, ShowsPreview | — |
| `GridView` | GridView → ViewBase, | PLANNED | AllowsColumnReorder, ColumnCollection, ColumnHeaderContainerStyle, ColumnHeaderContextMenu, ColumnHeaderStringFormat, ColumnHeaderTemplate, ColumnHeaderTemplateSelector, ColumnHeaderToolTip | — |
| `GridViewColumn` | GridViewColumn → DependencyObject, | PLANNED | CellTemplate, CellTemplateSelector, Header, HeaderContainerStyle, HeaderStringFormat, HeaderTemplate, HeaderTemplateSelector | — |
| `GridViewColumnCollection` | GridViewColumnCollection → ObservableCollection<GridViewColumn> | PLANNED | — | — |
| `GridViewColumnCollectionChangedEventArgs` | GridViewColumnCollectionChangedEventArgs | PLANNED | — | — |
| `GridViewColumnHeader` | GridViewColumnHeader → ButtonBase → ContentControl, | PLANNED | — | — |
| `GridViewHeaderRowPresenter` | GridViewHeaderRowPresenter → GridViewRowPresenterBase → FrameworkElement, | PLANNED | — | — |
| `GridViewRowPresenter` | GridViewRowPresenter → GridViewRowPresenterBase → FrameworkElement, | PLANNED | — | — |
| `GridViewRowPresenterBase` | GridViewRowPresenterBase → FrameworkElement, | PLANNED | Columns | — |
| `GroupBox` | GroupBox → HeaderedContentControl → ContentControl → Control, | DONE | — | — |
| `GroupItem` | GroupItem → ContentControl, | PLANNED | — | — |
| `GroupStyle` | GroupStyle → INotifyPropertyChanged | PLANNED | — | — |
| `HeaderedContentControl` | HeaderedContentControl → ContentControl → Control, | PLANNED | Header, HeaderStringFormat, HeaderTemplate, HeaderTemplateSelector | — |
| `HeaderedItemsControl` | HeaderedItemsControl → ItemsControl → Control, | PLANNED | HeaderStringFormat | — |
| `HierarchicalVirtualizationConstraints` | HierarchicalVirtualizationConstraints | PLANNED | — | — |
| `HierarchicalVirtualizationHeaderDesiredSizes` | HierarchicalVirtualizationHeaderDesiredSizes | PLANNED | — | — |
| `HierarchicalVirtualizationItemDesiredSizes` | HierarchicalVirtualizationItemDesiredSizes | PLANNED | — | — |
| `IContainItemStorage` | IContainItemStorage | PLANNED | — | — |
| `IHierarchicalVirtualizationAndScrollInfo` | IHierarchicalVirtualizationAndScrollInfo | PLANNED | — | — |
| `IItemContainerGenerator` | IItemContainerGenerator | PLANNED | — | — |
| `IProvideDataGridColumn` | IProvideDataGridColumn | PLANNED | — | — |
| `IRecyclingItemContainerGenerator` | IRecyclingItemContainerGenerator | PLANNED | — | — |
| `IScrollInfo` | IScrollInfo | PLANNED | — | — |
| `Image` | Image → FrameworkElement, | DONE | Source | ImageFailed |
| `InitializingNewItemEventArgs` | InitializingNewItemEventArgs → EventArgs | PLANNED | — | — |
| `InitializingNewItemEventHandler` | InitializingNewItemEventHandler | PLANNED | — | — |
| `InkCanvas` | InkCanvas → FrameworkElement, | PLANNED | Bottom, DefaultDrawingAttributes, EditingMode, EditingModeInverted, Left, Right, Top | ActiveEditingModeChanged, EditingModeChanged, EditingModeInvertedChanged, Gesture, StrokeCollected, StrokeErased |
| `InkCanvasClipboardFormat` | InkCanvasClipboardFormat | PLANNED | — | — |
| `InkCanvasSelectionHitResult` | InkCanvasSelectionHitResult | PLANNED | — | — |
| `InkPresenter` | InkPresenter → Decorator → FrameworkElement, | PLANNED | Strokes | — |
| `ItemCollection` | ItemCollection → CollectionView, | PLANNED | — | — |
| `ItemContainerGenerator` | ItemContainerGenerator → IRecyclingItemContainerGenerator, | PLANNED | — | — |
| `ItemContainerTemplate` | ItemContainerTemplate → DataTemplate | PLANNED | — | — |
| `ItemContainerTemplateKey` | ItemContainerTemplateKey → TemplateKey | PLANNED | — | — |
| `ItemContainerTemplateSelector` | ItemContainerTemplateSelector | PLANNED | — | — |
| `ItemsChangedEventArgs` | ItemsChangedEventArgs → EventArgs | PLANNED | — | — |
| `ItemsControl` | ItemsControl → Control, | PLANNED | AlternationCount, DisplayMemberPath, GroupStyleSelector, IsTextSearchCaseSensitive, IsTextSearchEnabled, ItemBindingGroup, ItemContainerStyle, ItemContainerStyleSelector, ItemStringFormat, ItemTemplate, ItemTemplateSelector, ItemsPanel, ItemsSource | — |
| `ItemsPanelTemplate` | ItemsPanelTemplate → FrameworkTemplate | PLANNED | — | — |
| `ItemsPresenter` | ItemsPresenter → FrameworkElement | PLANNED | — | — |
| `Label` | Label → ContentControl → Control, | DONE | Target | — |
| `LayoutInformation` | LayoutInformation | PLANNED | — | — |
| `Line` | Line → Shape → FrameworkElement | DONE | X1, X2, Y1, Y2 | — |
| `ListBox` | ListBox → Selector → ItemsControl → Control, | DONE | SelectionMode | — |
| `ListBoxItem` | ListBoxItem → ContentControl → Control, | PLANNED | — | — |
| `ListView` | ListView → ListBox → Selector → ItemsControl → Control, | PLANNED | View | — |
| `ListViewItem` | ListViewItem → ListBoxItem → ContentControl → Control, | PLANNED | — | — |
| `MediaElement` | MediaElement → FrameworkElement, | PLANNED | Balance, IsMuted, LoadedBehavior, ScrubbingEnabled, Source, UnloadedBehavior, Volume | BufferingEnded, BufferingStarted, MediaEnded, MediaFailed, MediaOpened, ScriptCommand |
| `MediaScriptCommandRoutedEventArgs` | MediaScriptCommandRoutedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `Menu` | Menu → MenuBase → ItemsControl → Control, | PLANNED | IsMainMenu | — |
| `MenuBase` | MenuBase → ItemsControl → Control, | PLANNED | ItemContainerTemplateSelector, UsesItemContainerTemplate | — |
| `MenuItem` | MenuItem → HeaderedItemsControl, | PLANNED | Icon, InputGestureText, IsCheckable, IsChecked, IsSubmenuOpen, StaysOpenOnClick | Checked, Click, SubmenuClosed, SubmenuOpened, Unchecked |
| `MenuScrollingVisibilityConverter` | MenuScrollingVisibilityConverter → IMultiValueConverter | PLANNED | — | — |
| `MultiSelector` | MultiSelector → Selector → ItemsControl → Control, | PLANNED | — | — |
| `MultipleCopiesCollection` | MultipleCopiesCollection | PLANNED | — | — |
| `NotifyDataErrorValidationRule` | NotifyDataErrorValidationRule → ValidationRule | PLANNED | — | — |
| `Orientation` | Orientation | PLANNED | — | — |
| `Page` | Page → FrameworkElement, | PLANNED | Title | — |
| `PageRanges` | PageRanges | PLANNED | — | — |
| `Panel` | Panel → FrameworkElement, | PLANNED | Background, IsItemsHost, ZIndex | — |
| `PanningMode` | PanningMode | PLANNED | — | — |
| `PasswordBox` | PasswordBox → Control, | DONE | PasswordChar | PasswordChanged |
| `PasswordTextContainer` | PasswordTextContainer | PLANNED | — | — |
| `PasswordTextNavigator` | PasswordTextNavigator | PLANNED | — | — |
| `Path` | Path → Shape → FrameworkElement | PLANNED | Data | — |
| `PlacementMode` | PlacementMode | PLANNED | — | — |
| `Polygon` | Polygon → Shape → FrameworkElement | PLANNED | FillRule, Points | — |
| `Polyline` | Polyline → Shape → FrameworkElement | PLANNED | FillRule, Points | — |
| `Popup` | Popup → FrameworkElement, | PLANNED | Child, CustomPopupPlacementCallback, HorizontalOffset, IsOpen, Placement, PlacementRectangle, PlacementTarget, PopupAnimation, StaysOpen, VerticalOffset | — |
| `PopupAnimation` | PopupAnimation | PLANNED | — | — |
| `PopupControlService` | PopupControlService | PLANNED | — | — |
| `PopupPrimaryAxis` | PopupPrimaryAxis | PLANNED | — | — |
| `PopupRoot` | PopupRoot | PLANNED | — | — |
| `PrintDialog` | PrintDialog | PLANNED | — | — |
| `PrintDialogException` | PrintDialogException → Exception | PLANNED | — | — |
| `ProgressBar` | ProgressBar → RangeBase → Control → FrameworkElement | DONE | IsIndeterminate, Orientation | — |
| `RadioButton` | RadioButton → ToggleButton → ButtonBase → ContentControl, | DONE | GroupName | — |
| `RangeBase` | RangeBase → Control → FrameworkElement | PLANNED | LargeChange, Maximum, Minimum, SmallChange, Value | ValueChanged |
| `RealizedColumnsBlock` | RealizedColumnsBlock | PLANNED | — | — |
| `Rectangle` | Rectangle → Shape → FrameworkElement | DONE | RadiusX, RadiusY | — |
| `RelativeHeaderPosition` | RelativeHeaderPosition | PLANNED | — | — |
| `RepeatButton` | RepeatButton → ButtonBase → ContentControl, | DONE | Delay, Interval | — |
| `ResizeGrip` | ResizeGrip → Control → FrameworkElement | PLANNED | — | — |
| `RichTextBox` | RichTextBox → TextBoxBase, | PLANNED | IsDocumentEnabled | — |
| `RowDefinition` | RowDefinition → IList<RowDefinition> | PLANNED | Height, MaxHeight, MinHeight | — |
| `RowDefinitionCollectionConverter` | RowDefinitionCollectionConverter | PLANNED | — | — |
| `ScrollBar` | ScrollBar → RangeBase → Control → FrameworkElement | DONE | Orientation, ViewportSize | Scroll |
| `ScrollChangedEventArgs` | ScrollChangedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ScrollContentPresenter` | ScrollContentPresenter → ContentPresenter, | PLANNED | — | — |
| `ScrollEventArgs` | ScrollEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ScrollUnit` | ScrollUnit | PLANNED | — | — |
| `ScrollViewer` | ScrollViewer → ContentControl → Control, | DONE | CanContentScroll, HorizontalScrollBarVisibility, IsDeferredScrollingEnabled, PanningDeceleration, PanningMode, PanningRatio, VerticalScrollBarVisibility | ScrollChanged |
| `SelectedCellsChangedEventArgs` | SelectedCellsChangedEventArgs → EventArgs | PLANNED | — | — |
| `SelectedCellsChangedEventHandler` | SelectedCellsChangedEventHandler | PLANNED | — | — |
| `SelectedCellsCollection` | SelectedCellsCollection | PLANNED | — | — |
| `SelectedDatesCollection` | SelectedDatesCollection → ObservableCollection<DateTime> | PLANNED | — | — |
| `SelectedItemCollection` | SelectedItemCollection | PLANNED | — | — |
| `SelectionChangedEventArgs` | SelectionChangedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `SelectiveScrollingGrid` | SelectiveScrollingGrid → Grid → Panel, | PLANNED | SelectiveScrollingOrientation | — |
| `SelectiveScrollingOrientation` | SelectiveScrollingOrientation | PLANNED | — | — |
| `Selector` | Selector → ItemsControl → Control, | PLANNED | IsSelected, IsSynchronizedWithCurrentItem, SelectedIndex, SelectedItem, SelectedValue, SelectedValuePath | Selected, SelectionChanged, Unselected |
| `Separator` | Separator → Control → FrameworkElement | DONE | — | — |
| `Setter` | Setter → SetterBase | PLANNED | Property, TargetName, Value | — |
| `SetterBase` | SetterBase → DependencyObject | PLANNED | — | — |
| `Shape` | Shape → FrameworkElement | PLANNED | Fill, Stretch, Stroke, StrokeDashArray, StrokeDashCap, StrokeDashOffset, StrokeEndLineCap, StrokeLineJoin, StrokeMiterLimit, StrokeStartLineCap, StrokeThickness | — |
| `SinglePageViewer` | SinglePageViewer → DocumentViewerBase, | PLANNED | MaxZoom, MinZoom, Zoom, ZoomIncrement | — |
| `Slider` | Slider → RangeBase → Control → FrameworkElement | DONE | AutoToolTipPlacement, AutoToolTipPrecision, IsDirectionReversed, IsMoveToPointEnabled, IsSelectionRangeEnabled, IsSnapToTickEnabled, Orientation, SelectionEnd, SelectionStart, TickFrequency, TickPlacement, Ticks | — |
| `SoundPlayerAction` | SoundPlayerAction → TriggerAction, | PLANNED | Source | — |
| `SpellCheck` | SpellCheck | PLANNED | IsEnabled, SpellingReform | — |
| `SpellingReform` | SpellingReform | PLANNED | — | — |
| `Stack` | Stack → Panel, | PLANNED | Orientation | — |
| `StatusBar` | StatusBar → ItemsControl → Control, | DONE | — | — |
| `StatusBarItem` | StatusBarItem → ContentControl → Control, | PLANNED | — | — |
| `StickyNote` | StickyNote | PLANNED | CaptionFontFamily, CaptionFontSize, CaptionFontStretch, CaptionFontStyle, CaptionFontWeight, IsActive, IsExpanded, PenWidth | — |
| `Storyboard` | Storyboard → ParallelTimeline | PLANNED | — | — |
| `StyleSelector` | StyleSelector | PLANNED | — | — |
| `TabControl` | TabControl → Selector → ItemsControl → Control, | PLANNED | ContentStringFormat, ContentTemplate, ContentTemplateSelector, TabStripPlacement | — |
| `TabItem` | TabItem → HeaderedContentControl → ContentControl → Control, | PLANNED | — | — |
| `TabPanel` | TabPanel → Panel → FrameworkElement, | PLANNED | — | — |
| `TextAdaptor` | TextAdaptor | PLANNED | — | — |
| `TextBlock` | TextBlock → FrameworkElement, | DONE | BaselineOffset, Text, TextTrimming, TextWrapping | — |
| `TextBox` | TextBox → TextBoxBase, | DONE | CharacterCasing, MaxLength, MaxLines, MinLines, Text | — |
| `TextBoxBase` | TextBoxBase → Control → FrameworkElement | PLANNED | AcceptsTab, AutoWordSelection, CaretBrush, IsInactiveSelectionHighlightEnabled, IsReadOnlyCaretVisible, IsUndoEnabled, SelectionBrush, SelectionOpacity, SelectionTextBrush, UndoLimit | SelectionChanged, TextChanged |
| `TextChange` | TextChange | PLANNED | — | — |
| `TextChangedEventArgs` | TextChangedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `TextRangeAdaptor` | TextRangeAdaptor | PLANNED | — | — |
| `TextSearch` | TextSearch → DependencyObject | PLANNED | Text, TextPath | — |
| `Thumb` | Thumb → Control → FrameworkElement | PLANNED | — | DragCompleted, DragDelta, DragStarted |
| `TickBar` | TickBar → FrameworkElement | PLANNED | Fill, Placement, ReservedSpace | — |
| `TickPlacement` | TickPlacement | PLANNED | — | — |
| `ToggleButton` | ToggleButton → ButtonBase → ContentControl, | PLANNED | IsChecked, IsThreeState | Checked, Indeterminate, Unchecked |
| `ToolBar` | ToolBar → HeaderedItemsControl → ItemsControl → Control, | DONE | Band, BandIndex, IsOverflowOpen, OverflowMode | — |
| `ToolBarOverflowPanel` | ToolBarOverflowPanel → Panel → FrameworkElement, | PLANNED | WrapWidth | — |
| `ToolBarPanel` | ToolBarPanel → StackPanel | PLANNED | — | — |
| `ToolBarTray` | ToolBarTray → FrameworkElement, | DONE | IsLocked, Orientation | — |
| `ToolTip` | ToolTip → ContentControl → Control, | PLANNED | IsOpen | Closed, Opened |
| `ToolTipService` | ToolTipService → RoutedEventArgs | PLANNED | BetweenShowDelay, HasDropShadow, HorizontalOffset, InitialShowDelay, IsEnabled, Placement, PlacementRectangle, PlacementTarget, ShowDuration, ShowOnDisabled, ShowsToolTipOnKeyboardFocus, ToolTip, VerticalOffset | ToolTipClosing, ToolTipOpening |
| `Track` | Track → FrameworkElement | PLANNED | IsDirectionReversed, Orientation, ViewportSize | — |
| `Trigger` | Trigger → TriggerBase | PLANNED | Property, Value | — |
| `TriggerAction` (System.Windows) | System.Windows.TriggerAction → DependencyObject (Style.Triggers action family) | PLANNED | — | — |
| `TriggerAction` (MS.Xaml.Behaviors) | Microsoft.Xaml.Behaviors `TriggerAction` → DependencyObject (interactivity action family) | PLANNED | — | — |
| `InvokeCommandAction` | InvokeCommandAction → TriggerAction (MS.Xaml.Behaviors) | PLANNED | Command, CommandParameter, CommandTarget | — |
| `TriggerBase` (System.Windows) | System.Windows.TriggerBase → DependencyObject (Style.Triggers family) | PLANNED | — | — |
| `TreeView` | TreeView → ItemsControl → Control, | PLANNED | SelectedValuePath | SelectedItemChanged |
| `TreeViewItem` | TreeViewItem → HeaderedItemsControl, | PLANNED | IsExpanded, IsSelected | Collapsed, Expanded, Selected, Unselected |
| `UIElementCollection` | UIElementCollection → IList | PLANNED | — | — |
| `UniformGrid` | UniformGrid → Panel → FrameworkElement, | PLANNED | Columns, FirstColumn, Rows | — |
| `UserControl` | UserControl → ContentControl → Control, | DONE | — | — |
| `Validation` | Validation | PLANNED | ErrorTemplate, ValidationAdornerSite, ValidationAdornerSiteFor | Error |
| `ValidationError` | ValidationError | PLANNED | — | — |
| `ValidationErrorEventArgs` | ValidationErrorEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ValidationResult` | ValidationResult | PLANNED | — | — |
| `ValidationRule` | ValidationRule | PLANNED | — | — |
| `ValidationStep` | ValidationStep | PLANNED | — | — |
| `ViewBase` | ViewBase → DependencyObject | PLANNED | — | — |
| `ViewBox` | ViewBox → Decorator → FrameworkElement, | PLANNED | Stretch, StretchDirection | — |
| `ViewPort3D` | ViewPort3D → FrameworkElement, | PLANNED | — | — |
| `VirtualizationCacheLength` | VirtualizationCacheLength | PLANNED | — | — |
| `VirtualizationCacheLengthConverter` | VirtualizationCacheLengthConverter → TypeConverter | PLANNED | — | — |
| `VirtualizationMode` | VirtualizationMode | PLANNED | — | — |
| `VirtualizedCellInfoCollection` | VirtualizedCellInfoCollection | PLANNED | — | — |
| `VirtualizingPanel` | VirtualizingPanel → Panel → FrameworkElement, | PLANNED | CacheLength, CacheLengthUnit, IsContainerVirtualizable, IsVirtualizing, IsVirtualizingWhenGrouping, ScrollUnit, VirtualizationMode | — |
| `VirtualizingStackPanel` | VirtualizingStackPanel → VirtualizingPanel, | PLANNED | Orientation | CleanUpVirtualizedItem |
| `VisualStates` | VisualStates | PLANNED | — | — |
| `WebBrowser` | WebBrowser → ActiveXHost | PLANNED | — | — |
| `WrapPanel` | WrapPanel → Panel → FrameworkElement, | DONE | ItemHeight, ItemWidth | — |

## Notes / recorded NO-GAP deferrals

- **`Geometry` is `abstract`** in WPF (PresentationCore `Media\Geometry.cs`). A bare `<Geometry>` element is
  NOT authorable — WPF authors concrete derivations (`PathGeometry`, `GeometryGroup`, `EllipseGeometry`…).
  Vaml row: **OUT-OF-SCOPE(11k-4)** — resolved at 11k-4 under concrete derived geometry resource elements.
- **`TextBlock : FrameworkElement`** (NOT a `Control`). Vaml must re-base TextBlock off the framework-element
  branch, not the Control base (it must NOT inherit `Control`'s `Padding`/content-alignment DPs).
- **Abstract bases** (`ContentControl`, `ButtonBase`, `RangeBase`, `ItemsControl`, `Control`, `FrameworkElement`,
  `UIElement`, `TextElement`) are `PLANNED` — their DP surface is the base-branch fidelity sprint (11k-11 S649).
