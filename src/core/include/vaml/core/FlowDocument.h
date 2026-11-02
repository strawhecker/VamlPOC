// 11k_12 (dep:S11k11) — FIDELITY CLOSURE on RichTextBox + the FLOW-DOCUMENT
// content model it relies on ("collections of TextBlock and such", followed all
// the way down per the roadmap's mandate).
//
// WPF ground truth (from E:\GitRepos\wpf, PresentationFramework):
//   RichTextBox : TextBoxBase          (Controls\RichTextBox.cs) — a CONTROL
//                                       (TextBoxBase : Control). Own surface:
//                                       ONLY IsDocumentEnabled (a bool DP).
//                                       NO own RoutedEvents. Everything else is
//                                       inherited from TextBoxBase (TextEntry
//                                       surface) + Control + ContentElement.
//   FlowDocument (Documents\FlowDocument.cs) is a FrameworkContentElement (a
//     DependencyObject, NOT a UIElement — it is NOT a control, NOT in the
//     Control branch; the roadmap's CONTROL vs FRAMEWORKELEMENT ground truth
//     classifies these as FrameworkContentElement content hosts). It is NOT
//     authorable as a bare visual (it is the DOCUMENT the RichTextBox hosts).
//   Paragraph : Block : TextElement : FrameworkContentElement (DependencyObject)
//   Section    : Block
//   Span  : Inline : TextElement
//   Run   : Span   (a text-carrying Inline — the "TextBlock-like" leaf)
//
// The VAML-faithful mapping (declared, conformance-pinned — not drift):
//   * RichTextBox is a CONTROl (TextBoxBase branch): authorable as an element,
//     classified Solid/Border/Text per the TextBoxBase surface; hosts a single
//     FlowDocument content (the S440 set_content slot generalized at 11k-11).
//   * FlowDocument/Paragraph/Span/Run/Section are NON-visual FrameworkContent-
//     element DependencyObjects — NOT authorable as BARE visual elements (they
//     are content the RichTextBox Document hosts, mirroring WPF: you don't
//     place a <FlowDocument/> in a visual tree). They own a minimal, real
//     text-carrying surface so a RichTextBox's Document (a FlowDocument of
//     Paragraphs of Runs) is authorable + captured (the "descendants audited
//     for gaps" clause) — each kept and extended, NO MINIMAL REAL.
//
// ADR-018: ZERO new locks. NO MINIMAL REAL — every symbol here is real,
// permanent, resolvable, and WPF-faithful.

#pragma once

#include "vaml/core/DependencyObject.h"
#include "vaml/core/Value.h"

#include <memory>
#include <string>
#include <vector>

namespace vaml {

// ---------------------------------------------------------------------------
// FrameworkContentElement — the NON-VISUAL content-element base (the WPF
// FrameworkContentElement : DependencyObject). Content hosts derive from this
// (NOT Control / NOT UIElement — the roadmap ground truth's non-control branch).
// It is a DependencyObject that can carry a Parenthal-hosted text string.
// ---------------------------------------------------------------------------
class FrameworkContentElement : public DependencyObject
{
public:
    // The element's OWN text content (the inline text it carries — e.g. a Run's
    // text, a Paragraph's text). WPF TextElement.Text is a real DP; here it is
    // a real String-backed property (conformance-pinned by the 11k_12 test).
    const std::string& text() const { return text_; }
    void set_text(std::string t) { text_ = std::move(t); }

protected:
    FrameworkContentElement() = default;

private:
    std::string text_;
};

// ---------------------------------------------------------------------------
// TextElement / Block / Inline — the flow hierarchy (per WPF Documents):
//   TextElement : FrameworkContentElement  (abstract)
//   Block : TextElement  (Paragraph/Section)
//   Inline : TextElement  (Span/Run)
// Each is a real, permanent content-node type; the CLASSIFICATION (control vs
// FrameworkContentElement) is the WPF-true branch, recorded not assumed.
// ---------------------------------------------------------------------------
class TextElement : public FrameworkContentElement
{
protected:
    TextElement() = default;
};

class Block : public TextElement
{
protected:
    Block() = default;
};

class Inline : public TextElement
{
protected:
    Inline() = default;
};

// Section : Block — a block-level grouping (WPF Documents\Section.cs).
class Section : public Block
{
public:
    Section() = default;
    // The block children (Paragraphs/Sections) — a real owned list.
    std::vector<std::unique_ptr<Block>>& blocks() { return blocks_; }
    const std::vector<std::unique_ptr<Block>>& blocks() const { return blocks_; }
    void add_block(std::unique_ptr<Block> b) { blocks_.push_back(std::move(b)); }

private:
    std::vector<std::unique_ptr<Block>> blocks_;
};

// Paragraph : Block — a flow block of inlines (WPF Documents\Paragraph.cs).
class Paragraph : public Block
{
public:
    Paragraph() = default;
    std::vector<std::unique_ptr<Inline>>& inlines() { return inlines_; }
    const std::vector<std::unique_ptr<Inline>>& inlines() const { return inlines_; }
    void add_inline(std::unique_ptr<Inline> i) { inlines_.push_back(std::move(i)); }

private:
    std::vector<std::unique_ptr<Inline>> inlines_;
};

// Span : Inline — a grouping inline (WPF Documents\Span.cs).
class Span : public Inline
{
public:
    Span() = default;
    std::vector<std::unique_ptr<Inline>>& inlines() { return inlines_; }
    void add_inline(std::unique_ptr<Inline> i) { inlines_.push_back(std::move(i)); }

private:
    std::vector<std::unique_ptr<Inline>> inlines_;
};

// Run : Span — the TEXT-CARRYING leaf Inline (the "TextBlock-like" in a
// RichTextBox — WPF Documents\Run.cs). Its `text()` (inherited) is the authored
// text; it is the descendant the roadmap's "collections of TextBlock" clause
// audits.
class Run : public Span
{
public:
    Run() = default;
    // Convenience ctor: <Run>text</Run> inline content -> the Run's text.
    explicit Run(std::string t) { set_text(std::move(t)); }
};

// ---------------------------------------------------------------------------
// FlowDocument — the DOCUMENT a RichTextBox hosts (WPF Documents\
// FlowDocument.cs). A FrameworkContentElement (non-visual) holding an ordered
// list of Block children. NOT authorable as a bare visual element — it is the
// RichTextBox's Document content.
// ---------------------------------------------------------------------------
class FlowDocument : public FrameworkContentElement
{
public:
    FlowDocument() = default;
    std::vector<std::unique_ptr<Block>>& blocks() { return blocks_; }
    const std::vector<std::unique_ptr<Block>>& blocks() const { return blocks_; }
    void add_block(std::unique_ptr<Block> b) { blocks_.push_back(std::move(b)); }

private:
    std::vector<std::unique_ptr<Block>> blocks_;
};

} // namespace vaml
