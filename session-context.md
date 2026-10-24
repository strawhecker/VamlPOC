# VAML session-context (per-run state)

## last_command
implementit — S246 (dep:S245) ICommand-analogue interface + RelayCommand implementation (invoke + CanExecute + CanExecuteChanged, full no stub) + S247 (dep:S246) RelayCommand tests — BOTH completed + verified from first principles both CI lanes. NEW src/core/include/vaml/core/ICommand.h (the pure abstract `vaml::ICommand` — Execute(Value)/CanExecute(Value) const/add_can_execute_changed(handler)->token/remove_can_execute_changed(token)->bool, the CanExecuteChanged event exposed on the interface, token-subscription registration-ordered per ObservableObject S241 / DependencyObject S041 — a strict superset of the cross-repo delegate.h/Accupulse iterator-handle style, deliberately NOT imported) + NEW src/core/include/vaml/core/RelayCommand.h + NEW src/core/src/RelayCommand.cpp (`vaml::RelayCommand final : ICommand` — REQUIRED ExecuteAction std::function<void(const Value&)> + optional CanExecutePredicate, null => CanExecute TRUE, NotifyCanExecuteChanged raises in REGISTRATION order, MOVE-ONLY per the S170 precedent, moved-from INERT) + MOD src/core/CMakeLists.txt (+RelayCommand.cpp) + NEW test/unit/RelayCommand_test.cpp (13 TEST_CASEs / 52 assertions, Shouldly vocabulary, pure value-domain — registers NO property names) + MOD test/unit/CMakeLists.txt (+RelayCommand_test.cpp) + NEW build/verify_s246.cmd + NEW build/verify_s247.cmd. Checkpoint advanced to S247; next = S247_1 (the vaml/mvvm bindable-property convenience layer).

## plan_digest
S246 (dep:S245) — the ICommand-analogue interface + RelayCommand implementation (full, no stub), + S247 (dep:S246) — RelayCommand tests. S246: NEW vaml::ICommand (pure abstract: Execute(Value) invoke, CanExecute(Value) const side-effect-free query, add_can_execute_changed/remove_can_execute_changed token-subscription CanExecuteChanged event on the interface, WPF-faithful — a strict superset of the cross-repo delegate.h iterator-handle style, rejected; ADR-018 zero locks) + NEW vaml::RelayCommand final : ICommand (REQUIRED ExecuteAction std::function<void(const Value&)>, optional CanExecutePredicate null=>true, Execute exact-parameter, CanExecute side-effect-free, NotifyCanExecuteChanged REGISTRATION-order snapshot-before-invoke, MOVE-ONLY per S170, moved-from INERT) + MOD src/core/CMakeLists.txt + build/verify_s246.cmd. S247: NEW test/unit/RelayCommand_test.cpp (13 TEST_CASEs / 52 assertions, Shouldly, pure value-domain) — execute-only always-executable, predicate CanExecute per parameter, Execute exact-parameter, subscribe fire-once, multi-subscriber REGISTRATION order, remove/re-add-appends (true/false tokens), distinct monotonic tokens, zero-subscriber safe no-op, side-effect-free CanExecute, Value::Null parameter, MOVE-ONLY transfer (move ctor + move-assign re-arm, moved-from inert), re-query contract (captured flag flips CanExecute + subscriber observes current flag) + MOD test/unit/CMakeLists.txt + build/verify_s247.cmd. NO MINIMAL REAL introduced (both symbols complete + permanent; S253 {Command} + S441 consume). NO-GAP (not silent): S247_1 (Bindable-property convenience) + S253 ({Command}) + S441 (app-shell RelayCommands) consume this command primitive. ADR-018: ZERO new locks.

## files_touched
- ADDED src/core/include/vaml/core/ICommand.h (S246 pure ICommand interface + CanExecuteChangedHandler + token type)
- ADDED src/core/include/vaml/core/RelayCommand.h (S246 RelayCommand final : ICommand)
- ADDED src/core/src/RelayCommand.cpp (S246 implementation)
- MOD src/core/CMakeLists.txt (+src/RelayCommand.cpp)
- ADDED test/unit/RelayCommand_test.cpp (S247 suite: 13 TEST_CASEs / 52 assertions)
- MOD test/unit/CMakeLists.txt (+RelayCommand_test.cpp into vaml-core-tests, with the S247 comment block)
- ADDED build/verify_s246.cmd (S246 CI-lane verification, msvc-debug)
- ADDED build/verify_s247.cmd (S247 CI-lane verification, msvc-debug)
- MOD docs/roadmap.md (S246 + S247 marked [DONE] with the full digests; the dep chain S247_1 follows)
- MOD progress.md (checkpoint advanced: last = S247, next = S247_1; S246+S247 SURVIVOR entry + checkpoint header digest)
- MOD session-context.md (this file)
- No NEW md files created (operational files only, per protocol scope).

## build_status
GREEN both CI lanes (from first principles, S246+S247 gate):
- windows-msvc: build/verify_s246.cmd — msvc-debug configure clean + build warning-as-error ( /WX ) CLEAN + ctest 3/3 PASS. The vaml-core-tests exe ran the S247 RelayCommand suite: 13 TEST_CASEs / 52 assertions all PASS (+ all prior suites green = backward-compat re-proof). Three compile-time fixes during the build: ICommand needed a protected default ctor (the deleted copy ctor suppressed the implicit default — C2512), and the test TU needed two micro-cleanups (a `std::vector<bool>::reference` SHOULD arg, and a complex nested SHOULD expression broken into bool locals for MSVC /W4 /WX). All resolved + re-verified green.
- ubuntu-clang: /mnt/hgfs/VamlPOC clang-debug — configure clean + build warnings==0 + ctest 3/3 PASS (EXIT=0 all 3 tests). The S247 suite on Linux, run directly: 13 TEST_CASEs / 52 assertions SUCCESS.
windows-clang-debug is NOT a lane (pre-existing failure; not built/tested).

## next_step
S247_1 (dep:S247) — Sprint 6, the Bindable-property convenience layer over the S240/S241 core (PROPERTY_BINDABLE(type,Name)-style generator: backing field + change notification; the Toolkit-analog convenience layer in vaml/mvvm). Continues the gapless dep chain past the command surface into the MVVM convenience layer. S246's NO-GAP recorded it as a consumer of the RelayCommand + ObservableObject primitives; no forward contracts were introduced by this step beyond that.

## notes
- S246 delivered the ICommand-analogue + RelayCommand as REAL permanent core surface (NOT a STUB / NOT MINIMAL REAL): the interface exposes the CanExecuteChanged event ON ICommand (WPF-faithful — any command consumer re-queries uniformly), and the token-subscription pattern mirrors ObservableObject S241 / DependencyObject S041 exactly (registration order, re-add appends, unknown-token no-op). The external `delegate.h`/Accupulse reference is a CROSS-REPO import (a different project), so it was deliberately NOT adopted; the codebase's own established subscription discipline is a strict superset (token removal is safe under re-entrant removal, never dangles an iterator).
- RelayCommand is MOVE-ONLY per the S170 move-only RAII precedent (owns a handler list + is virtual-rooted): move ctor/assign transfer the action + predicate + subscriber list + next-token counter; a moved-from command is INERT (Execute no-op, CanExecute true, empty list) — no dangling state on destruction.
- S247's suite is pure value-domain (registers NO property names) — a headless core-only proof, ADR-018 zero locks, exactly like the S241 ObservableObject suite. Its 13 TEST_CASEs exercise every arm of the S246 title (invoke + CanExecute + CanExecuteChanged) + the move contract + the re-query contract.
- The `std::vector<bool>` has no real reference type (MSVC proxy), so the re-query test records flag observations as int (0/1) rather than bool elements — a documented test-side portability choice.
- No commits made (per standing directive unless 'commitit' is explicitly commanded).
- No NEW md files created. MD deliverables: roadmap.md (S246 + S247 [DONE]) + progress.md (checkpoint S247/S247_1 + SURVIVOR entry) + session-context.md — the existing operational files only.
