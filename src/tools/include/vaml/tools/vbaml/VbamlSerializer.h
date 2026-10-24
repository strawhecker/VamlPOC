#pragma once
// S282 (dep:S281) — Serializer: writes a compiled program tree to bytes and
// decodes it back EQUAL (byte-identical).
//
// encode_program + decode implement the S281 six-section record set and read
// it back. The S282 round-trip contract:
//   * encode_program(prog, schema) -> bytes
//   * decode(bytes, schema) -> prog'  (structurally EQUAL to prog)
//   * encode_program(prog', schema) -> the SAME bytes (byte-identical)
// Encode is a PURE function of the in-memory model (no timestamps, no
// metadata, no hash-order perturbation) → byte-identical. The schema used to
// encode MUST be the same schema the decoder is handed (it carries the string
// table + token tables).
//
// ---------------------------------------------------------------------------
// WIRE FORMAT (Big-Endian; VarU32 integer encoding, URI-style; stable):
//   magic          u32 BE     = 0x76424D4C ('vBML')
//   version        u16 BE     = 1
//   elementCount   VarU32     (the element-id section)
//   stringCount    VarU32
//   string table:  for i in 0..stringCount-1:
//                      len    VarU32
//                      bytes  len × UTF-8            (string-table section; index == ordinal)
//   typeCount      VarU32
//   type table:    for each: idx VarU32              (type-token section: indexes into string table)
//   propCount      VarU32
//   prop table:    for each: idx VarU32              (prop-token section)
//   eventCount     VarU32
//   event table:   for each: idx VarU32              (event-token section)
//   resourceCount  VarU32
//   resource table:for each: idx VarU32              (resource-token section)
//   elements:      for eid in 0..elementCount-1:     (element-id section records, in id order)
//                      typeToken  VarU32
//                      attrCount  VarU32
//                      for each attr:
//                          propToken VarU32
//                          valueIdx  VarU32
//                          flags     u8   (bit0 isExtension, bit1 isBinding)
//                          if bit1 (binding): pathIdx VarU32, targetPropToken VarU32
//                      childCount VarU32
//                      for each child: childId VarU32
// ---------------------------------------------------------------------------
//
// Every count/length is bounded-checked on decode → VbamlMalformedBuffer on
// any structural violation (bad magic, bad version, truncated buffer, overlong
// VarU32, out-of-range string/type/prop token, out-of-range element id).
// Everything is single-threaded + pure (ADR-018: zero locks).

#include "vaml/tools/Vamlc.h" // CompiledProgram / ElementRecord / AttributeRecord
#include "vaml/tools/vbaml/VbamlSchema.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace vaml::tools::vbaml {

// A decoded buffer is structurally malformed (bad magic/version, truncated/
// overlong/invalid) — every invariant (1) the serializer can itself reject on
// the wire and (2) that `compile` maps into a VamlcException(CompileFailed).
class VAML_TOOLS_EXPORT VbamlMalformedBuffer : public std::runtime_error
{
public:
    explicit VbamlMalformedBuffer(const std::string& what) : std::runtime_error(what) {}
};

// Appends the header + five token/string sections of `schema` (the S281 record
// set). Existing `bytes` content is preserved (append-only). Pure + deterministic.
VAML_TOOLS_EXPORT void encode_schema(const Schema& schema, std::vector<std::uint8_t>& bytes);

// Appends the S282 element records for `prog` (the element-id section, in id
// order) onto `bytes`. Pure + deterministic. `prog` must be well-formed from
// S280; the schema must already have been encoded (encode_schema) so decode
// can correlate.
VAML_TOOLS_EXPORT void encode_program_records(
    const CompiledProgram& prog, const Schema& schema, std::vector<std::uint8_t>& bytes);

// Encodes schema + program as a complete .vbaml byte stream (schema sections
// then element records). The CLI writes exactly these bytes to the .vbaml file.
[[nodiscard]] VAML_TOOLS_EXPORT std::vector<std::uint8_t> encode(
    const CompiledProgram& prog, const Schema& schema);

// Decodes a complete .vbaml byte stream (produced by encode) back into a
// CompiledProgram. Throws VbamlMalformedBuffer on any structural violation.
// The decoded strings are string_views INTO `schema`'s owned string table —
// `schema` must outlive the returned program.
[[nodiscard]] VAML_TOOLS_EXPORT CompiledProgram decode(
    const std::vector<std::uint8_t>& bytes, const Schema& schema);

// Decodes ONLY the schema header + token/string sections from the byte stream,
// reconstructing an immutable Schema. Throws VbamlMalformedBuffer on violation.
[[nodiscard]] VAML_TOOLS_EXPORT Schema decode_schema(const std::vector<std::uint8_t>& bytes);

} // namespace vaml::tools::vbaml
