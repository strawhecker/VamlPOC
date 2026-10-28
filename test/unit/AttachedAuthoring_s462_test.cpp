// S462 (dep:S461) — ATTACHED-PROPERTY AUTHORING + FrameworkElement `Name`.
//
// The S2 text reader authors attached properties + a FrameworkElement Name:
//   * `<Canvas.Left="12.5">` / `<Canvas.Top="3">` — attached DPs authored as a
//     PLAIN dotted attribute, resolved through the S043 attached-aware DP path
//     (the generic dotted-key authoring the roadmap title "attached-property
//     authoring" delivers; Grid.Row/Grid.Column already work this way).
//   * `Name="submitButton"` — a plain FrameworkElement Name attribute registers
//     the built object in the S092 name registry ON PAR with `x:Name` (so
//     `named("submitButton")` resolves; the Name-scope identity, not a DP).
//   * a duplicate `Name`, like a duplicate `x:Name`, is the DuplicateName authoring
//     error (never silent last-wins).
//
// NO-GAP (recorded, not silent): the `DockPanel.Dock` float is deferred to S470
// (Sprint 11c) — the DockPanel element is not authored yet; S462 delivers the
// ATTACHED-AUTHORING + Canvas.Left/Top + Name half of the title. (The roadmap
// title listed DockPanel.Dock + Canvas.Left; Grid.Row/Grid.Column already work.)
//
// Registers no S462-unique DPs (Canvas.Left/Top are Canvas-owned, S136).
// Uses the Shouldly vocabulary. ADR-018 zero locks.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/Element.h"          // Canvas + Canvas::left/top + register_builtin_elements
#include "vaml/core/PropertyId.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include <memory>
#include <string>

using vaml::Canvas;
using vaml::DependencyObject;
using vaml::PropertyId;
using vaml::UIElement;
using vaml::Value;
using vaml::VamlReader;
using vaml::VamlReaderError;
using vaml::VamlReaderException;

// ---------------------------------------------------------------------------
// 1. Attached DPs author through the generic dotted-key path
// ---------------------------------------------------------------------------

TEST_CASE("S462: Canvas.Left / Canvas.Top author as attached DPs on a child")
{
    // Canvas.Left/Top are attached DPs registered by the Canvas ctor (S136
    // self-registration); construct a Canvas so the global DP exists before a
    // non-Canvas element authors it (the reader never auto-registers control DPs).
    { Canvas probe; (void)probe; }

    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Button x:Name=\"B\" Canvas.Left=\"12.5\" Canvas.Top=\"3\"/>"
        "</StackPanel>");

    // The named child carries the authored attached values (the S043 path).
    auto* button = dynamic_cast<UIElement*>(reader.named("B"));
    SHOULD(ShouldlyIntegral(button != nullptr).ShouldBe(1));
    if (button)
    {
        // Canvas.Left/Top are the attached Canvas DPs, readable via the statics.
        SHOULD(ShouldlyString(Canvas::left(*button)).ShouldBe(12.5));
        SHOULD(ShouldlyString(Canvas::top(*button)).ShouldBe(3.0));
    }
}

// ---------------------------------------------------------------------------
// 2. FrameworkElement Name registers in the S092 name registry (on par with x:Name)
// ---------------------------------------------------------------------------

TEST_CASE("S462: plain Name=\"submitButton\" registers in the S092 name registry")
{
    VamlReader reader;
    std::unique_ptr<DependencyObject> root = reader.Load(
        "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
        "<Button Name=\"submitButton\"/>"
        "</StackPanel>");

    // name_count includes the plain-Name registration; named() resolves it.
    SHOULD(ShouldlyIntegral(reader.name_count()).ShouldBe(1u));
    auto* btn = reader.named("submitButton");
    SHOULD(ShouldlyIntegral(btn != nullptr).ShouldBe(1));
    if (btn)
    {
        SHOULD(ShouldlyIntegral(dynamic_cast<vaml::UIElement*>(btn) != nullptr).ShouldBe(1));
    }
}

// ---------------------------------------------------------------------------
// 3. A duplicate Name is the DuplicateName authoring error (never silent)
// ---------------------------------------------------------------------------

TEST_CASE("S462: a duplicate Name throws DuplicateName (never silent last-wins)")
{
    VamlReader reader;
    bool threw = false;
    try
    {
        (void)reader.Load(
            "<StackPanel xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
            "<Button Name=\"dup\"/><Button Name=\"dup\"/>"
            "</StackPanel>");
    }
    catch (const VamlReaderException& e)
    {
        threw = true;
        SHOULD(ShouldlyIntegral(static_cast<int>(e.kind()))
                   .ShouldBe(static_cast<int>(VamlReaderError::DuplicateName)));
    }
    SHOULD(ShouldlyIntegral(static_cast<int>(threw)).ShouldBe(1));
}
