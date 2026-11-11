# session-context.md

last_command: implementit (S802 — PER-OBJECT STORE + RESOLUTION over the composite id; dep:S801)
plan_digest: S802 re-keys the per-object DP store + EVERY resolution path in DependencyObject.{h,cpp} onto the
WPF-ground-truth composite `(name, owner)` pair (DependencyProperty.cs FromNameKey(name, ownerType)), delivering
S801's recorded NO-GAP at the object layer. Two same-name/different-owner DPs now COEXIST as distinct per-object
store entries and resolve independently. Store: PD carries an interned `owner` view + `Store::find(name, owner)`
+ a GUARDED `find_unique_name` (MINIMAL REAL, S803); register_property interns BOTH name AND owner (same-pair
re-register throws; same-name/different-owner registers a DISTINCT entry). Get/Set/GetEffectiveValue/has_local_value/
nearest_ancestor_local + the descendant-push walk + fire_self_subscribers all key on the pair; global descriptor read
uses TryGet(name, owner) (owner-less ids keep the guarded TryGetByName). The S041 subscriber registry subs_ is re-keyed
to a (name, owner) SubKey (pair-keyed hash) with a MINIMAL REAL owner-less legacy fallback (S803) that keeps bare-name
bindings firing. ChangeQueue/ChangeProcessor carry BOTH name+owner (owned strings, re-interned on drain;
enqueue_value/drain_values/ValueDrainFn take (name, owner, value)); ChangeRecorder re-keyed to K=CompositeKey
(per-pair coalescing). ObjectPool.h slot 1216 -> 1344 (composite owner per inline slot + SubKey membership grew the
worst case; the sanctioned RAISE-it path). Tests: MutexRelocation_rekey_test re-keyed to the composite drain signature
+ NEW S802_composite_store_test.cpp (the coexistence headline). SESSION: both CI lanes green via build/verify_s802.cmd,
roadmap S802 [DONE], progress.md S802 record, session-context updated.

## files_touched — S802 (this session)
- src/core/include/vaml/core/DependencyObject.h (MOD): PD now carries an interned `owner` view; Store::find(name, owner)
  composite overload + guarded find_unique_name bridge (MINIMAL REAL, S803); resolve_entry/resolve_global helpers;
  nearest_ancestor_local + descendant-push + fire_self_subscribers/fire_subscribers_for signatures carry the owner;
  subs_ re-keyed to a (name, owner) SubKey + SubKeyHash; forward-declared DependencyProperty.
- src/core/src/DependencyObject.cpp (MOD): register_property (interns name+owner, same-pair throw), Get/GetEffectiveValue/
  has_local_value/Set (composite resolve_entry + resolve_global + canonical-owner materialization), nearest_ancestor_local,
  ProcessPendingChanges (drain resolves (name, owner)), add_property_changed/remove_property_changed/fire_self_subscribers/
  fire_subscribers_for/ProcessNotificationQueue (composite + owner-less legacy fallback), collect_descendant_effectives/
  visit_scope/notify_descendants (composite).
- src/core/include/vaml/core/ChangeQueue.h + src/core/src/ChangeQueue.cpp (MOD): Item + ValueWrite carry BOTH name AND
  owner (owned strings); enqueue_value(name, owner, value) / drain_values callback (name, owner, value); process/enqueue
  carry the composite PropertyId.
- src/core/include/vaml/core/ChangeRecorder.h + src/core/src/ChangeRecorder.cpp (MOD): re-keyed to K=CompositeKey —
  Pending carries owner, order/byName are composite-keyed, per-pair coalescing.
- src/core/src/ChangeProcessor.cpp (MOD): process passes the composite PropertyId through the recorder.
- src/core/src/ObjectPool.h (MOD): kSlotSize 1216 -> 1344, kLargestElement 1168 -> 1344 (S802 composite-identity growth;
  the sanctioned RAISE-it path).
- test/unit/MutexRelocation_rekey_test.cpp (MOD): the S037/S041 marshalling value-write family re-keyed to
  (name, owner, value) enqueue/drain signatures.
- test/unit/S802_composite_store_test.cpp (NEW): the coexistence headline — one object registers "Prop" under ownerA/
  ownerB -> both coexist + independent defaults + Set on one pair never touches the other + same-pair re-register throws
  + pair-keyed subscribers fire independently.
- test/unit/CMakeLists.txt (MOD): added S802_composite_store_test.cpp to vaml-core-tests.
- build/verify_s802.cmd (NEW): the S802 gate (both CI lanes + new-files present).
- docs/roadmap.md (MOD): S802 [DONE] + the owner-less bridge/subscriber-fallback MINIMAL REAL marks (→ S803).
- progress.md (MOD): S802 record + checkpoint advanced to S803.
- session-context.md (MOD): this record.

## build_status (S802 — from first principles)
- windows-msvc: build/verify_s802.cmd -> S802 GREEN. msvc-debug configure clean + build warning-as-error clean + ctest
  4/4 PASS (100% tests passed; the new S802_composite_store tests + all prior suites no-regression; the lone "1 failed"
  assertion is the intentional S036_1 should_fail() demonstration).
- ubuntu-clang (VM /mnt/hgfs/VamlPOC, clang 21): S802 GREEN -> UBUNTU-GREEN; clang-debug configure + build (warnings 0)
  + ctest 4/4 PASS.
- Audits: the per-object store + subscriber registry + marshalling queue no longer hold any name-only per-object identity
  (composite (name, owner) is the only per-object identity when an owner is known); the owner-less read paths
  (DependencyObject find_unique_name / DependencyProperty::TryGetByName / the subscriber owner-less fallback) are all
  [MINIMAL REAL] GUARDED bridges, RESOLVED BY S803, never silently picking among ambiguous coexist pairs. ADR-018 ZERO
  new locks (the sole kept lock is the S037/S041 ChangeQueue mutex, unchanged).
- windows-clang-debug: NOT a CI lane (pre-existing /Zc:preprocessor clang-cl failure; CI does not run it).

## next_step — S802 implemented (BOTH CI LANES GREEN via verify_s802.cmd)
- S802 done: the per-object store + all DependencyObject resolution (register_property/Get/Set/GetEffectiveValue/
  has_local_value/subscribers/marshalling) re-keyed onto the composite (name, owner); coexistence proven by the new
  S802_composite_store_test.
- Next per the 11s roadmap chain = **S803 (READER DOTTED-KEY → FIRST-CLASS COMPOSITE)** — make the reader's dotted-key
  authoring native (name, owner) composite (drop the flatten) and RETIRE the owner-less bridges
  (DependencyObject::find_unique_name / DependencyProperty::TryGetByName/GetByName / the DependencyObject subscriber
  owner-less fallback). S804 then adds the coexistence live-proof and S805 the un-revert.
- Commit is outstanding (standing directive — commitit fires only on your explicit command; driver's seat is yours).

## notes
- Every owner-less read path added by S802 is a guarded [MINIMAL REAL] bridge RESOLVED BY S803 (DependencyObject
  Store::find_unique_name, DependencyProperty::TryGetByName/GetByName already, + the DependencyObject subscriber owner-less
  legacy fallback). Each is tagged with the sentinel; none is extended or backfilled.
- The composite owner follows the S800/S801 encoding (a fully-qualified declaring-type string). The same-name/different-
  owner coexistence at the PER-OBJECT layer is the WPF ground truth (DependencyProperty.cs FromNameKey(name, ownerType)).
- The ObjectPool slot grew 1216 -> 1344 as the sanctioned S145 "RAISE it" consequence of the composite per-object identity
  (the `owner` view per inline PD slot + the SubKey map membership). This is recorded, not silent.
- 11t/11u thread-affinity + synchronous-callback corrections (S810+/S811) are NOT this step's scope — S802 only re-keys
  the existing surface. No commit made (standing directive — commitit outstanding on your command).
