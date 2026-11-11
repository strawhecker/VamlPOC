// S285 (dep:S284) — Runtime VBamlReader: bytes → DependencyObject tree
// (InitializeComponent-style).
//
// Decodes a .vbaml byte stream (S282) and materializes a LIVE DependencyObject
// tree via the S2 registries (S087 type table, S089 value table, S032–S039
// property table) — the compiled-path "S7 reader". Lives in vaml-tools (core
// cannot depend on tools; tools → core only). Single-threaded + pure
// (ADR-018: zero locks); ADR-021 headless-testable.
//
// S289 (dep:S288) — adds the PACK/IDENTITY surface (the InitializeComponent
// analogue + the x:Class owner mapping + the compiled-only entry). See Pack.h
// + the header comment.

#include "vaml/tools/VBamlReader.h"

#include "vaml/tools/Pack.h"               // pack::ResourcePack (S289)
#include "vaml/tools/vbaml/VbamlSchema.h"
#include "vaml/tools/vbaml/VbamlSerializer.h"
#include "vaml/tools/Vamlc.h" // CompiledProgram / ElementRecord / AttributeRecord

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h" // UIElement (S440: S121 transfer needs the child-host API)
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/Intern.h" // S140_1: vaml::intern — process-stable retained string_view
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h"
#include "vaml/core/EnumDomainRegistry.h"   // S600: shared TYPE-AWARE int32 enum conversion (A1)
#include "vaml/core/ValueConverter.h"
#include "vaml/core/Color.h"        // S643a: parse_color (hex/named color literal)
#include "vaml/core/Brush.h"        // S643a: SolidColorBrush (fresh literal brush)
#include "vaml/core/ResourceDictionary.h" // S643a: set_element_resources (flush)
#include "vaml/core/UIElement.h"    // S643a: resources() slot (registry target)

#include <algorithm> // std::find (event-name scan)
#include <charconv>  // S643a: std::from_chars (numeric brush-id arm, parse_int32_tools)
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vaml::tools {

// ---------------------------------------------------------------------------
// VBamlException
// ---------------------------------------------------------------------------

VBamlException::VBamlException(VBamlError kind, std::string what, std::size_t elementId,
                               std::string detail)
    : std::runtime_error(std::move(what)), kind_(kind), elementId_(elementId),
      detail_(std::move(detail))
{
}

// ---------------------------------------------------------------------------
// VBamlReader ctor/dtor
// ---------------------------------------------------------------------------

VBamlReader::VBamlReader() = default;
VBamlReader::~VBamlReader() = default;

namespace {

// S285: the type-name a ValueType maps to for VALUE conversion — a mirror of
// the S2 reader's converterNameFor (S091). Only the tags the S089 built-in
// converters can produce are represented (Int32/Bool/Color/Thickness +
// S444's Double — the S7 compiled path must convert Double literals exactly as
// the S2 text path does per the S288 parity contract: MainView.vaml's
// FontSize/Slider.* values are Double). String is handled by identity. Returns
// an empty view for a type with no converter.
[[nodiscard]] std::string_view converterNameFor(vaml::ValueType t) noexcept
{
    using vaml::ValueType;
    switch (t)
    {
        case ValueType::Int32:     return "Int32";
        case ValueType::Bool:      return "Bool";
        case ValueType::Color:     return "Color";
        case ValueType::Thickness: return "Thickness";
        case ValueType::Double:    return "Double";   // S444: FontSize / Slider.* route through the Double converter
        case ValueType::GridLength: return "GridLength"; // S601: grid-track / grid-length DPs (S288 parity with S2)
        default:                   return "";
    }
}

// S285: convert a plain literal to the DP's bound Value — mirrors the S2
// reader's convertLiteral (S091): a String-typed property takes it verbatim;
// otherwise the S089 value registry convert(typeName, literal). Throws
// ValueConversionException which the caller maps into VBamlError::MalformedValue.
[[nodiscard]] vaml::Value convertLiterals(const vaml::DependencyProperty& dp,
                                          std::string_view literal)
{
    using vaml::Value;
    using vaml::ValueType;
    using vaml::ValueConverterRegistry;

    const ValueType bound = dp.metadata().boundType();
    if (bound == ValueType::String)
    {
        return Value::String(std::string(literal));
    }
    if (bound == ValueType::Int32)
    {
        // S600 (A1): TYPE-AWARE enum conversion — shared with the S2 reader so
        // the S288 parity contract holds exactly (same per-DP enum-domain table).
        return vaml::convert_int32_for_dp(dp, literal);
    }
    const std::string_view typeName = converterNameFor(bound);
    if (typeName.empty())
    {
        throw VBamlException(VBamlError::MalformedValue,
                             "no value converter registered for the property's bound type",
                             kNoElement);
    }
    return ValueConverterRegistry::global().convert(typeName, literal);
}

} // namespace

namespace {

// S643a — mirrors the S2 reader's parse_int32 (VamlReader.cpp): true iff `s`
// parses wholly as a signed 32-bit integer. Used for the numeric brush-id arm.
[[nodiscard]] bool parse_int32_tools(std::string_view s, std::int32_t& out) noexcept
{
    if (s.empty())
    {
        return false;
    }
    const char* begin = s.data();
    const char* end = s.data() + s.size();
    const auto [ptr, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && ptr == end;
}

} // namespace

// S643a (dep:S643) — HEX-COLOR LITERAL ON BrushId DPs (compiled-path parity
// with the S2 text reader's VamlReader::convertBrushIdLiteral). Bounded by the
// call site to a DP whose bound type is BrushId. See VBamlReader.h member note.
vaml::Value VBamlReader::convertBrushIdLiteral(DependencyObject* target,
                                               vaml::ValueType boundType,
                                               std::string_view literal)
{
    using vaml::Value;
    const bool isObject = (boundType == vaml::ValueType::Brush); // S643b-1 (D1)

    // 1) NUMERIC brush id — the GeometryDrawing / StaticResource numeric
    //    convention. Only the legacy BrushId DP keeps the numeric-id path.
    std::int32_t id = 0;
    if (parse_int32_tools(literal, id))
    {
        if (!isObject)
        {
            return Value::BrushId(id);
        }
        throw VBamlException(VBamlError::MalformedValue,
                             "a numeric literal on a Brush OBJECT DP is not "
                             "WPF-faithful; author a color literal",
                             kNoElement);
    }

    // 2) COLOR literal — parse_color throws the structured ValueConversionException
    //    (MalformedValue, "Color") on a malformed literal; never silent.
    const vaml::Color c = vaml::parse_color(literal);

    // 3) Brush OBJECT DP (S643b-1 D1 CLOSED + S643b-4 D3): the color's SHARED
    //    FROZEN SolidColorBrush held directly. Same literal -> same shared
    //    frozen object (WPF TryGetFreezable, Brush.cs:55-77).
    if (isObject)
    {
        return Value::Brush(vaml::shared_frozen_solid_brush(c));
    }

    // 4) Registration divergence — the legacy BrushId path needs a UIElement
    //    target whose resources dict the render-time find_for_element walk
    //    resolves. A non-UIElement target fails loud (no resolution seam — see
    //    the S2 member note, GeometryDrawing case).
    vaml::UIElement* ui = dynamic_cast<vaml::UIElement*>(target);
    if (ui == nullptr)
    {
        throw VBamlException(VBamlError::MalformedValue,
                             "a color literal on a BrushId DP requires a UIElement "
                             "target to register the brush under 'Brush:<id>'",
                             kNoElement);
    }

    const std::int32_t autoId = vaml::next_auto_brush_id();
    auto brush = std::make_unique<vaml::SolidColorBrush>();
    (void)brush->set_color(c);
    pending_brushes_.push_back(PendingBrush{ ui, autoId, std::move(brush) });
    return Value::BrushId(autoId);
}

// S643a — flushes pending literal brushes (mirror of VamlReader::flush_pending_brushes).
void VBamlReader::flush_pending_brushes()
{
    for (PendingBrush& pending : pending_brushes_)
    {
        vaml::ResourceDictionary* dict = pending.target->resources();
        std::unique_ptr<vaml::ResourceDictionary> ownedDict;
        if (dict == nullptr)
        {
            ownedDict = std::make_unique<vaml::ResourceDictionary>();
            dict = ownedDict.get();
        }
        dict->set("Brush:" + std::to_string(pending.brushId), std::move(pending.brush));
        if (ownedDict)
        {
            vaml::detail::set_element_resources(pending.target, std::move(ownedDict));
        }
    }
    pending_brushes_.clear();
}

// ---------------------------------------------------------------------------
// S285 — the reader's entry: decode + build.
// ---------------------------------------------------------------------------

std::unique_ptr<vaml::DependencyObject>
VBamlReader::Load(const std::vector<std::uint8_t>& bytes)
{
    // Per-Load clearing (the S091/S092 discipline — no cross-Load leakage).
    // The event registration set (event_names_, set via register_event) is
    // reader-level CONFIGURATION and survives per-Load clearing — matching the
    // S092 contract — so it is NOT reset here.
    retained_.clear();
    visual_finalized_ = false; // S440: a fresh Load re-arms the finalize hand-off
    root_class_.clear();
    names_.clear();
    deferred_extensions_.clear();
    deferred_event_handlers_.clear();
    property_element_children_.clear(); // S452: fresh Load re-arms the object-property set
    pending_brushes_.clear();           // S643a: per-Load literal-brush pending scope
    // S643b-2: the auto-id counter is the PROCESS-LIFETIME core allocator
    // (vaml::next_auto_brush_id) — NOT reset per Load.

    // Decode the .vbaml (S282). A structurally malformed buffer throws
    // VbamlMalformedBuffer; map it to the compiled reader's MalformedBuffer
    // category so the caller always gets a stable VBamlException kind.
    vaml::tools::vbaml::Schema schema;
    try
    {
        schema = vaml::tools::vbaml::decode_schema(bytes);
    }
    catch (const vaml::tools::vbaml::VbamlMalformedBuffer& e)
    {
        throw VBamlException(VBamlError::MalformedBuffer, e.what(), kNoElement);
    }

    vaml::tools::CompiledProgram prog;
    try
    {
        prog = vaml::tools::vbaml::decode(bytes, schema);
    }
    catch (const vaml::tools::vbaml::VbamlMalformedBuffer& e)
    {
        throw VBamlException(VBamlError::MalformedBuffer, e.what(), kNoElement);
    }

    if (prog.elements.empty())
    {
        throw VBamlException(VBamlError::MalformedBuffer, "empty .vbaml: no elements",
                             kNoElement);
    }

    // Materialize every element in document order (id == index). Each slot is a
    // unique_ptr (default nullptr) so a child can be referenced by id after its
    // parent exists; ownership is unified at the end (root to caller, rest to
    // retained_).
    std::vector<std::unique_ptr<vaml::DependencyObject>> nodes(prog.elements.size());

    // Process each element: instantiate, then consume the four attribute
    // strands in S2-faithful order (directives → event routing → deferred
    // extensions → plain literals). `isRoot` marks id 0 so x:Class-on-non-root
    // is an authoring error only for depth != 0 (S092/S094).
    for (std::size_t id = 0; id < prog.elements.size(); ++id)
    {
        const vaml::tools::ElementRecord& rec = prog.elements[id];
        const bool isRoot = (id == 0);

        // Instantiate via the S087 type table (unknown type → UnknownType).
        try
        {
            nodes[id] = vaml::ElementFactoryRegistry::global().create(rec.typeToken);
        }
        catch (const vaml::ElementNotFound&)
        {
            throw VBamlException(VBamlError::UnknownType,
                                 "unknown element type '" + rec.typeToken + "'", id);
        }

        // S286 Strand A — x:Name / x:Class / x:Key directives (compiled-path
        // analogue of S092/S094). The compiler interns the directive keys under
        // "x:*"; resolve by exact directive name.
        for (const vaml::tools::AttributeRecord& a : rec.attributes)
        {
            if (a.key == "x:Name")
            {
                const std::string_view name = vaml::intern(a.value);
                // Duplicate x:Name is an authoring error — fail loud, never
                // last-wins (matches the S092 name-scope contract).
                for (const auto& n : names_)
                {
                    if (n.first == name)
                    {
                        throw VBamlException(VBamlError::DuplicateName, std::string(name),
                                             id, a.key);
                    }
                }
                names_.emplace_back(name, nodes[id].get());
                continue;
            }
            if (a.key == "x:Class")
            {
                if (!isRoot)
                {
                    throw VBamlException(VBamlError::ClassOnNonRoot,
                                         std::string(a.value), id, a.key);
                }
                root_class_ = std::string(a.value);
                continue;
            }
            if (a.key == "x:Key" || a.key == "x:Null")
            {
                // x:Key → S361, x:Null → S084: still deferred on the compiled
                // path, recorded NO-GAP, not silent.
                continue;
            }
        }

        // S286 Strand C — event routing (registered event names → deferred
        // handler). A key that exactly matches a registered event is an EVENT,
        // never a property set / unknown-property throw.
        for (const vaml::tools::AttributeRecord& a : rec.attributes)
        {
            if (a.isBinding || a.isExtension)
            {
                continue; // handled in Strand B
            }
            std::string_view k = vaml::intern(a.key);
            const bool isEvent = std::find(event_names_.begin(), event_names_.end(), k) !=
                                 event_names_.end();
            if (isEvent)
            {
                deferred_event_handlers_.push_back(
                    VBamlDeferredEventHandler{nodes[id].get(), k, vaml::intern(a.value)});
            }
        }

        // S286 Strand B — deferred {Binding} extensions (the compiled S2 tag).
        // An isBinding attribute is recorded verbatim (path + source retained);
        // the reader does NOT resolve bindings (that is the release consumer /
        // S252 analogue — recorded NO-GAP).
        for (const vaml::tools::AttributeRecord& a : rec.attributes)
        {
            if (a.isBinding || a.isExtension)
            {
                deferred_extensions_.push_back(
                    VBamlDeferredExtension{nodes[id].get(), vaml::intern(a.key),
                                           vaml::intern(a.value)});
            }
        }

        // S285 (unchanged) — plain literal props (non-binding, non-extension,
        // non-directive; a real DP). Directive keys are consumed above; each
        // REGISTERED event key is consumed in Strand C (→ DeferredEventHandler,
        // never a property set / unknown-property throw); an unrecognized
        // non-empty key is an UnknownProperty authoring error.
        for (const vaml::tools::AttributeRecord& a : rec.attributes)
        {
            if (a.isBinding || a.isExtension)
            {
                continue; // Strand B
            }
            if (a.key.empty() || a.key.rfind("x:", 0) == 0)
            {
                continue; // Strand A directives (consumed above)
            }
            const std::string_view k = vaml::intern(a.key);
            const bool isEvent = std::find(event_names_.begin(), event_names_.end(), k) !=
                                 event_names_.end();
            if (isEvent)
            {
                continue; // Strand C (already emitted a DeferredEventHandler)
            }
            const vaml::DependencyProperty* dp =
                vaml::DependencyProperty::ResolveForElement(a.key, rec.typeToken);
            if (dp == nullptr)
            {
                throw VBamlException(VBamlError::UnknownProperty,
                                     "unknown property '" + a.key + "'", id, a.key);
            }
            try
            {
                // S643a/S643b — a Brush-typed paint DP (Brush OBJECT — S643b-1)
                // and a legacy BrushId DP both route through the brush converter;
                // all other bound types use the unchanged generic converter.
                const vaml::ValueType bt = dp->metadata().boundType();
                const vaml::Value v =
                    (bt == vaml::ValueType::Brush || bt == vaml::ValueType::BrushId)
                        ? convertBrushIdLiteral(nodes[id].get(), bt, a.value)
                        : convertLiterals(*dp, a.value);
                (void)nodes[id]->Set(dp->property_id(), v);
            }
            catch (const vaml::ValueConversionException& e)
            {
                throw VBamlException(VBamlError::MalformedValue, e.what(), id);
            }
        }
    }

    // S643a — flush every literal-color brush recorded on the BrushId DPs
    // (after all elements are instantiated + their plain literals set; the
    // S7 reader never authors <X.Resources>, so no authored-dict clobber risk
    // exists here — it creates + attaches a per-element dict, or merges into an
    // existing one for parity with the S2 reader).
    flush_pending_brushes();

    // Pass 2: parent-link children (S039/S041 set_parent) — NON-OWNING parent
    // links only. Ownership stays unified: the root goes to the caller, every
    // non-root child stays owned here in `nodes` until the S440
    // `finalize_visual_tree()` hand-off (below) transfers UIElements into their
    // parents' S121 visual-children lists for rendering. Pre-S440 consumers
    // (parse/structure/introspection tests) call neither finalize and see the
    // original owning retained_ set.
    for (std::size_t id = 0; id < prog.elements.size(); ++id)
    {
        const vaml::tools::ElementRecord& rec = prog.elements[id];
        for (std::size_t cid : rec.children)
        {
            if (cid >= nodes.size() || !nodes[cid])
            {
                throw VBamlException(VBamlError::MalformedBuffer,
                                     "element references an out-of-range child", id);
            }
            if (!nodes[id])
            {
                continue;
            }
            nodes[cid]->set_parent(nodes[id].get());

            // S452 — an OBJECT-valued PROPERTY-ELEMENT child (the S366 NO-GAP
            // hand-off, RESOLVED): `cid` is NOT a visual child; it is the value
            // of the parent's named DP. Set the parent's DP to this child via
            // Value::UntypedPtr (the S2 text reader's object property-element),
            // and mark the child so finalize_visual_tree() skips it (it must
            // NOT be transferred into the S121 visual children).
            if (!prog.elements[cid].propertyElementTarget.empty())
            {
                const std::string& prop = prog.elements[cid].propertyElementTarget;
                const vaml::DependencyProperty* dp =
                    vaml::DependencyProperty::ResolveForElement(prop, prog.elements[id].typeToken);
                if (dp == nullptr)
                {
                    throw VBamlException(VBamlError::UnknownProperty,
                                         "unknown property '" + prop +
                                             "' (object property-element)",
                                         cid, prop);
                }
                (void)nodes[id]->Set(dp->property_id(),
                                     vaml::Value::UntypedPtr(nodes[cid].get()));
                property_element_children_.insert(nodes[cid].get());
            }
        }
    }

    // Pass 3: retained_ = every non-root, in POST-ORDER (children before
    // parent — the S285 retained_objects() contract, and the S2 text reader's
    // retained_objects() order, so nested children precede their container).
    // First collect the post-order id sequence, then move nodes OUT by those
    // ids (children are never shared across parents, so each id moves once).
    // This is the D1/S091 OWNING model the pre-S440 parse tests rely on; S440
    // `finalize_visual_tree()` (below) is the explicit hand-off that later lets
    // the S121 visual tree own those children for rendering.
    std::vector<std::size_t> postOrder;
    {
        std::vector<std::size_t> order;
        order.reserve(prog.elements.size());
        std::function<void(std::size_t)> dfs = [&](std::size_t id) {
            for (std::size_t cid : prog.elements[id].children)
            {
                if (cid < prog.elements.size())
                {
                    dfs(cid);
                }
            }
            order.push_back(id);
        };
        dfs(0);
        postOrder = std::move(order);
    }

    for (std::size_t pid = 0; pid < postOrder.size(); ++pid)
    {
        const std::size_t id = postOrder[pid];
        if (id != 0 && id < nodes.size() && nodes[id])
        {
            retained_.push_back(std::move(nodes[id]));
        }
    }

    // The root is owned by the caller; the rest by this reader (until S440
    // finalize_visual_tree() transfers them into the S121 tree).
    return std::move(nodes[0]);
}

// ---------------------------------------------------------------------------
// S440 — the S120/S121 visual-tree hand-off (parity with VamlReader).
// Closes the roadmap-recorded NO-GAP "children transfer into the UIElement
// visual tree at S120/S121". See the header.
// ---------------------------------------------------------------------------
std::size_t VBamlReader::finalize_visual_tree()
{
    if (visual_finalized_)
    {
        return 0; // idempotent
    }

    std::size_t transferred = 0;

    // Transfer each owned (non-root) child that is BOTH a UIElement AND owns a
    // UIElement parent into the parent's S121 visual-children list. The parent
    // link was seeded during Pass 2 (set_parent). Because add_child requires a
    // nullptr parent (it re-links), clear the seed then attach.
    //
    // Nested-parent correctness: a parent that is ITSELF a retained child must
    // be attached to ITS parent before (or at least as transferable as) its own
    // children are. We do repeat passes — on each pass transfer any child whose
    // parent is NOT still owned by `retained_` (the parent is the caller's root
    // or was already transferred), until a pass makes no progress. This yields
    // a bottom-up transfer (children before their own parents) while preserving
    // each parent's own document order (a parent is only finalized once — on
    // the pass right after all its children were done).
    for (;;)
    {
        bool progress = false;
        for (std::size_t i = 0; i < retained_.size(); )
        {
            std::unique_ptr<vaml::DependencyObject>& slot = retained_[i];
            vaml::UIElement* childUI = dynamic_cast<vaml::UIElement*>(slot.get());
            vaml::UIElement* parentUI =
                dynamic_cast<vaml::UIElement*>(slot->parent());
            if (childUI == nullptr || parentUI == nullptr)
            {
                ++i; // not transferable (non-UIElement child/parent)
                continue;
            }
            // S452 — an OBJECT-VALUED PROPERTY-ELEMENT child is NOT a visual
            // child: it was already set as the parent's DP value (Value::
            // UntypedPtr) in Pass 2. Skip it here so it stays OUT of the S121
            // visual-children list (matching the S2 text reader's property-
            // element, which never auto-attaches as a render child at S452).
            if (property_element_children_.find(slot.get()) !=
                property_element_children_.end())
            {
                ++i;
                continue;
            }

            // Parent still owned by retained_ (not yet finalized)?
            bool parentStillOwned = false;
            for (std::size_t j = 0; j < retained_.size(); ++j)
            {
                if (retained_[j].get() == static_cast<vaml::DependencyObject*>(parentUI))
                {
                    parentStillOwned = true;
                    break;
                }
            }
            if (parentStillOwned)
            {
                ++i; // wait for the parent's own attach first
                continue;
            }

            // Finalize: clear the S039 seed, then add_child (re-links + owns).
            childUI->set_parent(nullptr);
            parentUI->add_child(
                std::unique_ptr<vaml::UIElement>(static_cast<vaml::UIElement*>(slot.release())));
            retained_.erase(retained_.begin() + static_cast<std::ptrdiff_t>(i));
            ++transferred;
            progress = true;
            // do not advance i (element removed)
        }

        if (!progress)
        {
            break;
        }
    }

    // Any child still owned here is NOT transferable into an S121 list (a
    // non-UIElement child, or a UIElement under a non-UIElement parent). These
    // stay reader-owned; never silent.
    visual_finalized_ = true;
    return transferred;
}

// ---------------------------------------------------------------------------
// S285 scope: root_class() is defined inline in the header (S092-analogue).
// The S286 strands are DECLARED in the header and wired by the S286 step; the
// S285 delivery exposes the tree + literal props.
// ---------------------------------------------------------------------------

vaml::DependencyObject* VBamlReader::named(std::string_view name) const noexcept
{
    for (const auto& n : names_)
    {
        if (n.first == name)
        {
            return n.second;
        }
    }
    return nullptr;
}

std::size_t VBamlReader::name_count() const noexcept
{
    return names_.size();
}

const std::vector<std::pair<std::string_view, vaml::DependencyObject*>>&
VBamlReader::names() const noexcept
{
    return names_;
}

const std::vector<VBamlDeferredExtension>&
VBamlReader::deferred_extensions() const noexcept
{
    return deferred_extensions_;
}

void VBamlReader::register_event(std::string_view eventName)
{
    event_names_.push_back(std::string(eventName));
}

const std::vector<VBamlDeferredEventHandler>&
VBamlReader::deferred_event_handlers() const noexcept
{
    return deferred_event_handlers_;
}

// ---------------------------------------------------------------------------
// S289 — Pack/identity + InitializeComponent (the compiled-release path).
// ---------------------------------------------------------------------------

bool VBamlReader::SetOwner(vaml::DependencyObject* owner)
{
    if (owner == nullptr)
    {
        return false;
    }
    owner_ = owner;
    return true;
}

std::unique_ptr<vaml::DependencyObject>
VBamlReader::InitializeComponent(vaml::DependencyObject* owner,
                                 const std::vector<std::uint8_t>& bytes)
{
    if (owner == nullptr)
    {
        throw VBamlException(VBamlError::OwnerClassMismatch,
                             "InitializeComponent requires a non-null x:Class owner",
                             kNoElement);
    }
    // Load(bytes) — the S285 build — then bind the owner.
    std::unique_ptr<vaml::DependencyObject> root = Load(bytes);
    owner_ = owner;

    // The CLASS OWNER MAPPING: when the app registered x:Class owner names, the
    // decoded root_class() MUST be one of them (footprint-consistent with the
    // bound owner's declared class). When the set is EMPTY (no mapping was
    // registered), the check is skipped (a bare pack renders without an owner
    // class — the S444 app-shell always registers its class, so release is
    // always gated; a dev-tool bare load is not).
    if (!class_names_.empty())
    {
        const std::string& rc = root_class();
        if (rc.empty())
        {
            throw VBamlException(VBamlError::OwnerClassMismatch,
                                 "decoded .vbaml has no x:Class; cannot bind to an owner class",
                                 kNoElement);
        }
        bool found = false;
        for (const std::string& cn : class_names_)
        {
            if (cn == rc)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            throw VBamlException(VBamlError::OwnerClassMismatch,
                                 "x:Class '" + rc + "' not among the pack-owner registered classes",
                                 kNoElement, rc);
        }
    }
    return root;
}

std::unique_ptr<vaml::DependencyObject>
VBamlReader::InitializeComponentFrom(vaml::DependencyObject* owner,
                                     pack::ResourcePack& pack,
                                     std::string_view packUri)
{
    // THE COMPILED-ONLY RELEASE ENTRY. Resolve the pack identity → bytes (no
    // text of any kind reachable). A bad identity or an unregistered resource
    // is a structured, never-silent thrown error.
    const std::span<const std::uint8_t> bytes = pack::resolve_pack_uri(pack, packUri);
    if (bytes.empty())
    {
        throw VBamlException(VBamlError::MalformedBuffer,
                             "pack URI did not resolve to a registered compiled resource",
                             kNoElement, std::string(packUri));
    }
    // Copy the resolved bytes into the reader's Load(vector) input (Load takes
    // a std::vector by value — a span-backed copy is the honest interface).
    std::vector<std::uint8_t> owned(bytes.begin(), bytes.end());
    return InitializeComponent(owner, owned);
}

void VBamlReader::register_class(std::string_view className)
{
    for (const std::string& cn : class_names_)
    {
        if (cn == className)
        {
            return;   // idempotent
        }
    }
    class_names_.push_back(std::string(className));
}

bool VBamlReader::is_registered_class(std::string_view className) const noexcept
{
    for (const std::string& cn : class_names_)
    {
        if (cn == className)
        {
            return true;
        }
    }
    return false;
}

} // namespace vaml::tools
