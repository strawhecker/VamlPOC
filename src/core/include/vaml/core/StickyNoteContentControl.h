#pragma once
// VAML StickyNoteContentControl — the WPF-faithful content-host abstraction
// (WPF PresentationFramework MS\Internal\Controls\StickyNote\
// StickyNoteContentControl.cs). 11k-A (S-kA-3).
//
// WPF GROUND TRUTH (read at plan time): StickyNoteContentControl is an
// ABSTRACT content host behind the Save/Load/Clear/IsEmpty/Type surface, with
// two concrete impls: StickyNoteRichTextBox (the Text note body — a RichTextBox
// hosting a FlowDocument, serialized as base64 XAML) and StickyNoteInkCanvas
// (the Ink note body — an InkCanvas hosting a StrokeCollection, serialized as
// base64 ISF). The StickyNoteControl delegates its body persistence to this
// host; the host is the "content is indirection over existing controls" layer.
//
// THE VAML-FAITHFUL MAPPING (conformance-pinned, ADR-011 held — core data):
//   * `StickyNoteContentControl` is an ABSTRACT base for the two concrete
//     hosts — the S127 camera/geometry abstract-base precedent. It is NOT an
//     authorable factory element (abstract — the 11k-4 abstract-base norm).
//   * The REAL concrete hosts: `StickyNoteRichTextBox : UIElement` (a
//     single-Content note body with the S402 Text editing surface) and
//     `StickyNoteInkCanvas : UIElement` (an InkCanvas-backed note body). Each
//     implements Save/Load/Clear/IsEmpty/Type behind the abstract surface.
//   * The VAML serialization form: the bodies are STRING cargos (the value-
//     domain form VAML uses — NO System.Xml / base64, the honest value-domain
//     analogue of the WPF base64-XAML/ISF). The `MaxBufferSize` guard bounds a
//     Save so an unbounded body never explodes the store (bounded + recorded).
//
// ZERO MINIMAL REAL. ZERO invented names. ADR-018: ZERO new locks.
// The S-kA-4 ADORNER integration (StickyNoteAdorner + AdornerHitTest) is the
// separate StickyNoteAdorner.h step (recorded NO-GAP, not silent).

#include "vaml/core/UIElement.h"      // the framework-element branch (hosts)
#include "vaml/core/FlowDocument.h"   // the Text host's Document model
#include "vaml/core/StickyNote.h"     // StickyNoteType + the CAF value-domain
#include "vaml/core/export.h"

#include <string>

namespace vaml {

// The abstract content-host base (WPF StickyNoteContentControl). A StickyNote
// hosts ONE of these; Save/Load/Clear/IsEmpty/Type are the WPF-faithful
// persistence surface over the value-domain body string.
class VAML_CORE_EXPORT StickyNoteContentControl : public UIElement
{
public:
    ~StickyNoteContentControl() override;

    // The note body type (Text vs Ink). Pure.
    [[nodiscard]] virtual StickyNoteType Type() const noexcept = 0;

    // Serializes the host's current content into the VALUE-DOMAIN body string
    // (the WPF Save analogue). Bounded by `maxBufferSize` (a body larger than
    // that is refused + reported — never silent, never explodes the store).
    [[nodiscard]] virtual bool Save(std::string& out, std::size_t maxBufferSize) const = 0;

    // Loads the authored body string into the host (the WPF Load analogue).
    [[nodiscard]] virtual bool Load(const std::string& body) = 0;

    // True iff the host has no content (the WPF IsEmpty directive).
    [[nodiscard]] virtual bool IsEmpty() const noexcept = 0;

protected:
    StickyNoteContentControl() = default;
};

// StickyNoteRichTextBox — the TEXT note body (WPF StickyNoteRichTextBox): a
// single-Document editor host. The VAML real surface: the S402 Text editing
// surface (Text/Body) + a hosted FlowDocument model + the abstract save/load/
// clear/type surface. Classifier -> Solid (the edit-field fill, TextBox
// precedent; the glyph path is S409_1/S400).
class VAML_CORE_EXPORT StickyNoteRichTextBox final : public StickyNoteContentControl
{
public:
    StickyNoteRichTextBox();

    // --- The body content (S402 surface) --------------------------------
    [[nodiscard]] SetResult set_text(const std::string& value);
    [[nodiscard]] std::string text() const;

    // --- The hosted Document (the flow-content model, 11k_12) -----------
    // set_document adopts a FlowDocument; document() returns non-owning.
    void set_document(std::unique_ptr<FlowDocument> doc);
    [[nodiscard]] FlowDocument* document() const noexcept;

    // --- StickyNoteContentControl --------------------------------------
    [[nodiscard]] StickyNoteType Type() const noexcept override { return StickyNoteType::Text; }
    [[nodiscard]] bool Save(std::string& out, std::size_t maxBufferSize) const override;
    [[nodiscard]] bool Load(const std::string& body) override;
    [[nodiscard]] bool IsEmpty() const noexcept override { return text_.empty(); }

    // --- Measure (the framework-element branch) ------------------------
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    // The value-domain body cargo the Save surface produces (the host's owned
    // text body). Declared + defined privately; the Save() face is the
    // abstract surface.
    [[nodiscard]] std::string body_text_cargo() const;

    std::string text_;
    std::unique_ptr<FlowDocument> doc_;
    FlowDocument* docObserver_ = nullptr;
};

// StickyNoteInkCanvas — the INK note body (WPF StickyNoteInkCanvas): an
// InkCanvas-backed note body behind the abstract surface. The VAML real
// surface rides the S554 InkCanvas surface (the authored strokes token list).
class VAML_CORE_EXPORT StickyNoteInkCanvas final : public StickyNoteContentControl
{
public:
    StickyNoteInkCanvas();

    // --- The body content (the S554 InkCanvas Strokes surface) ----------
    [[nodiscard]] SetResult set_strokes(const std::string& value);
    [[nodiscard]] std::string strokes() const;

    // --- StickyNoteContentControl --------------------------------------
    [[nodiscard]] StickyNoteType Type() const noexcept override { return StickyNoteType::Ink; }
    [[nodiscard]] bool Save(std::string& out, std::size_t maxBufferSize) const override;
    [[nodiscard]] bool Load(const std::string& body) override;
    [[nodiscard]] bool IsEmpty() const noexcept override { return strokes_.empty(); }

    // --- Measure (the framework-element branch) ------------------------
    [[nodiscard]] vaml::Size MeasureOverride(const vaml::Size& available) const override;

private:
    std::string strokes_;
};

} // namespace vaml
