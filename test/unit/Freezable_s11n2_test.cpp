// 11n-2 — Freezable invariant (vaml::core), isolated from the heavy Element
// PCH context so the MSVC PCH heap does not exhaust on this machine.
//
// Proves the Freezable freeze-write-refusal contract (the WPF "cannot change a
// frozen Freezable" invariant) via InputBinding : Freezable, ICommandSource.
//
// ADR-018: ZERO new locks (single-threaded Freezable state).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include <vaml/core/InputBinding.h>
#include <vaml/core/RelayCommand.h>

using vaml::KeyBinding;
using vaml::KeyGesture;
using vaml::RelayCommand;
using vaml::Value;

TEST_CASE("11n-2: frozen Freezable (KeyBinding) refuses a write (WPF invariant)")
{
    RelayCommand noop([](const Value&) { /* no-op */ });
    RelayCommand other([](const Value&) { /* no-op */ });

    KeyBinding kb(KeyGesture(83, 2));
    kb.with_command(&noop);
    kb.Freeze();
    CHECK(kb.is_frozen() == true);

    // A write after Freeze throws (the WPF "cannot change a frozen Freezable"
    // invariant). Caught with `...` so nothing escapes the test case.
    bool threw = false;
    try { kb.with_command(&other); }
    catch (...) { threw = true; }
    CHECK(threw == true);

    // The pre-freeze binding is preserved (the throw happened before assignment).
    CHECK(kb.source_command() == static_cast<vaml::ICommand*>(&noop));
}
