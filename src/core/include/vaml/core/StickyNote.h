#pragma once
// VAML StickyNote — the WPF-faithful annotation-anchored note (WPF
// PresentationFramework MS\Internal\Controls\StickyNote\StickyNoteAnnotations.cs
// + StickyNoteContentControl.cs + StickyNoteHelper.cs). 11k-A (S-kA-1..S-kA-4).
//
// WPF GROUND TRUTH (read from the three StickyNote sources at plan time):
//   * A `StickyNoteControl` is a `Control` (NOT an authored element) that sits
//     in an ADORNER LAYER over the element it annotates. It is the visible
//     facade of a CAF annotation: its state (position, size, expansion, z) is
//     mirrored into the annotation's "Meta Data" cargo (Left/Top/XOffset/
//     YOffset/Width/Height/IsExpanded/ZOrder), and its body is the "Text Data"
//     (base64-XAML) or "Ink Data" (base64-ISF) cargo.
//   * `StickyNoteContentControl` is an abstract content HOST with two concrete
//     impls: `StickyNoteRichTextBox` (Text) and `StickyNoteInkCanvas` (Ink),
//     behind the abstract Save/Load/Clear/IsEmpty/Type surface.
//   * Positioning is ADORNER-LAYER + TRANSFORM, not layout: the note sits in an
//     `AdornerLayer` keyed to the annotated element; `GetDesiredTransform`
//     composes an optional RTL self-mirror + a TranslateTransform(S204 anchor)
//     + the offset transform. Page-boundary math (`GetOffsets`, persisted
//     _offsetX/Y + transient _deltaX/Y) keeps the note on the page.
//   * The reentrancy guard (WPF `LockHelper`, a Flag lock) prevents the
//     annotation<->SNC mirror from echoing (the "transferring_" S244 precedent).
//
// THE VAML-FAITHFUL MAPPING (conformance-pinned, ADR-011 held — core data):
//   * `StickyNote : UIElement` — the framework-element branch IS UIElement in
//     VAML (the S129/S131 precedent); it is NOT a Control family base (none
//     exists — the established self-contained-control convention). It carries
//     the REAL WPF DP surface: IsExpanded, Author (read-only), PositionTransform
//     (S208 Matrix2D), StickyNoteType {Text, Ink}, Width/Height, ZOrder.
//   * The CAF ENGINE (Annotation/AnnotationResource/AnnotationService) is a
//     REAL permanent core surface in this header — VAML has NO System.Xml, so
//     the cargo values ride the VAML value domain (the SNCAnnotation mapper is
//     the token<->cargo<->value bridge). ADR-018: ZERO new locks (the
//     reentrancy guard is a single-threaded bool RAII guard — the WPF Flag
//     lock, never an OS lock).
//   * The content-host abstraction (Text->RichTextBox FlowDocument / Ink->
//     InkCanvas behind Save/Load/Clear/IsEmpty/Type) is a separate header
//     (StickyNoteContentControl.h). The adorner integration (StickyNoteAdorner
//     + the AdornerHitTest refinement) is the separate StickyNoteAdorner.h step.
//   * Scope (recorded, NOT silent): VAML is preview-only (no in-viewer editing),
//     so S-kA-5 (viewer growth) renders an ADORNER-ANCHORED StickyNote preview
//     + exercises the CAF store headlessly; the round-trip persistence of the
//     base64-XAML/ISF bodies is the S-kA-3 content-serialization arm, bounded
//     per the MaxBufferSize guard.
//
// ZERO MINIMAL REAL. ZERO invented names. ADR-018: ZERO new locks.

#include "vaml/core/UIElement.h"      // the framework-element branch (StickyNote base)
#include "vaml/core/Transform.h"      // Matrix2D (PositionTransform + GetDesiredTransform)
#include "vaml/core/FlowDocument.h"   // the Text content host rides RichTextBox's Document model
#include "vaml/core/RoutedEvent.h"    // RoutedEventArgs (the note raises a change)
#include "vaml/core/export.h"

#include <string>
#include <string_view>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// S-kA-1 — THE CAF ANNOTATION ENGINE (the plain data model + the store).
// WPF names conformance-pinned (Annotation/AnnotationResource/AnnotationService/
// IAttachedAnnotation + the StickyNote metadata token set). VAML has no
// System.Xml, so the cargo values ride the VAML value domain.
// ---------------------------------------------------------------------------

// The annotation cargo type (WPF AnnotationResourceType — the CAF cargos a
// StickyNote owns: the metadata block + one content body).
enum class AnnotationResourceType : std::uint8_t
{
    MetaData = 0, // "Meta Data" — the 8 position/state tokens
    TextData = 1, // "Text Data" — the note's text body
    InkData  = 2, // "Ink Data"  — the note's ink body
};

// One CAF cargo (an ordered name/value pair — the WPF AnnotationResource).
struct VAML_CORE_EXPORT AnnotationResource
{
    AnnotationResourceType type  = AnnotationResourceType::MetaData;
    std::string             token;   // the metadata token name (e.g. "Left", "Width")
    std::string             value;   // the cargo value (VAML value-domain form)

    [[nodiscard]] bool operator==(const AnnotationResource&) const noexcept = default;
};

// An annotation (the WPF Annotation + its ORDERED AnnotationResources).
struct VAML_CORE_EXPORT Annotation
{
    std::string id;                                  // WPF Annotation.Id
    std::string author;                              // WPF Annotation.Author
    std::vector<AnnotationResource> resources;       // ORDERED cargos (WPF order)

    // Metadata cargo helpers: get/set a named metadata token by name.
    [[nodiscard]] const std::string* metadata(const std::string& token) const noexcept;
    void set_metadata(const std::string& token, const std::string& value);

    // Content cargo helpers: get/set the TextData/InkData body.
    [[nodiscard]] const std::string* content(AnnotationResourceType type) const noexcept;
    void set_content(AnnotationResourceType type, const std::string& body);
};

// The StickyNote metadata token vocabulary (the WPF SNCAnnotation XmlToken
// set: Left/Top/XOffset/YOffset/Width/Height/IsExpanded/ZOrder — conformance-
// pinned from StickyNoteAnnotations.cs). Ah anonymous namespace lives in the
// .cpp; the PUBLIC token identity is the enum below.
enum class StickyNoteMetaToken : std::uint8_t
{
    Left = 0, Top = 1, XOffset = 2, YOffset = 3,
    Width = 4, Height = 5, IsExpanded = 6, ZOrder = 7,
};

// The SNCAnnotation mapper authority: token<->cargo-name<->value.
struct VAML_CORE_EXPORT StickyNoteTokens
{
    [[nodiscard]] static const char* name(StickyNoteMetaToken token) noexcept; // WPF cargo token name
    [[nodiscard]] static StickyNoteMetaToken from_name(const std::string& name) noexcept; // the inverse
};

// IAttachedAnnotation — the interface an attached annotation host (a
// StickyNote) implements to materialize its annotation cargos into its live
// DPs and back (the WPF UpdateSNCWithAnnotation / UpdateAnnotationWithSNC
// bridge). A host returns non-owning records; the SERVICE owns the store.
class VAML_CORE_EXPORT IAttachedAnnotation
{
public:
    virtual ~IAttachedAnnotation() = default;

    // Applies the annotation's MetaData/content cargos onto the host's live
    // DPs (annotation -> SNC). Returns the mirror outcome.
    [[nodiscard]] virtual bool ApplyAnnotation(const Annotation& annotation) = 0;

    // Captures the host's live DPs back into the annotation's cargos (SNC ->
    // annotation). Returns a mirror outcome.
    [[nodiscard]] virtual bool UpdateAnnotation(Annotation& annotation) const = 0;

    // The anchored element this host annotates (the AdornerLayer anchor; may
    // be nullptr until attached).
    [[nodiscard]] virtual UIElement* AnchoredElement() const noexcept = 0;
};

// AnnotationService — the CAF STORE (attach/attach ServiceLocator-LESS: a
// plain ctor, NO DI container, the S250 app-owned composition pattern). The
// service holds NON-OWNING attached-annotation records (the caller owns the
// Annotation + the host object), keyed by the anchored element. AddAttached /
// RemoveAttached / ModifyAttached are the WPF AddAttachedAnnotation /
// RemoveAttachedAnnotation / ModifyAttachedAnnotation surface.
//
// ADR-018: ZERO new locks (single-threaded affinity; the WA reentrancy guard
// is a bool RAII, the WPF LockHelper Flag-lock precedent, never an OS lock).
class VAML_CORE_EXPORT AnnotationService
{
public:
    // An attached annotation record: the annotation (borrowed) + the host
    // object (borrowed) + the anchored element + the mirror-reentrancy guard
    // slot. Non-owning — the service never frees these.
    struct Attached
    {
        Annotation*          annotation = nullptr;
        IAttachedAnnotation* host       = nullptr;
        UIElement*           anchor     = nullptr;
        bool                 mirroring  = false; // the SNC<->annotation reentrancy guard
    };

    AnnotationService() = default;

    // Registers an annotation attached to `anchor` (WPF AddAttachedAnnotation).
    // Returns a borrowed record (nullptr on a duplicate anchor/id — a double
    // attach is refused, never silent).
    [[nodiscard]] Attached* AddAttached(Annotation* annotation,
                                        IAttachedAnnotation* host,
                                        UIElement* anchor);
    // Removes the record (WPF RemoveAttachedAnnotation). Returns true when one
    // was removed.
    [[nodiscard]] bool RemoveAttached(Attached* attached);
    // Re-applies / re-captures the attached annotation (WPF
    // ModifyAttachedAnnotation).
    void ModifyAttached(Attached* attached);

    [[nodiscard]] std::size_t AttachedCount() const noexcept;
    // The attached records (a borrowed view, insertion order).
    void attached(std::vector<const Attached*>& out) const noexcept;

    // --- The reentrancy guard (WPF LockHelper, a bool RAII Flag lock) -------
    // Drives the annotation<->SNC mirror so a change never echoes into a
    // duplicate. Set/queried on the affinity thread; ZERO OS locks (ADR-018).
    [[nodiscard]] bool is_mirroring() const noexcept;

private:
    // The mirror guard is per-service but we hold the per-attached bool AND a
    // coarse service-level flag (the WPF LockHelper is an instance flag). The
    // per-attached `mirroring` is the faithful per-record guard; this flag is
    // the coarse assertion that ONLY ONE mirror runs at a time (single-thread).
    bool mirroring_ = false;
    std::vector<Attached> attached_;
};

// ---------------------------------------------------------------------------
// S-kA-2 — THE STICKYNOTE CONTROL (the visible annotation facade).
// Real, permanent, WPF-faithful surface; ZERO MINIMAL REAL.
// ---------------------------------------------------------------------------
enum class StickyNoteType : std::uint8_t
{
    Text = 0, // WPF StickyNoteType.Text — a RichTextBox body
    Ink  = 1, // WPF StickyNoteType.Ink — an InkCanvas body
};

// StickyNote : UIElement (the framework-element branch). A Control-facade note
// that renders ABOVE its anchored element (hosted by an AdornerLayer) and
// mirrors its state into a CAF annotation. The DP surface is the WPF
// StickyNoteControl set (conformance-pinned):
//   IsExpanded (Bool)  Author (read-only String)  PositionTransform (Matrix2D)
//   StickyNoteType (Int32)  Width/Height (Double, auto/NaN)  ZOrder (Int32)
class VAML_CORE_EXPORT StickyNote final : public UIElement, public IAttachedAnnotation
{
public:
    StickyNote();

    // --- IsExpanded (REAL DP; Bool via Int32, default false) --------------
    [[nodiscard]] SetResult set_is_expanded(bool value);
    [[nodiscard]] bool is_expanded() const;

    // --- Author (REAL read-only DP; String, default "") -------------------
    // WPF StickyNoteControl.Author is read-only (the note's author comes from
    // the annotation, never authored). Set() on it throws PropertyReadOnly
    // (the S045 discipline); set_author is the owner-internal startup path.
    [[nodiscard]] SetResult set_author(const std::string& value);
    [[nodiscard]] std::string author() const;

    // --- StickyNoteType (REAL DP; Int32, default Text=0) ------------------
    [[nodiscard]] SetResult set_sticky_note_type(StickyNoteType t);
    [[nodiscard]] StickyNoteType sticky_note_type() const;

    // --- Width / Height (REAL DPs; Double, default auto/NaN) --------------
    // The note's logical box on each axis (NaN/0 = auto — the content extent).
    [[nodiscard]] SetResult set_note_width(double value);
    [[nodiscard]] double note_width() const;
    [[nodiscard]] SetResult set_note_height(double value);
    [[nodiscard]] double note_height() const;

    // --- ZOrder (REAL DP; Int32, default 0) -------------------------------
    // The note's z within its anchored element's adorner stack (the WPF
    // ZOrder the annotation carries).
    [[nodiscard]] SetResult set_z_order(std::int32_t value);
    [[nodiscard]] std::int32_t z_order() const;

    // --- PositionTransform (REAL DP; Matrix2D, default identity) ----------
    // The anchored-position transform (the WPF PositionTransform the note's
    // Translate+offset compose). S208 Matrix2D value type.
    [[nodiscard]] SetResult set_position_transform(const vaml::Matrix2D& value);
    [[nodiscard]] vaml::Matrix2D position_transform() const;

    // --- The anchored element (WPF StickyNoteControl.AdornedElement) -------
    // The element this note annotates (NON-OWNING). set as part of the
    // adorner attach; must outlive the note (the layer/layout owns it).
    [[nodiscard]] UIElement* AnchoredElement() const noexcept override;
    void set_anchored_element(UIElement* anchor) noexcept;

    // --- The stub content-host slot (S-kA-3 StickyNoteContentControl) -----
    // The note is a façade over a content host: the Text host (a RichTextBox
    // FlowDocument) or the Ink host (an InkCanvas). The physical content host
    // object is authored in StickyNoteContentControl.h; this slot holds the
    // NOTE's owned body state (the serialized text/ink cargo the annotation
    // mirrors). set_body/set_body_text etc. are the note-level surface.
    void set_body_text(const std::string& text);
    [[nodiscard]] std::string body_text() const;
    void set_body_ink(const std::string& ink);
    [[nodiscard]] std::string body_ink() const;

    // --- IAttachedAnnotation (the SNC<->annotation bridge) ----------------
    // Applies the annotation's Metadata/text/ink cargos onto this note's live
    // DPs (annotation -> SNC), guarded by the reentrancy flag. NO-GAP: the
    // live re-position of an anchored StickyNote over its anchor + the page-
    // boundary offset math (WPF GetOffsets, _offsetX/Y persisted + _deltaX/Y
    // transient) is the S-kA-4 adorner-integration step (StickyNoteAdorner.h).
    [[nodiscard]] bool ApplyAnnotation(const Annotation& annotation) override;
    [[nodiscard]] bool UpdateAnnotation(Annotation& annotation) const override;

    // --- Measure (the framework-element branch) ---------------------------
    // The note measures to its authored box (Width/Height DPs; NaN = auto ->
    // a nominal default note box — WPF StickyNoteControl sizes to content).
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    // The mirror guard: the annotation<->SNC reentrancy guard (the WPF
    // LockHelper Flag). Single-threaded; ZERO OS locks (ADR-018).
    UIElement*      anchor_ = nullptr;
    std::string     bodyText_;
    std::string     bodyInk_;
};

} // namespace vaml
