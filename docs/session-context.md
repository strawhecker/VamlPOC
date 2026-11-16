# session-context.md — Sprint S811 (D2/D3 corrective: wrong-thread THROWS + remove fabricated S041 pipeline)

last_command: implementit (S811)
plan_digest: S811 — drop the wrong-thread value queue + SetResult::QUEUED; non-affine
            Set/Get THROWS (WPF VerifyAccess); remove the fabricated S041 change-
            notification surface (add_property_changed registry, ProcessNotificationQueue,
            descendant-push postbox, ChangeQueue/ChangeProcessor/ChangeRecorder);
            DP notification = synchronous PropertyMetadata with_changed/notifyChanged;
            BindingExpression target write-back re-pointed onto the WPF-faithful
            Expression-as-value seam (DO.cs:705-708 / BindingExpressionBase.cs:397-414).
            Both CI lanes green.
files_touched:
  SRC:
  - src/core/include/vaml/core/DependencyObject.h      (removed SetResult::QUEUED,
    add_property_changed/remove_property_changed/Process pending* queue methods,
    descendant-push members; added DependencyPropertyExpression seam +
    current_expression/set_current_expression; docs corrected to the throw contract)
  - src/core/src/DependencyObject.cpp                  (non-affine Set/Get THROWS; removed
    the S037 queue path, ProcessPendingChanges/pending_change_count, subscriber fire/
    descendant-push; Set explores the current expression for write-back before storing)
  - src/core/include/vaml/core/DependencyPropertyExpression.h  (NEW — WPF Expression-as-value)
  - src/core/src/DependencyPropertyExpression.cpp              (NEW — symbol home)
  - src/core/include/vaml/core/BindingExpression.h       (implements DependencyPropertyExpression;
    removed targetToken_/add_property_changed write-back; transfer_write_back(value))
  - src/core/src/BindingExpression.cpp                   (write-back = Expression.SetValue
    interception; forward transfer temporarily detaches+restores the expression)
  - src/core/CMakeLists.txt                              (removed Change*.cpp; added
    DependencyPropertyExpression.cpp)
  DELETED (fabricated S041/Change* surface):
  - src/core/include/vaml/core/ChangeQueue.h, ChangeProcessor.h, ChangeRecorder.h
  - src/core/src/ChangeQueue.cpp, ChangeProcessor.cpp, ChangeRecorder.cpp
  TEST:
  - deleted test/unit/ChangeQueue_test.cpp, ChangeRecorder_test.cpp,
    ChangeNotificationPipeline_test.cpp, Coalescing_test.cpp
  - test/unit/DependencyObject_marshalling_test.cpp      (QUEUED -> throw asserts)
  - test/unit/DependencyCallbacks_test.cpp, AttachedProperty_test.cpp,
    ReadOnlyAndValidation_test.cpp                        (wrong-thread -> WrongThreadException)
  - test/unit/S802_composite_store_test.cpp              (subscriber -> per-pair with_changed)
  - test/unit/AttachedInheritanceNotification_test.cpp   (descendant-push -> origin metadata cb)
  - test/unit/UIElement_test.cpp, VisualTree_test.cpp     (push -> pull-model inherited VALUE)
  - test/unit/MutexRelocation_rekey_test.cpp             (ChangeQueue Part A removed; Part B kept)
  - test/unit/vaml_test_stringmakers.h                   (removed QUEUED arms)
  - test/unit/CMakeLists.txt
  - build/verify_s811.cmd                                (NEW — S811 verify gate)
build_status:
  S811 GREEN — BOTH CI lanes:
    windows-msvc : msvc-debug build warning-as-error clean + ctest 4/4 passed
                   (core 1296, tools 151, vulkan, presentation 269).
    ubuntu-clang : clang-debug on the Linux VM (cf/debug) configure + build + ctest all pass.
next_step: None for S811 — the corrective is complete and two-lane green. Optional:
           record the S811 resolution (Change* deletion + Expression seam) in the roadmap/progress
           NO-GAP notes; the WPF source lines held for the record are DO.cs:161/412/705-708/1704/
           1732/2037/2039 + BindingExpressionBase.cs:397-414.
notes:
  - WPF-grounded (audited from the real D:\GitRepos\wpf source): a non-affine access throws
    (VerifyAccess); there is NO per-object multi-subscriber list; the two-way write-back is
    Expression-as-value SetValue interception, NOT a metadata/changed or subscriber callback.
  - SetResult::QUEUED (the S037 appended tail) is removed backward-compatibly; APPLIED/REJECTED/
    NO_CHANGE keep their values.
  - thread_affinity_id / is_affine_to_current_thread RETAINED ("the thread that owns the pump").
  - CollectionView keeps its OWN fill->drain deque + swap mutex (does not consume ChangeQueue — verified).
