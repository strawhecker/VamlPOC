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
> **Tri-state:** every WPF type is in EXACTLY ONE state — `DONE` (Vaml models it + its surface),
> `PLANNED` (declared, scheduled), or `OUT-OF-SCOPE(<resolution-sprint>)` (declared, NOT silent).
> *Nothing is hidden*: unmodeled controls are declared `OUT-OF-SCOPE`, never omitted.

| Type | Inheritance (DAG) | Vaml tri-state | Own DPs | Routed events |
|------|-------------------|----------------|---------|---------------|
| `AVElementHelper` | AVElementHelper | PLANNED | — | — |
| `AccessText` | AccessText → FrameworkElement, | PLANNED | Text | — |
| `AddingNewItemEventArgs` | AddingNewItemEventArgs → EventArgs | PLANNED | — | — |
| `AdornedElementPlaceholder` | AdornedElementPlaceholder → FrameworkElement, | PLANNED | — | — |
| `AlternationConverter` | AlternationConverter → IValueConverter | DONE | — | — |
| `AutoToolTipPlacement` | AutoToolTipPlacement | DONE | — | — |
| `BooleanToSelectiveScrollingOrientationConverter` | BooleanToSelectiveScrollingOrientationConverter | DONE | — | — |
| `BooleanToVisibilityConverter` | BooleanToVisibilityConverter → IValueConverter | PLANNED | — | — |
| `Border` | Border → Decorator → FrameworkElement, | DONE | BorderBrush, BorderThickness, CornerRadius, Padding | — |
| `BorderGapMaskConverter` | BorderGapMaskConverter → IMultiValueConverter | DONE | — | — |
| `BulletDecorator` | BulletDecorator → Decorator → FrameworkElement, | PLANNED | — | — |
| `Button` | Button → ButtonBase → ContentControl, | DONE | IsCancel, IsDefault | — |
| `ButtonBase` | ButtonBase → ContentControl, | PLANNED | ClickMode, Command, CommandParameter, CommandTarget | Click |
| `Calendar` | Calendar → Control → FrameworkElement | DONE | CalendarButtonStyle, CalendarDayButtonStyle, CalendarItemStyle, DisplayDate, DisplayDateEnd, DisplayDateStart, DisplayMode, FirstDayOfWeek, IsTodayHighlighted, SelectedDate, SelectionMode | SelectedDatesChanged |
| `CalendarBlackoutDatesCollection` | CalendarBlackoutDatesCollection → ObservableCollection<CalendarDateRange> | PLANNED | — | — |
| `CalendarButton` | CalendarButton → Button → ButtonBase → ContentControl, | DONE | — | — |
| `CalendarDateChangedEventArgs` | CalendarDateChangedEventArgs → System.Windows.RoutedEventArgs | PLANNED | — | — |
| `CalendarDateRange` | CalendarDateRange → INotifyPropertyChanged | PLANNED | — | — |
| `CalendarDateRangeChangingEventArgs` | CalendarDateRangeChangingEventArgs | PLANNED | — | — |
| `CalendarDayButton` | CalendarDayButton → Button → ButtonBase → ContentControl, | DONE | — | — |
| `CalendarItem` | CalendarItem | DONE | — | — |
| `CalendarKeyboardHelper` | CalendarKeyboardHelper | PLANNED | — | — |
| `CalendarMode` | CalendarMode | PLANNED | — | — |
| `CalendarModeChangedEventArgs` | CalendarModeChangedEventArgs → System.Windows.RoutedEventArgs | PLANNED | — | — |
| `CalendarSelectionChangedEventArgs` | CalendarSelectionChangedEventArgs | PLANNED | — | — |
| `CalendarSelectionMode` | CalendarSelectionMode | PLANNED | — | — |
| `CalendarVisualStates` | CalendarVisualStates | PLANNED | — | — |
| `Canvas` | Canvas → Panel → FrameworkElement, | DONE | Bottom, Left, Right, Top | — |
| `CharacterCasing` | CharacterCasing | DONE | — | — |
| `CheckBox` | CheckBox → ToggleButton → ButtonBase → ContentControl, | DONE | — | — |
| `CleanUpVirtualizedItemEventArgs` | CleanUpVirtualizedItemEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ClickMode` | ClickMode | DONE | — | — |
| `ColumnDefinition` | ColumnDefinition → IList<ColumnDefinition> | PLANNED | MaxWidth, MinWidth, Width | — |
| `ColumnDefinitionCollectionConverter` | ColumnDefinitionCollectionConverter | PLANNED | — | — |
| `ComboBox` | ComboBox → Selector → ItemsControl → Control, | DONE | IsDropDownOpen, IsEditable, MaxDropDownHeight, ShouldPreserveUserEnteredPrefix, StaysOpenOnEdit, Text | — |
| `ComboBoxItem` | ComboBoxItem → ListBoxItem → ContentControl → Control, | DONE | — | — |
| `ContainerTracking` | ContainerTracking | PLANNED | — | — |
| `ContentControl` | ContentControl → Control, | PLANNED | Content, ContentStringFormat, ContentTemplate, ContentTemplateSelector | — |
| `ContentPresenter` | ContentPresenter → FrameworkElement | DONE | ContentSource, ContentStringFormat, RecognizesAccessKey | — |
| `ContextMenu` | ContextMenu → MenuBase → ItemsControl → Control, | DONE | — | — |
| `ContextMenuService` | ContextMenuService → RoutedEventArgs | DONE | ContextMenu, HasDropShadow, HorizontalOffset, IsEnabled, Placement, PlacementRectangle, PlacementTarget, ShowOnDisabled, VerticalOffset | ContextMenuClosing, ContextMenuOpening |
| `Control` | Control → FrameworkElement | PLANNED | HorizontalContentAlignment, Padding, Template, VerticalContentAlignment | MouseDoubleClick, PreviewMouseDoubleClick |
| `ConversionValidationRule` | ConversionValidationRule | DONE | — | — |
| `CustomDictionarySources` | CustomDictionarySources | PLANNED | — | — |
| `CustomPopupPlacement` | CustomPopupPlacement | DONE | — | — |
| `CustomPopupPlacementCallback` | CustomPopupPlacementCallback | DONE | — | — |
| `DataErrorValidationRule` | DataErrorValidationRule → ValidationRule | DONE | — | — |
| `DataGrid` | DataGrid → MultiSelector → Selector → ItemsControl → Control, | DONE | AlternatingRowBackground, AreRowDetailsFrozen, AutoGenerateColumns, CanUserAddRows, CanUserDeleteRows, CanUserReorderColumns, CanUserResizeColumns, CanUserResizeRows, CanUserSortColumns, CellStyle, ClipboardCopyMode, ColumnHeaderHeight, ColumnHeaderStyle, ColumnWidth, CurrentCell, CurrentColumn, CurrentItem, DragIndicatorStyle, DropLocationIndicatorStyle, EnableColumnVirtualization, EnableRowVirtualization, FrozenColumnCount, GridLinesVisibility, HeadersVisibility, HorizontalGridLineThickness, HorizontalGridLinesBrush, IsReadOnly, MaxColumnWidth, MinColumnWidth, MinRowHeight, RowBackground, RowDetailsTemplate, RowDetailsTemplateSelector, RowDetailsVisibilityMode, RowHeaderStyle, RowHeaderTemplate, RowHeaderTemplateSelector, RowHeaderWidth, RowHeight, RowStyle, RowStyleSelector, RowValidationErrorTemplate, SelectionMode, SelectionUnit, VerticalGridLineThickness, VerticalGridLinesBrush | — |
| `DataGridAutoGeneratingColumnEventArgs` | DataGridAutoGeneratingColumnEventArgs → EventArgs | PLANNED | — | — |
| `DataGridBeginningEditEventArgs` | DataGridBeginningEditEventArgs → EventArgs | PLANNED | — | — |
| `DataGridBoundColumn` | DataGridBoundColumn → DataGridColumn → DependencyObject | PLANNED | EditingElementStyle, ElementStyle | — |
| `DataGridCell` | DataGridCell → ContentControl, | DONE | IsEditing, IsSelected | Selected, Unselected |
| `DataGridCellClipboardEventArgs` | DataGridCellClipboardEventArgs → EventArgs | PLANNED | — | — |
| `DataGridCellEditEndingEventArgs` | DataGridCellEditEndingEventArgs → EventArgs | PLANNED | — | — |
| `DataGridCellInfo` | DataGridCellInfo | PLANNED | — | — |
| `DataGridCellsPanel` | DataGridCellsPanel → VirtualizingPanel → Panel → FrameworkElement, | PLANNED | — | — |
| `DataGridCellsPresenter` | DataGridCellsPresenter → ItemsControl → Control, | PLANNED | — | — |
| `DataGridCheckBoxColumn` | DataGridCheckBoxColumn → DataGridBoundColumn → DataGridColumn → DependencyObject | PLANNED | — | — |
| `DataGridClipboardCellContent` | DataGridClipboardCellContent | PLANNED | — | — |
| `DataGridClipboardCopyMode` | DataGridClipboardCopyMode | PLANNED | — | — |
| `DataGridClipboardHelper` | DataGridClipboardHelper | PLANNED | — | — |
| `DataGridColumn` | DataGridColumn → DependencyObject | DONE | CanUserReorder, CanUserResize, CanUserSort, CellStyle, DisplayIndex, DragIndicatorStyle, Header, HeaderStringFormat, HeaderStyle, HeaderTemplate, HeaderTemplateSelector, IsReadOnly, MaxWidth, MinWidth, SortDirection, SortMemberPath, Visibility, Width | — |
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
| `DataGridHeadersVisibilityToVisibilityConverter` | DataGridHeadersVisibilityToVisibilityConverter | DONE | — | — |
| `DataGridHelper` | DataGridHelper | PLANNED | — | — |
| `DataGridHyperlinkColumn` | DataGridHyperlinkColumn → DataGridBoundColumn → DataGridColumn → DependencyObject | PLANNED | — | — |
| `DataGridItemAttachedStorage` | DataGridItemAttachedStorage | PLANNED | — | — |
| `DataGridLength` | DataGridLength | DONE | — | — |
| `DataGridLengthConverter` | DataGridLengthConverter → TypeConverter | PLANNED | — | — |
| `DataGridLengthUnitType` | DataGridLengthUnitType | PLANNED | — | — |
| `DataGridNotificationTarget` | DataGridNotificationTarget | PLANNED | — | — |
| `DataGridPreparingCellForEditEventArgs` | DataGridPreparingCellForEditEventArgs → EventArgs | PLANNED | — | — |
| `DataGridRow` | DataGridRow → Control → FrameworkElement | DONE | DetailsTemplate, DetailsTemplateSelector, DetailsVisibility, Header, HeaderStyle, HeaderTemplate, HeaderTemplateSelector, Item, ValidationErrorTemplate | — |
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
| `DataTemplateSelector` | DataTemplateSelector | PLANNED | — | — |
| `DatePicker` | DatePicker → Control → FrameworkElement | DONE | CalendarStyle, DisplayDate, DisplayDateEnd, DisplayDateStart, FirstDayOfWeek, IsDropDownOpen, IsTodayHighlighted, SelectedDate, SelectedDateFormat, Text | SelectedDateChanged |
| `DatePickerDateValidationErrorEventArgs` | DatePickerDateValidationErrorEventArgs → EventArgs | PLANNED | — | — |
| `DatePickerFormat` | DatePickerFormat | PLANNED | — | — |
| `DatePickerTextBox` | DatePickerTextBox | DONE | — | — |
| `DateTimeHelper` | DateTimeHelper | PLANNED | — | — |
| `Decorator` | Decorator → FrameworkElement, | PLANNED | — | — |
| `DeferredRunTextReference` | DeferredRunTextReference | PLANNED | — | — |
| `DeferredSelectedIndexReference` | DeferredSelectedIndexReference | PLANNED | — | — |
| `DeferredTextReference` | DeferredTextReference | PLANNED | — | — |
| `DefinitionBase` | DefinitionBase → FrameworkContentElement | DONE | SharedSizeGroup | — |
| `DockPanel` | DockPanel → Panel → FrameworkElement, | DONE | Dock, LastChildFill | — |
| `DocumentPageView` | DocumentPageView → FrameworkElement, | PLANNED | PageNumber | — |
| `DocumentViewer` | DocumentViewer → DocumentViewerBase → Control, | DONE | HorizontalOffset, HorizontalPageSpacing, MaxPagesAcross, ShowPageBorders, VerticalOffset, VerticalPageSpacing, Zoom | — |
| `DocumentViewerBase` | DocumentViewerBase → Control, | PLANNED | Document, IsMasterPage | — |
| `DragCompletedEventArgs` | DragCompletedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `DragDeltaEventArgs` | DragDeltaEventArgs → RoutedEventArgs | PLANNED | — | — |
| `DragStartedEventArgs` | DragStartedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `Ellipse` | Ellipse → Shape → FrameworkElement | DONE | — | — |
| `ExceptionRoutedEventArgs` | ExceptionRoutedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ExceptionValidationRule` | ExceptionValidationRule → ValidationRule | DONE | — | — |
| `Expander` | Expander → HeaderedContentControl → ContentControl → Control, | DONE | ExpandDirection, IsExpanded | Collapsed, Expanded |
| `FlowDocumentReader` | FlowDocumentReader → Control, | DONE | Document, IsFindEnabled, IsPageViewEnabled, IsPrintEnabled, IsScrollViewEnabled, IsTwoPageViewEnabled, ViewingMode | — |
| `FlowDocumentScrollViewer` | FlowDocumentScrollViewer → Control, | DONE | Document, IsSelectionEnabled, IsToolBarVisible | — |
| `Frame` | Frame → ContentControl, | DONE | JournalOwnership, NavigationUIVisibility, SandboxExternalContent, Source | — |
| `Grid` | Grid → Panel, | DONE | Column, ColumnSpan, IsSharedSizeScope, Row, RowSpan, ShowGridLines | — |
| `GridSplitter` | GridSplitter → Thumb → Control → FrameworkElement | DONE | DragIncrement, KeyboardIncrement, PreviewStyle, ResizeBehavior, ResizeDirection, ShowsPreview | — |
| `GridView` | GridView → ViewBase, | DONE | AllowsColumnReorder, ColumnCollection, ColumnHeaderContainerStyle, ColumnHeaderContextMenu, ColumnHeaderStringFormat, ColumnHeaderTemplate, ColumnHeaderTemplateSelector, ColumnHeaderToolTip | — |
| `GridViewColumn` | GridViewColumn → DependencyObject, | PLANNED | CellTemplate, CellTemplateSelector, Header, HeaderContainerStyle, HeaderStringFormat, HeaderTemplate, HeaderTemplateSelector | — |
| `GridViewColumnCollection` | GridViewColumnCollection → ObservableCollection<GridViewColumn> | PLANNED | — | — |
| `GridViewColumnCollectionChangedEventArgs` | GridViewColumnCollectionChangedEventArgs | PLANNED | — | — |
| `GridViewColumnHeader` | GridViewColumnHeader → ButtonBase → ContentControl, | DONE | — | — |
| `GridViewHeaderRowPresenter` | GridViewHeaderRowPresenter → GridViewRowPresenterBase → FrameworkElement, | PLANNED | — | — |
| `GridViewRowPresenter` | GridViewRowPresenter → GridViewRowPresenterBase → FrameworkElement, | PLANNED | — | — |
| `GridViewRowPresenterBase` | GridViewRowPresenterBase → FrameworkElement, | PLANNED | Columns | — |
| `GroupBox` | GroupBox → HeaderedContentControl → ContentControl → Control, | DONE | — | — |
| `GroupItem` | GroupItem → ContentControl, | DONE | — | — |
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
| `InkCanvas` | InkCanvas → FrameworkElement, | DONE | Bottom, DefaultDrawingAttributes, EditingMode, EditingModeInverted, Left, Right, Top | ActiveEditingModeChanged, EditingModeChanged, EditingModeInvertedChanged, Gesture, StrokeCollected, StrokeErased |
| `InkCanvasClipboardFormat` | InkCanvasClipboardFormat | PLANNED | — | — |
| `InkCanvasSelectionHitResult` | InkCanvasSelectionHitResult | PLANNED | — | — |
| `InkPresenter` | InkPresenter → Decorator → FrameworkElement, | DONE | Strokes | — |
| `ItemCollection` | ItemCollection → CollectionView, | DONE | — | — |
| `ItemContainerGenerator` | ItemContainerGenerator → IRecyclingItemContainerGenerator, | DONE | — | — |
| `ItemContainerTemplate` | ItemContainerTemplate → DataTemplate | DONE | — | — |
| `ItemContainerTemplateKey` | ItemContainerTemplateKey → TemplateKey | PLANNED | — | — |
| `ItemContainerTemplateSelector` | ItemContainerTemplateSelector | PLANNED | — | — |
| `ItemsChangedEventArgs` | ItemsChangedEventArgs → EventArgs | PLANNED | — | — |
| `ItemsControl` | ItemsControl → Control, | PLANNED | AlternationCount, DisplayMemberPath, GroupStyleSelector, IsTextSearchCaseSensitive, IsTextSearchEnabled, ItemBindingGroup, ItemContainerStyle, ItemContainerStyleSelector, ItemStringFormat, ItemTemplate, ItemTemplateSelector, ItemsPanel, ItemsSource | — |
| `ItemsPanelTemplate` | ItemsPanelTemplate → FrameworkTemplate | DONE | — | — |
| `ItemsPresenter` | ItemsPresenter → FrameworkElement | DONE | — | — |
| `Label` | Label → ContentControl → Control, | DONE | Target | — |
| `LayoutInformation` | LayoutInformation | PLANNED | — | — |
| `Line` | Line → Shape → FrameworkElement | DONE | X1, X2, Y1, Y2 | — |
| `ListBox` | ListBox → Selector → ItemsControl → Control, | DONE | SelectionMode | — |
| `ListBoxItem` | ListBoxItem → ContentControl → Control, | DONE | — | — |
| `ListView` | ListView → ListBox → Selector → ItemsControl → Control, | DONE | View | — |
| `ListViewItem` | ListViewItem → ListBoxItem → ContentControl → Control, | DONE | — | — |
| `MediaElement` | MediaElement → FrameworkElement, | DONE | Balance, IsMuted, LoadedBehavior, ScrubbingEnabled, Source, UnloadedBehavior, Volume | BufferingEnded, BufferingStarted, MediaEnded, MediaFailed, MediaOpened, ScriptCommand |
| `MediaScriptCommandRoutedEventArgs` | MediaScriptCommandRoutedEventArgs → RoutedEventArgs | PLANNED | — | — |
| `Menu` | Menu → MenuBase → ItemsControl → Control, | DONE | IsMainMenu | — |
| `MenuBase` | MenuBase → ItemsControl → Control, | PLANNED | ItemContainerTemplateSelector, UsesItemContainerTemplate | — |
| `MenuItem` | MenuItem → HeaderedItemsControl, | DONE | Icon, InputGestureText, IsCheckable, IsChecked, IsSubmenuOpen, StaysOpenOnClick | Checked, Click, SubmenuClosed, SubmenuOpened, Unchecked |
| `MenuScrollingVisibilityConverter` | MenuScrollingVisibilityConverter → IMultiValueConverter | DONE | — | — |
| `MultiSelector` | MultiSelector → Selector → ItemsControl → Control, | PLANNED | — | — |
| `MultipleCopiesCollection` | MultipleCopiesCollection | PLANNED | — | — |
| `NotifyDataErrorValidationRule` | NotifyDataErrorValidationRule → ValidationRule | DONE | — | — |
| `Orientation` | Orientation | DONE | — | — |
| `Page` | Page → FrameworkElement, | DONE | Title | — |
| `PageRanges` | PageRanges | PLANNED | — | — |
| `Panel` | Panel → FrameworkElement, | PLANNED | Background, IsItemsHost, ZIndex | — |
| `PanningMode` | PanningMode | DONE | — | — |
| `PasswordBox` | PasswordBox → Control, | DONE | PasswordChar | PasswordChanged |
| `PasswordTextContainer` | PasswordTextContainer | PLANNED | — | — |
| `PasswordTextNavigator` | PasswordTextNavigator | PLANNED | — | — |
| `Path` | Path → Shape → FrameworkElement | DONE | Data | — |
| `PlacementMode` | PlacementMode | DONE | — | — |
| `Polygon` | Polygon → Shape → FrameworkElement | DONE | FillRule, Points | — |
| `Polyline` | Polyline → Shape → FrameworkElement | DONE | FillRule, Points | — |
| `Popup` | Popup → FrameworkElement, | DONE | Child, CustomPopupPlacementCallback, HorizontalOffset, IsOpen, Placement, PlacementRectangle, PlacementTarget, PopupAnimation, StaysOpen, VerticalOffset | — |
| `PopupAnimation` | PopupAnimation | DONE | — | — |
| `PopupControlService` | PopupControlService | DONE | — | — |
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
| `ScrollChangedEventArgs` | ScrollChangedEventArgs → RoutedEventArgs | DONE | — | — |
| `ScrollContentPresenter` | ScrollContentPresenter → ContentPresenter, | PLANNED | — | — |
| `ScrollEventArgs` | ScrollEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ScrollUnit` | ScrollUnit | DONE | — | — |
| `ScrollViewer` | ScrollViewer → ContentControl → Control, | DONE | CanContentScroll, HorizontalScrollBarVisibility, IsDeferredScrollingEnabled, PanningDeceleration, PanningMode, PanningRatio, VerticalScrollBarVisibility | ScrollChanged |
| `SelectedCellsChangedEventArgs` | SelectedCellsChangedEventArgs → EventArgs | PLANNED | — | — |
| `SelectedCellsChangedEventHandler` | SelectedCellsChangedEventHandler | PLANNED | — | — |
| `SelectedCellsCollection` | SelectedCellsCollection | DONE | — | — |
| `SelectedDatesCollection` | SelectedDatesCollection → ObservableCollection<DateTime> | PLANNED | — | — |
| `SelectedItemCollection` | SelectedItemCollection | DONE | — | — |
| `SelectionChangedEventArgs` | SelectionChangedEventArgs → RoutedEventArgs | DONE | — | — |
| `SelectiveScrollingGrid` | SelectiveScrollingGrid → Grid → Panel, | DONE | SelectiveScrollingOrientation | — |
| `SelectiveScrollingOrientation` | SelectiveScrollingOrientation | PLANNED | — | — |
| `Selector` | Selector → ItemsControl → Control, | PLANNED | IsSelected, IsSynchronizedWithCurrentItem, SelectedIndex, SelectedItem, SelectedValue, SelectedValuePath | Selected, SelectionChanged, Unselected |
| `Separator` | Separator → Control → FrameworkElement | DONE | — | — |
| `Shape` | Shape → FrameworkElement | PLANNED | Fill, Stretch, Stroke, StrokeDashArray, StrokeDashCap, StrokeDashOffset, StrokeEndLineCap, StrokeLineJoin, StrokeMiterLimit, StrokeStartLineCap, StrokeThickness | — |
| `SinglePageViewer` | SinglePageViewer → DocumentViewerBase, | DONE | MaxZoom, MinZoom, Zoom, ZoomIncrement | — |
| `Slider` | Slider → RangeBase → Control → FrameworkElement | DONE | AutoToolTipPlacement, AutoToolTipPrecision, IsDirectionReversed, IsMoveToPointEnabled, IsSelectionRangeEnabled, IsSnapToTickEnabled, Orientation, SelectionEnd, SelectionStart, TickFrequency, TickPlacement, Ticks | — |
| `SoundPlayerAction` | SoundPlayerAction → TriggerAction, | PLANNED | Source | — |
| `SpellCheck` | SpellCheck | DONE | IsEnabled, SpellingReform | — |
| `SpellingReform` | SpellingReform | PLANNED | — | — |
| `Stack` | Stack → Panel, | PLANNED | Orientation | — |
| `StatusBar` | StatusBar → ItemsControl → Control, | DONE | — | — |
| `StatusBarItem` | StatusBarItem → ContentControl → Control, | DONE | — | — |
| `StickyNote` | StickyNote | PLANNED | CaptionFontFamily, CaptionFontSize, CaptionFontStretch, CaptionFontStyle, CaptionFontWeight, IsActive, IsExpanded, PenWidth | — |
| `StyleSelector` | StyleSelector | PLANNED | — | — |
| `TabControl` | TabControl → Selector → ItemsControl → Control, | DONE | ContentStringFormat, ContentTemplate, ContentTemplateSelector, TabStripPlacement | — |
| `TabItem` | TabItem → HeaderedContentControl → ContentControl → Control, | DONE | — | — |
| `TabPanel` | TabPanel → Panel → FrameworkElement, | PLANNED | — | — |
| `TextAdaptor` | TextAdaptor | PLANNED | — | — |
| `TextBlock` | TextBlock → FrameworkElement, | DONE | BaselineOffset, Text, TextTrimming, TextWrapping | — |
| `TextBox` | TextBox → TextBoxBase, | DONE | CharacterCasing, MaxLength, MaxLines, MinLines, Text | — |
| `TextBoxBase` | TextBoxBase → Control → FrameworkElement | PLANNED | AcceptsTab, AutoWordSelection, CaretBrush, IsInactiveSelectionHighlightEnabled, IsReadOnlyCaretVisible, IsUndoEnabled, SelectionBrush, SelectionOpacity, SelectionTextBrush, UndoLimit | SelectionChanged, TextChanged |
| `TextChange` | TextChange | PLANNED | — | — |
| `TextChangedEventArgs` | TextChangedEventArgs → RoutedEventArgs | DONE | — | — |
| `TextRangeAdaptor` | TextRangeAdaptor | PLANNED | — | — |
| `TextSearch` | TextSearch → DependencyObject | DONE | Text, TextPath | — |
| `Thumb` | Thumb → Control → FrameworkElement | PLANNED | — | DragCompleted, DragDelta, DragStarted |
| `TickBar` | TickBar → FrameworkElement | DONE | Fill, Placement, ReservedSpace | — |
| `TickPlacement` | TickPlacement | PLANNED | — | — |
| `ToggleButton` | ToggleButton → ButtonBase → ContentControl, | PLANNED | IsChecked, IsThreeState | Checked, Indeterminate, Unchecked |
| `ToolBar` | ToolBar → HeaderedItemsControl → ItemsControl → Control, | DONE | Band, BandIndex, IsOverflowOpen, OverflowMode | — |
| `ToolBarOverflowPanel` | ToolBarOverflowPanel → Panel → FrameworkElement, | PLANNED | WrapWidth | — |
| `ToolBarPanel` | ToolBarPanel → StackPanel | PLANNED | — | — |
| `ToolBarTray` | ToolBarTray → FrameworkElement, | DONE | IsLocked, Orientation | — |
| `ToolTip` | ToolTip → ContentControl → Control, | DONE | IsOpen | Closed, Opened |
| `ToolTipService` | ToolTipService → RoutedEventArgs | DONE | BetweenShowDelay, HasDropShadow, HorizontalOffset, InitialShowDelay, IsEnabled, Placement, PlacementRectangle, PlacementTarget, ShowDuration, ShowOnDisabled, ShowsToolTipOnKeyboardFocus, ToolTip, VerticalOffset | ToolTipClosing, ToolTipOpening |
| `Track` | Track → FrameworkElement | DONE | IsDirectionReversed, Orientation, ViewportSize | — |
| `TreeView` | TreeView → ItemsControl → Control, | DONE | SelectedValuePath | SelectedItemChanged |
| `TreeViewItem` | TreeViewItem → HeaderedItemsControl, | DONE | IsExpanded, IsSelected | Collapsed, Expanded, Selected, Unselected |
| `UIElementCollection` | UIElementCollection → IList | DONE | — | — |
| `UniformGrid` | UniformGrid → Panel → FrameworkElement, | PLANNED | Columns, FirstColumn, Rows | — |
| `UserControl` | UserControl → ContentControl → Control, | DONE | — | — |
| `Validation` | Validation | DONE | ErrorTemplate, ValidationAdornerSite, ValidationAdornerSiteFor | Error |
| `ValidationError` | ValidationError | DONE | — | — |
| `ValidationErrorEventArgs` | ValidationErrorEventArgs → RoutedEventArgs | PLANNED | — | — |
| `ValidationResult` | ValidationResult | DONE | — | — |
| `ValidationRule` | ValidationRule | DONE | — | — |
| `ValidationStep` | ValidationStep | PLANNED | — | — |
| `ViewBase` | ViewBase → DependencyObject | PLANNED | — | — |
| `ViewBox` | ViewBox → Decorator → FrameworkElement, | PLANNED | Stretch, StretchDirection | — |
| `ViewPort3D` | ViewPort3D → FrameworkElement, | OUT-OF-SCOPE(11m) | — | — |
| `VirtualizationCacheLength` | VirtualizationCacheLength | DONE | — | — |
| `VirtualizationCacheLengthConverter` | VirtualizationCacheLengthConverter → TypeConverter | PLANNED | — | — |
| `VirtualizationMode` | VirtualizationMode | DONE | — | — |
| `VirtualizedCellInfoCollection` | VirtualizedCellInfoCollection | PLANNED | — | — |
| `VirtualizingPanel` | VirtualizingPanel → Panel → FrameworkElement, | PLANNED | CacheLength, CacheLengthUnit, IsContainerVirtualizable, IsVirtualizing, IsVirtualizingWhenGrouping, ScrollUnit, VirtualizationMode | — |
| `VirtualizingStackPanel` | VirtualizingStackPanel → VirtualizingPanel, | DONE | Orientation | CleanUpVirtualizedItem |
| `VisualStates` | VisualStates | PLANNED | — | — |
| `WebBrowser` | WebBrowser → ActiveXHost | OUT-OF-SCOPE(11m) | — | — |
| `WrapPanel` | WrapPanel → Panel → FrameworkElement, | DONE | ItemHeight, ItemWidth | — |

## Notes / recorded NO-GAP deferrals

- **`Geometry` is `abstract`** in WPF (PresentationCore `Media\Geometry.cs`). A bare `<Geometry>` element is
  NOT authorable — WPF authors concrete derivations (`PathGeometry`, `GeometryGroup`, `EllipseGeometry`…).
  Vaml row: **OUT-OF-SCOPE(11k-4)** — resolved at 11k-4 under concrete derived geometry resource elements.
- **`TextBlock : FrameworkElement`** (NOT a `Control`). Vaml must re-base TextBlock off the framework-element
  branch, not the Control base (it must NOT inherit `Control`'s `Padding`/content-alignment DPs).
- **Abstract bases** (`ContentControl`, `ButtonBase`, `RangeBase`, `ItemsControl`, `Control`, `FrameworkElement`,
  `UIElement`, `TextElement`) are `PLANNED` — their DP surface is the base-branch fidelity sprint (11k-11 S649).

## Manual-add rows (re-applied verbatim — the S503a GEN-LOSSY escape)

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
>   surface S502/S503a owns. **RESOLVED at S503a**: S502 authored the types + behavior path
>   (Behavior + TypedBehavior<T> + BehaviorCollection + TriggerAction + InvokeCommandAction +
>   InteractivityEventTrigger); the rows are flipped PLANNED→DONE **here** at S503a.
> **EventTrigger DISAMBIGUATION (D3):** two DISTINCT EventTrigger classes exist.
> - `System.Windows EventTrigger` (RoutedEvent/SourceName — the `Style.Triggers` family) → stays `PLANNED`
>   (delivered at S559's full re-apply). NOT flipped by 11f.
> - `Microsoft.Xaml.Behaviors EventTrigger` (EventName/SourceObject/Actions) → flipped `DONE` at S503a
>   (VAML maps it → `InteractivityEventTrigger`, the S502 behavior that fires its ordered
>   `TriggerAction`s on the routed event).
> The S11k-5 EventTrigger container surface was authored at 11e (S495a); its `TriggerAction` payload
> distributes 11e/11f/11g — 11f owns the behaviors action family.
>
> **GEN-LOSSY NOTE (S503a, recorded NO-GAP not silent):** `gap_dp_daggen.py` REGENERATES the whole file
> and therefore **overwrites these manual-add rows** (the generator's scan scope cannot emit the behaviors
> family / the `DataTemplate → FrameworkTemplate` parent / the EventTrigger DISAMBIGUATION). Running it
> after a manual-add is therefore DESTRUCTIVE to the closure-under-ancestry rows. The correct process is
> the manual-add escape hatch itself: the generator's output is the base, and the manual rows are
> re-applied AFTER any regeneration (they are never generated). At S503a the generator was NOT re-run
> destructively; the manual rows + their PLANNED→DONE flips are the authoritative DAG state here.
>
> **Tri-state:** every WPF type is in EXACTLY ONE state — `DONE` (Vaml models it + its surface),
> `PLANNED` (declared, scheduled), or `OUT-OF-SCOPE(<resolution-sprint>)` (declared, NOT silent).
> *Nothing is hidden*: unmodeled controls are declared `OUT-OF-SCOPE`, never omitted.


> **11n-0 DAG-EXT (manual add, Sprint-11n content, under the closure-under-ancestry invariant).**
> `gap_dp_daggen.py` scans only `PresentationFramework\System\Windows\Controls`, `Controls\Primitives`,
> and `Shapes` — it **cannot** emit PresentationCore (`Input\Command\*`, `System\Windows\Media\*`,
> `System\Windows\Automation\*`, `System\Windows\Input\*`), WindowsBase /
> `System.ComponentModel`/`System.Collections.ObjectModel` (the observable + weak-event interfaces), nor the
> `FrameworkTemplate`/`FrameworkContentElement`/`DispatcherObject` branches. These are the Sprint-11n
> types (added with the DAG update at **11n-0**; the underlying IMPLEMENTATION sprints 11n-1..11n-6 flip the
> rows PLANNED→DONE — this 11n-0 row-add is the DAG update the roadmap wrap commits BEFORE those
> implementation steps, so the DAG is live and complete at 11n-0). Added under the same escape hatch, at
> ground truth from `E:\GitRepos\wpf`:
> - **Input/command spine (11n-2):** `ICommand` (interface), `RoutedCommand → ICommand`,
>   `CommandBinding`, `CommandManager`, `ICommandSource` (interface — the `ButtonBase` touchpoint),
>   `InputGesture` (abstract) → `KeyGesture`/`MouseGesture`, `InputBinding : Freezable, ICommandSource` →
>   `KeyBinding`/`MouseBinding`.
> - **Automation (11n-1):** `AutomationProperties` (static attached-property class on
>   `DependencyObject`/`UIElement`: Name/AutomationId/HelpText/ItemStatus).
> - **Templates (11n-3):** `FrameworkTemplate` (parent of the already-listed `ControlTemplate`/
>   `DataTemplate`; closes the `ItemsPanelTemplate → FrameworkTemplate` parent edge already listed).
> - **Binding/observables (11n-4, OPTION A event-subscription):** `Freezable`,
>   `DispatcherObject`, `IWeakEventListener` (the weak subscription-lifetime contract),
>   `INotifyPropertyChanged` / `INotifyCollectionChanged`, `NotifyCollectionChangedEventArgs`
>   (+ `CollectionChangedAction`), `ObservableCollection<T>`,
>   `BindingBase` (a `MarkupExtension`) → `MultiBinding`/`PriorityBinding`,
>   `BindingExpressionBase` → `BindingExpression`, `IMultiValueConverter`,
>   `ICollectionView` → `CollectionView : DispatcherObject, ICollectionView, INotifyPropertyChanged`.
> - **3D (11n-5):** `Visual3D : DependencyObject, IVisual3DContainer` (NOT a `Visual`),
>   `Viewport3D : FrameworkElement, IAddChild` (the host — corrects the pre-existing `ViewPort3D`
>   spelling row), `Model3D : Animatable`, `Geometry3D : Animatable`, `IVisual3DContainer`,
>   `Animatable` (animatable-content base).
> - **Visuals/adorners (11n-6, done LAST):** `Adorner : FrameworkElement`, `AdornerLayer` (a
>   panel-like adorner host), `VisualTreeHelper` (static visual-tree walk — forces the visual-tree model
>   decision; the ADR-011 classifier/encode collision is recorded NO-GAP by 11n-6).
> All rows below are added as `PLANNED` (their owning 11n step flips them PLANNED→DONE, never silently).

