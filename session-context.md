# session-context.md

last_command: implementit (S801 — COMPOSITE-KEYED KEY STORE — the single most central edit; dep:S800)
plan_digest: S801 re-keys the DP identity at its STORE onto the WPF-ground-truth composite `(name, ownerType)` pair
(resolving the S033/S039 name-only contract drift at its source). `detail/NameTable<T,K>` is TEMPLATED on its key
(K=std::string default so ChangeRecorder/ChangeQueue + every string-keyed store stay BYTE-IDENTICAL — NOT
blanket-rewritten) + a new `detail::CompositeKey` (owned (name,owner) content key, strict pair ==, S141 fold over
BOTH members, no-transient-string borrowed find; never empty-owner in a live path). PropertyRegistry + 
DependencyProperty::GlobalTable + Register/TryGet/Get RE-KEYED onto the composite — dedupe throws
PropertyAlreadyRegistered ONLY on a SAME-PAIR collision (type/flags not key identity); same-name/different-owner
coexists as DISTINCT DPs. A [MINIMAL REAL] guarded UNIQUE-NAME read bridge (TryGetByName/GetByName, RESOLVED BY
S803) is the sanctioned transitional read for the reader's bare-name attrs (a READ surface, never name-only KEYING).
Call-site migration (DependencyObject — store stays name-keyed this step, per-object composite is S802 —,
VamlReader NEW lookup_dp_for_attr: dotted key → native (name,owner) / bare → guarded bridge, ElementDpFidelity,
StyleApplier, AutomationProperties, EnumDomainRegistry, the tools BindingValidator/Vamlc/VBamlReader,
viewer_seed_context). Tests rewritten to COEXISTENCE semantics (PropertyRegistryExhaustive + PropertyKeyRegistry +
the S242–S245 binding suites re-keyed to pair lookups; the S283 cross-TU S280_1Text aligned to ONE canonical owner).
SESSION: CompositeKey.h + NameTable templating + registry/GlobalTable re-key + the guarded bridge + call-site
migration + coexistence test rewrite + both CI lanes green via build/verify_s801.cmd + roadmap S801 [DONE] +
progress.md S801 record + session-context updated. NO MINIMAL REAL introduced (the bridge is tagged
[MINIMAL REAL] RESOLVED BY S803, recorded not silent). ADR-018 ZERO new locks.

## files_touched — S801 (this session)
- src/core/include/vaml/core/detail/CompositeKey.h (NEW): the owned `(name, owner)` content key — strict PAIR == / equal(name,owner), S141 content fold over BOTH members, static fold_of for the store's borrowed-view lookup.
- src/core/include/vaml/core/detail/NameTable.h (MOD): TEMPLATED `<T, K = std::string>` — the string-keyed stores (ChangeRecorder/ChangeQueue/string consumers) stay byte-identical; the K=CompositeKey forms expose insert/find/find_key overloads on `(name, owner)` + the guarded `find_unique_name(name, &matches)` bridge.
- src/core/include/vaml/core/PropertyRegistry.h + src/core/src/PropertyRegistry.cpp (MOD): re-keyed — register_key dedupes on the (name, ownerTypeId) PAIR; try_find/find/id_of/try_id take (name, owner); the error message names BOTH fields.
- src/core/include/vaml/core/DependencyProperty.h + src/core/src/DependencyProperty.cpp (MOD): GlobalTable + Register/TryGet/Get re-keyed onto the (name, owner) pair; NEW [MINIMAL REAL] `TryGetByName`/`GetByName` guarded unique-name bridge (RESOLVED BY S803).
- src/core/src/DependencyObject.cpp / ElementDpFidelity.cpp / StyleApplier.cpp / AutomationProperties.cpp / EnumDomainRegistry.cpp (MOD): call-site migration to the guarded bridge / composite pair.
- src/core/src/VamlReader.cpp (MOD): NEW `lookup_dp_for_attr` — dotted key → native composite (name, owner) (owner = last-dot prefix); bare key → the guarded unique-name bridge; 9 read sites migrated.
- src/tools/src/BindingValidator.cpp / Vamlc.cpp / VBamlReader.cpp (MOD): bare-name reads → TryGetByName/GetByName.
- vaml_viewer/viewer_seed_context.h (MOD): → TryGetByName.
- test/unit/PropertyRegistryExhaustive_test.cpp + PropertyKeyRegistry_test.cpp (REWRITE): coexistence semantics.
- test/unit/PropertyId_test.cpp / MutexRelocation_rekey_test.cpp / DependencyProperty_test.cpp / AttachedProperty_test.cpp / ReadOnlyAndValidation_test.cpp / AttachedInheritanceNotification_test.cpp / HeapChurn_test.cpp + the S242–S245 binding suites (MOD): re-keyed to pair lookups.
- test/unit/Vamlc_s283_test.cpp (MOD): S280_1Text aligned to the ONE canonical owner (S280_1_Owner) so the shared DP stays a single unambiguous (name, owner) registration.
- build/verify_s801.cmd (NEW): the S801 gate (both CI lanes + new-files present).
- docs/roadmap.md (MOD): S801 [DONE] + the guarded-bridge MINIMAL REAL mark (→ S803) added to the active-marks list.
- progress.md (MOD): S801 record + checkpoint advanced to S802.
- session-context.md (MOD): this record.

## build_status (S801 — from first principles)
- windows-msvc: build/verify_s801.cmd -> S801 GREEN. msvc-debug configure clean + build warning-as-error clean + ctest 4/4 PASS (the coexistence-rewritten suites + all prior suites no-regression).
- ubuntu-clang (VM /mnt/hgfs/VamlPOC, clang 21): S801 GREEN -> UBUNTU-GREEN; clang-debug configure + build (warnings 0) + ctest 4/4 PASS.
- Audits: the store-level name-only uniqueness is GONE from PropertyRegistry.cpp + DependencyProperty.cpp (the composite key is the only registry/GlobalTable identity); the unique-name bridge is a [MINIMAL REAL] READ surface (never name-only KEYING) resolved by S803; ADR-018 ZERO new locks.
- windows-clang-debug: NOT a CI lane (pre-existing /Zc:preprocessor clang-cl failure; CI does not run it).

## next_step — S801 implemented (BOTH CI LANES GREEN via verify_s801.cmd)
- S801 done: the composite (name, owner) key store landed end-to-end at the registry + GlobalTable (dedupe on the pair;
  coexistence). The [MINIMAL REAL] unique-name bridge + the reader's dotted-key native composite lookup are in place.
- Next per the 11s roadmap chain = **S802 (PER-OBJECT STORE + RESOLUTION over the composite id)** — re-key the per-object
  DP store + all DependencyObject resolution (register_property/Get/Set/GetEffectiveValue/has_local_value/subscribers/
  marshalling) onto the composite PropertyId, so an object's DPs are distinct per (name, owner). S803 then makes the
  reader dotted-key first-class composite and RETIRES the unique-name bridge.
- Commit is outstanding (standing directive — commitit fires only on your explicit command; driver's seat is yours).

## notes
- The guarded UNIQUE-NAME bridge (TryGetByName/GetByName) is the driver-approved A2 decision: the store is
  composite-only; a bare-name READ resolves only when exactly ONE (owner) registration has the name (never silently
  picks among ambiguous ones), and S803 retires it when no call site can lack an owner.
- The composite owner follows the S800 encoding: a fully-qualified declaring-type string; the same-name/different-owner
  coexistence is the WPF ground truth (DependencyProperty.cs FromNameKey(name, ownerType)).
- No commit made (standing directive — commitit outstanding on your command).
