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

> **12k-5 trigger family — MANUAL ADD (closure-under-ancestry / GEN-LOSSY escape):**
> the framework `TriggerBase`/`TriggerAction`/`SetterBase` + the concrete
> `BeginStoryboard`/`SoundPlayerAction`/`ControllableStoryboardAction` rows live OUTSIDE the
> generator's `Controls`/`Primitives`/`Shapes` scan scope (they are `System.Windows`
> core trigger/action types). Added under the closure-under-ancestry invariant so
> `SoundPlayerAction -> TriggerAction` (and the trigger action ancestry) is NOT a dangling
> parent edge. Row states: trigger family PLANNED until the trigger/action delivery steps;
> the interactivity EventTrigger (Microsoft.Xaml.Behaviors) is DISTINCT from the
> System.Windows Style.Triggers EventTrigger (S503a).

> **11f behaviors / DataTemplate — MANUAL ADD (DAG-EXT-11f, GEN-LOSSY escape):**
> the Microsoft.Xaml.Behaviors family (`Behavior -> DependencyObject`, `Behavior<T> -> Behavior`,
> `BehaviorCollection`, `TriggerAction -> DependencyObject`, `InvokeCommandAction -> TriggerAction`,
> interactivity `EventTrigger -> TriggerBase` with EventName/SourceObject) + the missing
> `DataTemplate` row (closing the dangling `ItemContainerTemplate -> DataTemplate` parent edge).
> States: the behaviors rows flipped PLANNED->DONE at S503a (11f) — behavior/trigger/action are
> REAL; the System.Windows `EventTrigger` (Style.Triggers) row stays PLANNED (the Style.Triggers
> family, delivered later).

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
> `E:\GitRepos\wpf`).

## 11n anchor inventory (DONE — first-class DAG rows, re-applied verbatim)

> **11n-7 DAG CLOSE.** These anchor rows are the full 11n delta consolidated into the AGENDA table (the DAG-anchor inventory). Each row flips from the 11n-0 DAG-EXT cluster note to a REAL DONE row as its 11n body step shipped: 11n-1 (AutomationProperties), 11n-2 (ICommand spine: ICommandSource/ Freezable/RoutedCommand/CommandManager/InputGesture/InputBinding), 11n-3 (FrameworkTemplate — and ItemsPanelTemplate, which is ALREADY a DONE generated row re-based onto FrameworkTemplate, so it is intentionally NOT duplicated here), 11n-4 (reactive spine: BindingBase/BindingExpressionBase/ MarkupExtension/IWeakEventListener/ICollectionView/INotifyPropertyChanged/INotifyCollectionChanged/ + the Binding/MultiBinding/PriorityBinding/BindingExpression + CollectionView additions), 11n-5 (3D: Visual3D/Viewport3D/Model3D/Geometry3D).

| Type | Inheritance (DAG) | Vaml tri-state | Own DPs | Routed events |
|------|-------------------|----------------|---------|---------------|
| `ICommand` | ICommand | DONE | — | CanExecuteChanged |
| `ICommandSource` | ICommandSource | DONE | Command, CommandParameter, CommandTarget | — |
| `Freezable` | Freezable → DependencyObject | DONE | — | — |
| `Animatable` | Animatable → Freezable | DONE | — | — |
| `BindingBase` | BindingBase → MarkupExtension | DONE | — | — |
| `BindingExpressionBase` | BindingExpressionBase → Freezable | DONE | — | — |
| `MarkupExtension` | MarkupExtension | DONE | — | — |
| `FrameworkTemplate` | FrameworkTemplate → DependencyObject | DONE | — | — |
| `DispatcherObject` | DispatcherObject | DONE | — | — |
| `IWeakEventListener` | IWeakEventListener | DONE | — | — |
| `ICollectionView` | ICollectionView | DONE | — | — |
| `INotifyPropertyChanged` | INotifyPropertyChanged | DONE | — | PropertyChanged |
| `INotifyCollectionChanged` | INotifyCollectionChanged | DONE | — | CollectionChanged |
| `RoutedCommand` | RoutedCommand → ICommand | DONE | Name, OwnerType | CanExecuteChanged |
| `CommandManager` | CommandManager | DONE | — | RequerySuggested |
| `InputGesture` | InputGesture | DONE | — | — |
| `KeyGesture` | KeyGesture → InputGesture | DONE | Key, Modifiers | — |
| `MouseGesture` | MouseGesture → InputGesture | DONE | MouseAction, Modifiers | — |
| `InputBinding` | InputBinding → Freezable, ICommandSource | DONE | Command, CommandParameter, CommandTarget | — |
| `KeyBinding` | KeyBinding → InputBinding | DONE | Gesture | — |
| `MouseBinding` | MouseBinding → InputBinding | DONE | Gesture | — |
| `Visual3D` | Visual3D → DependencyObject | DONE | — | — |
| `Viewport3D` | Viewport3D → FrameworkElement | DONE | Camera, Children | — |
| `Model3D` | Model3D → Animatable | DONE | — | — |
| `Geometry3D` | Geometry3D → Animatable | DONE | — | — |
| `Binding` | Binding → BindingBase | DONE | Path, Mode, Converter, Source, FallbackValue | — |
| `MultiBinding` | MultiBinding → BindingBase | DONE | Bindings, Mode, Converter | — |
| `PriorityBinding` | PriorityBinding → BindingBase | DONE | Bindings | — |
| `BindingExpression` | BindingExpression → BindingExpressionBase | DONE | — | — |
| `CollectionView` | CollectionView → DispatcherObject, ICollectionView, INotifyPropertyChanged | DONE | — | — |

> **11n-7 continuation:** the 3D anchors (Visual3D/Viewport3D/Model3D/Geometry3D) are the 11n-5 3D surface (DONE); the Adorner/VisualTreeHelper anchors are the 11n-6 surface (DONE). MultiBinding/ PriorityBinding live multi-source evaluation beyond the mechanism binding is recorded forward (not silent); AdornerHitTest + adorner input routing is the named forward refinement. No silent gaps.
