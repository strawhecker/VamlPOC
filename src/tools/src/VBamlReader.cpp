// S285 (dep:S284) — Runtime VBamlReader: bytes → DependencyObject tree
// (InitializeComponent-style).
//
// Decodes a .vbaml byte stream (S282) and materializes a LIVE DependencyObject
// tree via the S2 registries (S087 type table, S089 value table, S032–S039
// property table) — the compiled-path "S7 reader". Lives in vaml-tools (core
// cannot depend on tools; tools → core only). Single-threaded + pure
// (ADR-018: zero locks); ADR-021 headless-testable.

#include "vaml/tools/VBamlReader.h"

#include "vaml/tools/vbaml/VbamlSchema.h"
#include "vaml/tools/vbaml/VbamlSerializer.h"
#include "vaml/tools/Vamlc.h" // CompiledProgram / ElementRecord / AttributeRecord

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/ElementFactoryRegistry.h"
#include "vaml/core/Intern.h" // S140_1: vaml::intern — process-stable retained string_view
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"
#include "vaml/core/ValueConversionError.h"
#include "vaml/core/ValueConverter.h"

#include <algorithm> // std::find (event-name scan)
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
// converters can produce are represented (Int32/Bool/Color/Thickness); String
// is handled by identity. Returns an empty view for a type with no converter.
[[nodiscard]] std::string_view converterNameFor(vaml::ValueType t) noexcept
{
    using vaml::ValueType;
    switch (t)
    {
        case ValueType::Int32:     return "Int32";
        case ValueType::Bool:      return "Bool";
        case ValueType::Color:     return "Color";
        case ValueType::Thickness: return "Thickness";
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
    root_class_.clear();
    names_.clear();
    deferred_extensions_.clear();
    deferred_event_handlers_.clear();

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
            const vaml::DependencyProperty* dp = vaml::DependencyProperty::TryGet(a.key);
            if (dp == nullptr)
            {
                throw VBamlException(VBamlError::UnknownProperty,
                                     "unknown property '" + a.key + "'", id, a.key);
            }
            try
            {
                (void)nodes[id]->Set(vaml::PropertyId::of(a.key),
                                     convertLiterals(*dp, a.value));
            }
            catch (const vaml::ValueConversionException& e)
            {
                throw VBamlException(VBamlError::MalformedValue, e.what(), id);
            }
        }
    }

    // Pass 2: parent-link children (S039/S041 set_parent).
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
            if (nodes[id])
            {
                nodes[cid]->set_parent(nodes[id].get());
            }
        }
    }

    // Pass 3: retained_ = every non-root, in POST-ORDER (children before
    // parent — the S2 reader's retained_objects() contract, so nested children
    // precede their container). First collect the post-order id sequence, then
    // move nodes OUT by those ids (children are never shared across parents, so
    // each id is moved exactly once).
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

    // The root is owned by the caller; the rest by this reader.
    return std::move(nodes[0]);
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

} // namespace vaml::tools
