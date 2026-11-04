// 11k-A — STICKYNOTE + CAF ANNOTATION ENGINE, core (S-kA-1..S-kA-4).
//
// WPF ground truth (read from the three StickyNote sources at plan time):
//   - MS\Internal\Controls\StickyNote\StickyNoteAnnotations.cs — the
//     StickyNoteControl DP surface + the SNCAnnotation bridge (the metadata
//     cargo tokens Left/Top/XOffset/YOffset/Width/Height/IsExpanded/ZOrder +
//     the TextData/InkData content cargos) + the reentrancy guard.
//   - StickyNoteContentControl.cs — the abstract content host behind
//     Save/Load/Clear/IsEmpty/Type, with StickyNoteRichTextBox (Text) +
//     StickyNoteInkCanvas (Ink).
//   - StickyNoteHelper.cs — the PART_* template parts + the CAF resource GUID
//     names + LockHelper.
//
// Under test (all CPU-side, both CI lanes):
//   (A) the CAF engine: Annotation metadata/content cargo round-trip + the
//       StickyNoteTokens token<->name authority; AnnotationService
//       AddAttached/RemoveAttached/ModifyAttached + the reentrancy guard + a
//       duplicate attach refused.
//   (B) the StickyNote control: the DP surface round-trips; the SNC<->
//       annotation bridge (ApplyAnnotation / UpdateAnnotation over the real
//       cargos); the anchored-element binding.
//   (C) the content-host abstraction: StickyNoteContentControl abstract base +
//       StickyNoteRichTextBox (Text) + StickyNoteInkCanvas (Ink), behind
//       Save/Load/Clear/IsEmpty/Type + the MaxBufferSize guard.
//   (D) the S-kA-4 adorner integration: GetDesiredTransform composes the
//       PositionTransform onto the offered transform; AdornerHitTest (the
//       resolved 11n-6 forward token); StickyKeepOnPage page-boundary clamp.
//
// ADR-018: ZERO new locks (the mirror guard is a bool RAII, the WPF LockHelper
// Flag). ZERO MINIMAL REAL.

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"
#include <vaml/assert/Shouldly.h>

#include "vaml/core/StickyNote.h"
#include "vaml/core/StickyNoteContentControl.h"
#include "vaml/core/StickyNoteAdorner.h"
#include "vaml/core/AdornerLayer.h"
#include "vaml/core/UIElement.h"
#include "vaml/core/Transform.h"

#include <memory>

using vaml::Annotation;
using vaml::AnnotationResourceType;
using vaml::AnnotationService;
using vaml::StickyNote;
using vaml::StickyNoteMetaToken;
using vaml::StickyNoteType;
using vaml::StickyNoteRichTextBox;
using vaml::StickyNoteInkCanvas;
using vaml::StickyNoteAdorner;
using vaml::Matrix2D;

// ---------------------------------------------------------------------------
// (A) the CAF engine
// ---------------------------------------------------------------------------
TEST_CASE("11k-A: Annotation metadata + content cargo round-trip")
{
    Annotation a;
    a.id = "note-1";
    a.author = "robert";

    a.set_metadata("Left", "10");
    a.set_metadata("Top", "20");
    a.set_metadata("Width", "200");
    const std::string* w = a.metadata("Width");
    SHOULD(ShouldlyPointer(w).ShouldNotBeNull());
    SHOULD(ShouldlyString(*w).ShouldBe("200"));

    // Update-in-place keeps order + value.
    a.set_metadata("Width", "220");
    const std::string* w2 = a.metadata("Width");
    SHOULD(ShouldlyPointer(w2).ShouldNotBeNull());
    SHOULD(ShouldlyString(*w2).ShouldBe("220"));
    SHOULD(ShouldlyString(*a.metadata("Left")).ShouldBe("10"));

    // Content cargo set/get.
    a.set_content(AnnotationResourceType::TextData, "hello");
    a.set_content(AnnotationResourceType::InkData, "stroke-a");
    const std::string* text = a.content(AnnotationResourceType::TextData);
    const std::string* ink = a.content(AnnotationResourceType::InkData);
    SHOULD(ShouldlyPointer(text).ShouldNotBeNull());
    SHOULD(ShouldlyString(*text).ShouldBe("hello"));
    SHOULD(ShouldlyPointer(ink).ShouldNotBeNull());
    SHOULD(ShouldlyString(*ink).ShouldBe("stroke-a"));
    // An absent metadata cargo -> nullptr.
    SHOULD(ShouldlyPointer(a.metadata("Missing")).ShouldBeNull());
}

TEST_CASE("11k-A: StickyNoteTokens token<->name authority (WPF SNCAnnotation set)")
{
    // The full WPF SNCAnnotation XmlToken set round-trips name<->token.
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::Left))).ShouldBe("Left"));
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::Top))).ShouldBe("Top"));
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::XOffset))).ShouldBe("XOffset"));
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::YOffset))).ShouldBe("YOffset"));
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::Width))).ShouldBe("Width"));
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::Height))).ShouldBe("Height"));
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::IsExpanded))).ShouldBe("IsExpanded"));
    SHOULD(ShouldlyString(std::string(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::ZOrder))).ShouldBe("ZOrder"));

    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(vaml::StickyNoteTokens::from_name("Top"))).ShouldBe(static_cast<std::int32_t>(vaml::StickyNoteMetaToken::Top)));
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(vaml::StickyNoteTokens::from_name("IsExpanded"))).ShouldBe(static_cast<std::int32_t>(vaml::StickyNoteMetaToken::IsExpanded)));
}

TEST_CASE("11k-A: AnnotationService add/remove/modify + the reentrancy guard")
{
    AnnotationService svc;
    SHOULD(ShouldlyIntegral(static_cast<std::size_t>(svc.AttachedCount())).ShouldBe(0u));

    auto anchor1 = std::make_unique<vaml::UIElement>();
    auto host1 = std::make_unique<StickyNote>();
    Annotation ann1;
    ann1.id = "ann-1";

    AnnotationService::Attached* attached = svc.AddAttached(&ann1, host1.get(), anchor1.get());
    SHOULD(ShouldlyPointer(attached).ShouldNotBeNull());
    SHOULD(ShouldlyIntegral(static_cast<std::size_t>(svc.AttachedCount())).ShouldBe(1u));

    // A duplicate attach (same anchor) is REFUSED.
    Annotation ann2;
    AnnotationService::Attached* dup = svc.AddAttached(&ann2, host1.get(), anchor1.get());
    SHOULD(ShouldlyPointer(dup).ShouldBeNull());
    SHOULD(ShouldlyIntegral(static_cast<std::size_t>(svc.AttachedCount())).ShouldBe(1u));

    // ModifyAttached runs the mirror (apply then capture) under the guard.
    host1->set_anchored_element(anchor1.get());
    svc.ModifyAttached(attached);
    SHOULD(ShouldlyIntegral(static_cast<int>(svc.is_mirroring())).ShouldBe(0)); // the mirror released

    // Remove returns true once; absent -> false, no throw.
    SHOULD(ShouldlyIntegral(static_cast<int>(svc.RemoveAttached(attached))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<std::size_t>(svc.AttachedCount())).ShouldBe(0u));
    SHOULD(ShouldlyIntegral(static_cast<int>(svc.RemoveAttached(attached))).ShouldBe(0));
}

// ---------------------------------------------------------------------------
// (B) the StickyNote control + the SNC<->annotation bridge
// ---------------------------------------------------------------------------
TEST_CASE("11k-A: StickyNote DP surface round-trip")
{
    StickyNote note;

    (void)note.set_is_expanded(true);
    SHOULD(ShouldlyIntegral(static_cast<int>(note.is_expanded())).ShouldBe(1));
    (void)note.set_is_expanded(false);
    SHOULD(ShouldlyIntegral(static_cast<int>(note.is_expanded())).ShouldBe(0));

    (void)note.set_sticky_note_type(StickyNoteType::Ink);
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(note.sticky_note_type())).ShouldBe(static_cast<std::int32_t>(StickyNoteType::Ink)));
    (void)note.set_sticky_note_type(StickyNoteType::Text);
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(note.sticky_note_type())).ShouldBe(static_cast<std::int32_t>(StickyNoteType::Text)));

    (void)note.set_note_width(240.0);
    (void)note.set_note_height(180.0);
    SHOULD(ShouldlyString(note.note_width()).ShouldBe(240.0f));
    SHOULD(ShouldlyString(note.note_height()).ShouldBe(180.0f));

    (void)note.set_z_order(5);
    SHOULD(ShouldlyIntegral(note.z_order()).ShouldBe(5));

    (void)note.set_position_transform(Matrix2D::translation(10.0f, 20.0f));
    const Matrix2D t = note.position_transform();
    SHOULD(ShouldlyString(t.dx).ShouldBe(10.0f));
    SHOULD(ShouldlyString(t.dy).ShouldBe(20.0f));

    auto anchor = std::make_unique<vaml::UIElement>();
    note.set_anchored_element(anchor.get());
    SHOULD(ShouldlyIntegral((note.AnchoredElement() == anchor.get()) ? 1 : 0).ShouldBe(1));

    note.set_body_text("note text");
    note.set_body_ink("stroke");
    SHOULD(ShouldlyString(note.body_text()).ShouldBe("note text"));
    SHOULD(ShouldlyString(note.body_ink()).ShouldBe("stroke"));
}

TEST_CASE("11k-A: StickyNote SNC<->annotation bridge (apply + capture)")
{
    StickyNote note;
    Annotation ann;
    ann.id = "note-bridge";
    ann.author = "robert";

    ann.set_metadata(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::IsExpanded), "1");
    ann.set_metadata(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::Width), "260");
    ann.set_metadata(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::Height), "170");
    ann.set_metadata(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::ZOrder), "3");
    ann.set_content(AnnotationResourceType::TextData, "hello note");
    ann.set_content(AnnotationResourceType::InkData, "stroke-1");

    SHOULD(ShouldlyIntegral(static_cast<int>(note.ApplyAnnotation(ann))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(note.is_expanded())).ShouldBe(1));
    SHOULD(ShouldlyString(note.note_width()).ShouldBe(260.0f));
    SHOULD(ShouldlyString(note.note_height()).ShouldBe(170.0f));
    SHOULD(ShouldlyIntegral(note.z_order()).ShouldBe(3));
    SHOULD(ShouldlyString(note.body_text()).ShouldBe("hello note"));
    SHOULD(ShouldlyString(note.body_ink()).ShouldBe("stroke-1"));

    Annotation out;
    SHOULD(ShouldlyIntegral(static_cast<int>(note.UpdateAnnotation(out))).ShouldBe(1));
    SHOULD(ShouldlyString(*out.metadata(vaml::StickyNoteTokens::name(vaml::StickyNoteMetaToken::IsExpanded))).ShouldBe("1"));
    SHOULD(ShouldlyString(*out.content(AnnotationResourceType::TextData)).ShouldBe("hello note"));
}

// ---------------------------------------------------------------------------
// (C) the content-host abstraction
// ---------------------------------------------------------------------------
TEST_CASE("11k-A: StickyNoteContentControl abstract base + Text/Ink hosts")
{
    auto textHost = std::make_unique<StickyNoteRichTextBox>();
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(textHost->Type())).ShouldBe(static_cast<std::int32_t>(StickyNoteType::Text)));
    SHOULD(ShouldlyIntegral(static_cast<int>(textHost->IsEmpty())).ShouldBe(1));

    (void)textHost->set_text("editable note body");
    SHOULD(ShouldlyIntegral(static_cast<int>(textHost->IsEmpty())).ShouldBe(0));
    SHOULD(ShouldlyString(textHost->text()).ShouldBe("editable note body"));

    std::string body;
    SHOULD(ShouldlyIntegral(static_cast<int>(textHost->Save(body, 1024))).ShouldBe(1));
    SHOULD(ShouldlyString(body).ShouldBe("editable note body"));
    // An over-budget Save refuses (never explodes the store).
    std::string overflow;
    SHOULD(ShouldlyIntegral(static_cast<int>(textHost->Save(overflow, 4))).ShouldBe(0));

    StickyNoteRichTextBox textHost2;
    SHOULD(ShouldlyIntegral(static_cast<int>(textHost2.Load("loaded body"))).ShouldBe(1));
    SHOULD(ShouldlyString(textHost2.text()).ShouldBe("loaded body"));

    auto inkHost = std::make_unique<StickyNoteInkCanvas>();
    SHOULD(ShouldlyIntegral(static_cast<std::int32_t>(inkHost->Type())).ShouldBe(static_cast<std::int32_t>(StickyNoteType::Ink)));
    SHOULD(ShouldlyIntegral(static_cast<int>(inkHost->IsEmpty())).ShouldBe(1));
    (void)inkHost->set_strokes("m0,0 l10,10");
    SHOULD(ShouldlyIntegral(static_cast<int>(inkHost->IsEmpty())).ShouldBe(0));
    std::string inkBody;
    SHOULD(ShouldlyIntegral(static_cast<int>(inkHost->Save(inkBody, 1024))).ShouldBe(1));
    SHOULD(ShouldlyString(inkBody).ShouldBe("m0,0 l10,10"));

    auto note = std::make_unique<StickyNote>();
    note->set_body_text("note text");
    note->set_body_ink("stroke");
    SHOULD(ShouldlyString(note->body_text()).ShouldBe("note text"));
    SHOULD(ShouldlyString(note->body_ink()).ShouldBe("stroke"));
}

// ---------------------------------------------------------------------------
// (D) the S-kA-4 adorner integration
// ---------------------------------------------------------------------------
TEST_CASE("11k-A: StickyNoteAdorner GetDesiredTransform composes the offset")
{
    auto anchor = std::make_unique<vaml::UIElement>();
    auto note = std::make_unique<StickyNote>();
    note->set_anchored_element(anchor.get());
    (void)note->set_position_transform(Matrix2D::translation(5.0f, 7.0f));

    StickyNoteAdorner adorner(note.get(), anchor.get());
    SHOULD(ShouldlyIntegral((adorner.Note() == note.get()) ? 1 : 0).ShouldBe(1));
    SHOULD(ShouldlyIntegral((adorner.AdornedElement() == anchor.get()) ? 1 : 0).ShouldBe(1));

    const Matrix2D offered = Matrix2D::identity();
    const Matrix2D t = adorner.GetDesiredTransform(offered);
    SHOULD(ShouldlyString(t.dx).ShouldBe(5.0f));
    SHOULD(ShouldlyString(t.dy).ShouldBe(7.0f));

    auto layer = std::make_unique<vaml::AdornerLayer>();
    SHOULD(ShouldlyIntegral(static_cast<int>(layer->Add(&adorner))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(layer->HasAdorners(anchor.get()))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<std::size_t>(layer->AdornerCount())).ShouldBe(1u));
    SHOULD(ShouldlyIntegral(static_cast<int>(layer->Remove(&adorner))).ShouldBe(1));
}

TEST_CASE("11k-A: AdornerHitTest (the resolved 11n-6 forward token)")
{
    auto anchor = std::make_unique<vaml::UIElement>();
    anchor->Measure(vaml::Size{300.0f, 200.0f});
    anchor->Arrange(vaml::Rect{0.0f, 0.0f, 300.0f, 200.0f});

    auto note = std::make_unique<StickyNote>();
    note->set_anchored_element(anchor.get());
    (void)note->set_position_transform(Matrix2D::translation(0.0f, 0.0f));
    (void)note->set_note_width(200.0f);
    (void)note->set_note_height(160.0f);

    StickyNoteAdorner adorner(note.get(), anchor.get());
    SHOULD(ShouldlyIntegral(static_cast<int>(vaml::AdornerHitTest(adorner, 50.0f, 50.0f))).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(vaml::AdornerHitTest(adorner, 250.0f, 50.0f))).ShouldBe(0));
    SHOULD(ShouldlyIntegral(static_cast<int>(vaml::AdornerHitTest(adorner, 50.0f, 250.0f))).ShouldBe(0));
}

TEST_CASE("11k-A: StickyKeepOnPage clamps the position to the page")
{
    const vaml::Rect page{0.0f, 0.0f, 800.0f, 600.0f};
    const vaml::Rect anchor{100.0f, 100.0f, 200.0f, 100.0f};

    vaml::StickyNotePoint in{200.0f, 150.0f};
    const auto kept = vaml::StickyKeepOnPage(in, anchor, page);
    SHOULD(ShouldlyString(kept.x).ShouldBe(200.0f));
    SHOULD(ShouldlyString(kept.y).ShouldBe(150.0f));

    vaml::StickyNotePoint off{-50.0f, -50.0f};
    const auto clamped = vaml::StickyKeepOnPage(off, anchor, page);
    SHOULD(ShouldlyIntegral(static_cast<int>(clamped.x >= 0.0f)).ShouldBe(1));
    SHOULD(ShouldlyIntegral(static_cast<int>(clamped.y >= 0.0f)).ShouldBe(1));
}
