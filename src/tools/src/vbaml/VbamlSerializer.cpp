// S282 (dep:S281) — Serializer: writes a compiled program tree to bytes and
// decodes it back EQUAL (byte-identical). Implements the S281 six-section
// record set with a Big-Endian, VarU32, deterministic wire format. Encode and
// decode are PURE functions of the in-memory model (ADR-018: zero locks).
//
// See VbamlSerializer.h for the complete wire-format listing. Byte-identity:
// same program + same schema → same bytes, because nothing (timestamps,
// metadata, hash order) is written beyond the model.

#include "vaml/tools/vbaml/VbamlSerializer.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace vaml::tools::vbaml {

namespace {

// --- VarU32: URI-style, 7 bits per byte, LSB-first, continuation bit 0x80.
// Small values (typical ids/tokens/counts) encode as a single byte. -------
void put_varu32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    while (v >= 0x80)
    {
        out.push_back(static_cast<std::uint8_t>((v & 0x7F) | 0x80));
        v >>= 7;
    }
    out.push_back(static_cast<std::uint8_t>(v));
}

void put_u16(std::vector<std::uint8_t>& out, std::uint16_t v)
{
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v)
{
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
}

// The attribute FLAGS byte: bit0 isExtension, bit1 isBinding.
std::uint8_t attr_flags(const AttributeRecord& a)
{
    return static_cast<std::uint8_t>(
        (a.isExtension ? 0x01 : 0) | (a.isBinding ? 0x02 : 0));
}

// Looks up the string-table index of `needle` in the schema (linear scan —
// deterministic). Throws if the string was never interned (encode precondition).
std::uint32_t schema_index_of(const Schema& s, std::string_view needle)
{
    for (std::size_t i = 0; i < s.strings.size(); ++i)
    {
        if (s.strings[i] == needle)
        {
            return static_cast<std::uint32_t>(i);
        }
    }
    throw VbamlMalformedBuffer("encode: string not interned in schema: '" +
                               std::string(needle) + "'");
}

// A bounds-checked, position-tracked reader over the byte buffer. Every get
// throws VbamlMalformedBuffer on a structural violation.
class Reader
{
public:
    explicit Reader(const std::vector<std::uint8_t>& b) : b_(b) {}

    std::uint32_t u32(const char* what)
    {
        require(4, what);
        const std::uint32_t v = (static_cast<std::uint32_t>(b_[pos_]) << 24) |
                                (static_cast<std::uint32_t>(b_[pos_ + 1]) << 16) |
                                (static_cast<std::uint32_t>(b_[pos_ + 2]) << 8) |
                                (static_cast<std::uint32_t>(b_[pos_ + 3]));
        pos_ += 4;
        return v;
    }

    std::uint16_t u16(const char* what)
    {
        require(2, what);
        const std::uint16_t v = static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(b_[pos_]) << 8) |
            static_cast<std::uint16_t>(b_[pos_ + 1]));
        pos_ += 2;
        return v;
    }

    std::uint32_t varu32(const char* what)
    {
        std::uint32_t v = 0;
        unsigned shift = 0;
        std::size_t n = 0;
        while (n < 5)
        {
            if (pos_ >= b_.size())
            {
                throw VbamlMalformedBuffer(std::string("truncated VarU32 (") + what + ")");
            }
            const std::uint8_t c = b_[pos_++];
            v |= static_cast<std::uint32_t>(c & 0x7F) << shift;
            if ((c & 0x80) == 0)
            {
                return v;
            }
            shift += 7;
            ++n;
        }
        throw VbamlMalformedBuffer(std::string("overlong VarU32 (") + what + ")");
    }

    std::string_view bytes(std::size_t len, const char* what)
    {
        require(len, what);
        const std::string_view v(reinterpret_cast<const char*>(b_.data() + pos_), len);
        pos_ += len;
        return v;
    }

    std::size_t position() const noexcept { return pos_; }

    // Bounds-checks a decoded index against a vector size.
    static std::uint32_t checked(std::uint32_t idx, std::size_t size, const char* what)
    {
        if (idx >= size)
        {
            throw VbamlMalformedBuffer(std::string("out-of-range ") + what + " index: " +
                                       std::to_string(idx));
        }
        return idx;
    }

private:
    const std::vector<std::uint8_t>& b_;
    std::size_t pos_ = 0;

    void require(std::size_t n, const char* what)
    {
        if (pos_ + n > b_.size())
        {
            throw VbamlMalformedBuffer(std::string("truncated ") + what);
        }
    }
};

} // namespace

VAML_TOOLS_EXPORT
void encode_schema(const Schema& s, std::vector<std::uint8_t>& bytes)
{
    put_u32(bytes, s.magic);
    put_u16(bytes, s.version);

    // string-table section
    put_varu32(bytes, static_cast<std::uint32_t>(s.strings.size()));
    for (std::string_view str : s.strings)
    {
        put_varu32(bytes, static_cast<std::uint32_t>(str.size()));
        bytes.insert(bytes.end(), str.begin(), str.end());
    }

    // type-token / prop-token / event-token / resource-token sections
    put_varu32(bytes, static_cast<std::uint32_t>(s.types.size()));
    for (std::uint32_t idx : s.types)
    {
        put_varu32(bytes, idx);
    }
    put_varu32(bytes, static_cast<std::uint32_t>(s.props.size()));
    for (std::uint32_t idx : s.props)
    {
        put_varu32(bytes, idx);
    }
    put_varu32(bytes, static_cast<std::uint32_t>(s.events.size()));
    for (std::uint32_t idx : s.events)
    {
        put_varu32(bytes, idx);
    }
    put_varu32(bytes, static_cast<std::uint32_t>(s.resources.size()));
    for (std::uint32_t idx : s.resources)
    {
        put_varu32(bytes, idx);
    }
}

VAML_TOOLS_EXPORT
void encode_program_records(const CompiledProgram& prog, const Schema& schema,
                            std::vector<std::uint8_t>& bytes)
{
    // element-id section: total element count, then per-element records.
    put_varu32(bytes, static_cast<std::uint32_t>(prog.elements.size()));
    for (const ElementRecord& e : prog.elements)
    {
        put_varu32(bytes, schema_index_of(schema, e.typeToken));

        put_varu32(bytes, static_cast<std::uint32_t>(e.attributes.size()));
        for (const AttributeRecord& a : e.attributes)
        {
            put_varu32(bytes, schema_index_of(schema, a.key));
            put_varu32(bytes, schema_index_of(schema, a.value));
            bytes.push_back(attr_flags(a));
            if (a.isBinding)
            {
                put_varu32(bytes, schema_index_of(schema, a.bindingPath));
                // The S280_1 target prop token (== the attribute key's index);
                // re-emitted for the explicit S280_1 contract.
                put_varu32(bytes, schema_index_of(schema, a.key));
            }
        }

        put_varu32(bytes, static_cast<std::uint32_t>(e.children.size()));
        for (std::size_t c : e.children)
        {
            put_varu32(bytes, static_cast<std::uint32_t>(c));
        }
    }
}

VAML_TOOLS_EXPORT
std::vector<std::uint8_t> encode(const CompiledProgram& prog, const Schema& schema)
{
    std::vector<std::uint8_t> bytes;
    encode_schema(schema, bytes);
    encode_program_records(prog, schema, bytes);
    return bytes;
}

VAML_TOOLS_EXPORT
Schema decode_schema(const std::vector<std::uint8_t>& bytes)
{
    Reader r(bytes);
    const std::uint32_t magic = r.u32("magic");
    if (magic != kVbamlMagic)
    {
        throw VbamlMalformedBuffer("bad .vbaml magic");
    }
    const std::uint16_t version = r.u16("version");
    if (version != kVbamlVersion)
    {
        throw VbamlMalformedBuffer("unsupported .vbaml version");
    }

    Schema s;
    s.magic = magic;
    s.version = version;

    const std::uint32_t stringCount = r.varu32("stringCount");
    s.strings.reserve(stringCount);
    for (std::uint32_t i = 0; i < stringCount; ++i)
    {
        const std::uint32_t len = r.varu32("stringLen");
        s.strings.emplace_back(r.bytes(len, "string bytes"));
    }

    const auto readIndexList = [&](std::vector<std::uint32_t>& out, const char* what) {
        const std::uint32_t count = r.varu32((std::string(what) + "Count").c_str());
        out.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
        {
            out.push_back(Reader::checked(r.varu32(what), s.strings.size(), what));
        }
    };
    readIndexList(s.types, "type");
    readIndexList(s.props, "prop");
    readIndexList(s.events, "event");
    readIndexList(s.resources, "resource");

    return s;
}

VAML_TOOLS_EXPORT
CompiledProgram decode(const std::vector<std::uint8_t>& bytes, const Schema& schema)
{
    // First verify the schema section (magic/version/string+token tables) so
    // the caller's schema agrees with the wire, then locate the element-records
    // boundary by re-encoding the schema (deterministic byte length).
    (void)decode_schema(bytes);
    std::vector<std::uint8_t> header;
    encode_schema(schema, header);
    if (header.size() > bytes.size())
    {
        throw VbamlMalformedBuffer("buffer shorter than schema section");
    }

    // Parse ONLY the element-records section (which follows `header`).
    std::vector<std::uint8_t> body(bytes.begin() + static_cast<std::ptrdiff_t>(header.size()),
                                   bytes.end());
    Reader br(body);

    const std::uint32_t elementCount = br.varu32("elementCount");
    CompiledProgram prog;
    prog.elements.reserve(elementCount);
    for (std::uint32_t eid = 0; eid < elementCount; ++eid)
    {
        ElementRecord e;
        e.id = eid;
        const std::uint32_t typeIdx =
            Reader::checked(br.varu32("typeToken"), schema.strings.size(), "type");
        e.typeToken = std::string(schema.strings[typeIdx]);

        const std::uint32_t attrCount = br.varu32("attrCount");
        e.attributes.reserve(attrCount);
        for (std::uint32_t i = 0; i < attrCount; ++i)
        {
            AttributeRecord a;
            const std::uint32_t keyIdx =
                Reader::checked(br.varu32("keyIdx"), schema.strings.size(), "key");
            a.key = std::string(schema.strings[keyIdx]);
            const std::uint32_t valIdx =
                Reader::checked(br.varu32("valIdx"), schema.strings.size(), "value");
            a.value = std::string(schema.strings[valIdx]);
            if (br.position() + 1 > body.size())
            {
                throw VbamlMalformedBuffer("truncated attribute flags");
            }
            const std::uint8_t flags = body[br.position()];
            // skip the 1-byte raw flag (cannot use varu32)
            (void)br.bytes(1, "flags");
            a.isExtension = (flags & 0x01) != 0;
            a.isBinding = (flags & 0x02) != 0;
            if (a.isBinding)
            {
                const std::uint32_t pathIdx =
                    Reader::checked(br.varu32("pathIdx"), schema.strings.size(), "path");
                a.bindingPath = std::string(schema.strings[pathIdx]);
                // The target prop token (== the attribute key's index) is
                // consumed for structural completeness (S280_1 contract).
                (void)Reader::checked(br.varu32("targetPropIdx"), schema.strings.size(), "targetProp");
            }
            e.attributes.push_back(std::move(a));
        }

        const std::uint32_t childCount = br.varu32("childCount");
        e.children.reserve(childCount);
        for (std::uint32_t i = 0; i < childCount; ++i)
        {
            const std::uint32_t cid =
                Reader::checked(br.varu32("childId"), elementCount, "child");
            e.children.push_back(cid);
        }
        prog.elements.push_back(std::move(e));
    }
    return prog;
}

} // namespace vaml::tools::vbaml
