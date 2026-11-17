#pragma once
// VAML PropertyKey flags — the declarative bit-set carried by a dependency
// property key. S032 (dep:S031). Foundation for the DP property system.
// Extended to the full WPF `FrameworkPropertyMetadataOptions` arm set at
// S818 (Sprint 11aa), byte-faithful to the WPF enum's canonical ordinals so
// VAML's flags are a one-to-one import (no drift), minus the invented
// `InheritOnly` that 11u removed.
//
// The bits are pure *declaration* flags plus the Affects*/render arms that
// carry runtime invalidation behavior (wired in UIElement per
// FrameworkElement.OnPropertyChanged, FE.cs:2198-2251). Anything with other
// behavior (default value, CoerceValue, ValidateValue callbacks) is metadata
// delivered by S034, not a flag — so no such bits exist here, by design.
//
// Flags combine under OR (`operator|`) and are tested with `has_flag`, which
// treats bits as independent (each maps to exactly one semantic). There is no
// "at most one of" constraint among these bits.
//
// WPF-faithful ordinals (FrameworkPropertyMetadata.cs):
//   AffectsMeasure 0x001, AffectsArrange 0x002, AffectsParentMeasure 0x004,
//   AffectsParentArrange 0x008, AffectsRender 0x010, Inherits 0x020,
//   OverridesInheritanceBehavior 0x040, NotDataBindable 0x080,
//   BindsTwoWayByDefault 0x100, Journal 0x400, SubPropertiesDoNotAffectRender 0x800.
// VAML-only ReadOnly = 0x200 — WPF's single free gap (between
// BindsTwoWayByDefault=0x100 and Journal=0x400); the only arm WPF does not
// define, placed there BY DESIGN so the byte layout needs no invented slot.

#include "vaml/core/export.h"

#include <cstdint>

namespace vaml {

enum class PropertyFlags : std::uint32_t
{
    None                          = 0x000,
    AffectsMeasure                = 0x001, // invalidate own measure (FE.cs:2241)
    AffectsArrange                = 0x002, // invalidate own arrange (FE.cs:2243)
    AffectsParentMeasure          = 0x004, // invalidate nearest layout-parent measure (FE.cs:2219)
    AffectsParentArrange          = 0x008, // invalidate nearest layout-parent arrange (FE.cs:2234)
    AffectsRender                 = 0x010, // invalidate visual (FE.cs:2251)
    Inherits                      = 0x020, // participates in the inheritance cascade (S039/S040)
    OverridesInheritanceBehavior  = 0x040, // WPF FPM.cs:18-55 inheritance-behavior override (S819)
    NotDataBindable               = 0x080, // binding refuses this target (S819)
    BindsTwoWayByDefault          = 0x100, // default binding mode is two-way (S242-S244/S819)
    ReadOnly                      = 0x200, // Set() throws (S045); VAML-only, WPF free gap
    Journal                       = 0x400, // WPF journal flag (declaration; runtime later)
    SubPropertiesDoNotAffectRender = 0x800, // gate AffectsRender on non-value changes (FE.cs:2251)
};

// Bitwise OR — combines independent flags into a single PropertyFlags value.
VAML_CORE_EXPORT PropertyFlags operator|(PropertyFlags lhs, PropertyFlags rhs) noexcept;

// True iff every bit in `flag` is set in `value`. Independent-bit semantics:
// the result does not require any *other* bit that may also be set in `value`
// to be absent.
VAML_CORE_EXPORT bool has_flag(PropertyFlags value, PropertyFlags flag) noexcept;

} // namespace vaml
