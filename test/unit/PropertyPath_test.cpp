// S240 â€” PropertyPath parser + evaluator (dep:S226). Sprint 6 first token.
//
// Parse suite (Class A syntax): valid dot-paths ("Title", "Address.Street.City",
// "Items[0].Name", "Items[-1]", "Items[\"key\"]", "['key']", "[0]" root, mixed);
// every syntax error throws PropertyPathException with the right kind + a what()
// that names the offending fragment; try_parse never throws + sets the kind.
//
// Evaluator suite (Class B data outcomes), against a test-local indexable host
// (TestRecord/TestList behind UntypedPtr) for the indexer axis and real
// DependencyObjects (via dependency_object_reader) for the property + null-safe
// + missing/not-readable axis:
//   final Null -> ok=true/Value::Null; intermediate Null -> NullIntermediate at
//   the right segmentIndex; unknown property -> MissingProperty; Index on a
//   DependencyObject -> NotReadable; non-object root -> NotReadable; full chain
//   resolves; indexer chain ([0].Name) resolves via the test-local reader.
//
// S240 tests register S240-unique property names (no global-name pollution).

#include "doctest/doctest.h"

#include "vaml_test_stringmakers.h"       // StringMaker + operator<< for Value/enums
#include <vaml/assert/Shouldly.h>         // ShouldlyString / ShouldThrow (SHOULD seam)

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyKey.h"
#include "vaml/core/PropertyPath.h"

#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using vaml::DependencyObject;
using vaml::PropertyFlags;
using vaml::PropertyKey;
using vaml::PropertyMetadata;
using vaml::PropertyPath;
using vaml::PropertyPathError;
using vaml::PropertyPathException;
using vaml::PropertyRead;
using vaml::PropertyReader;
using vaml::PropertySegment;
using vaml::PropertySegmentKind;
using vaml::Value;
using vaml::ValueType;
using vaml::test::ShouldThrow;

namespace {

// Returns the kind thrown by PropertyPath::parse(text). Throws std::runtime_error
// if the text parses successfully (i.e. the caller mis-classified a valid path).
PropertyPathError kind_of(const char* text)
{
    try
    {
        (void)PropertyPath::parse(text);
    }
    catch (const PropertyPathException& e)
    {
        return e.kind();
    }
    throw std::runtime_error(std::string("S240: expected parse to throw for '") + text + "'");
}

int kind_int(PropertyPathError k) { return static_cast<int>(k); }

// The evaluator reads a `Value` node. A `TestRecord` (prop name -> Value) holds a
// property-accessible node; a `TestList` (indexable vector of Values) holds an
// index-accessible node. Both are transported as Value::UntypedPtr and the
// reader dispatches on the segment kind (index -> TestList, property -> TestRecord).
using TestRecord = std::map<std::string, Value>;
struct TestList
{
    std::vector<Value> items;
};

PropertyReader test_reader()
{
    return [](const Value& node, const PropertySegment& seg) -> PropertyRead {
        if (node.type() != ValueType::UntypedPtr)
        {
            return PropertyRead{false, PropertyPathError::NotReadable, Value::Null(), 0};
        }
        void* raw = node.get<void*>();
        if (seg.kind == PropertySegmentKind::Index)
        {
            auto* list = static_cast<TestList*>(raw);
            if (seg.index.kind != vaml::PropertyIndex::Kind::Integer)
            {
                return PropertyRead{false, PropertyPathError::MalformedIndex, Value::Null(), 0};
            }
            std::int64_t idx = seg.index.integer;
            if (idx < 0)
            {
                idx = static_cast<std::int64_t>(list->items.size()) + idx; // from-end
            }
            if (idx < 0 || static_cast<std::size_t>(idx) >= list->items.size())
            {
                return PropertyRead{false, PropertyPathError::NotReadable, Value::Null(), 0};
            }
            return PropertyRead{true, PropertyPathError::Empty, list->items[static_cast<std::size_t>(idx)], 0};
        }
        auto* rec = static_cast<TestRecord*>(raw);
        const auto it = rec->find(seg.name);
        if (it == rec->end())
        {
            return PropertyRead{false, PropertyPathError::MissingProperty, Value::Null(), 0};
        }
        return PropertyRead{true, PropertyPathError::Empty, it->second, 0};
    };
}

// ---------- Parse-suite helpers -------------------------------------------
void expect_valid(const char* text, std::size_t expectedSegments)
{
    PropertyPath p = PropertyPath::parse(text);
    SHOULD(ShouldlyIntegral(p.valid()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(p.segment_count()).ShouldBe(expectedSegments));
    SHOULD(ShouldlyString(p.original()).ShouldBe(text));
}

void expect_kind(const char* text, PropertyPathError expected)
{
    SHOULD(ShouldlyIntegral(kind_int(kind_of(text))).ShouldBe(kind_int(expected)));
}

} // namespace

// ===========================================================================
// PARSE SUITE (Class A â€” syntax)
// ===========================================================================

TEST_CASE("S240 parse: valid single-step and dotted paths")
{
    expect_valid("S240Title", 1);
    expect_valid("S240Street.S240City", 2);
    expect_valid("S240Street.S240Title.S240City", 3);
}

TEST_CASE("S240 parse: valid indexer and mixed paths")
{
    expect_valid("[0]", 1);
    expect_valid("[0].S240Name", 2);
    expect_valid("Items[0].S240Name", 3);
    expect_valid("Items[-1]", 2);
    expect_valid("Items[\"key\"]", 2);
    expect_valid("['key']", 1);
    expect_valid("Items[0].S240Name[2]", 4);
}

TEST_CASE("S240 parse: segment kinds and index payloads")
{
    PropertyPath p = PropertyPath::parse("Items[2].S240Name['k']");
    SHOULD(ShouldlyIntegral(p.segment_count()).ShouldBe(4));

    // Leading property step "Items", then an Integer index, then a property
    // step, then a StringKey index.
    const auto& seg0 = p.segments()[0];
    SHOULD(ShouldlyIntegral(static_cast<int>(seg0.kind)).ShouldBe(static_cast<int>(PropertySegmentKind::Property)));
    SHOULD(ShouldlyString(seg0.name).ShouldBe("Items"));

    const auto& seg1 = p.segments()[1];
    SHOULD(ShouldlyIntegral(static_cast<int>(seg1.kind)).ShouldBe(static_cast<int>(PropertySegmentKind::Index)));
    SHOULD(ShouldlyIntegral(static_cast<int>(seg1.index.kind)).ShouldBe(static_cast<int>(vaml::PropertyIndex::Kind::Integer)));
    SHOULD(ShouldlyIntegral(seg1.index.integer).ShouldBe(2));
    SHOULD(ShouldlyString(seg1.source).ShouldBe("[2]"));

    const auto& seg2 = p.segments()[2];
    SHOULD(ShouldlyIntegral(static_cast<int>(seg2.kind)).ShouldBe(static_cast<int>(PropertySegmentKind::Property)));
    SHOULD(ShouldlyString(seg2.name).ShouldBe("S240Name"));
    // Sprint 11af — AddProperty records the WPF SourceValueInfo.name field (the
    // BARE name), NOT an authored `.`-prefixed step text (the pre-11af source
    // carried the leading separator; the new WPF-faithful parser records the
    // name only, PathParser.cs:276-281).
    SHOULD(ShouldlyString(seg2.source).ShouldBe("S240Name"));

    const auto& seg3 = p.segments()[3];
    SHOULD(ShouldlyIntegral(static_cast<int>(seg3.kind)).ShouldBe(static_cast<int>(PropertySegmentKind::Index)));
    SHOULD(ShouldlyIntegral(static_cast<int>(seg3.index.kind)).ShouldBe(static_cast<int>(vaml::PropertyIndex::Kind::StringKey)));
    // Sprint 11af — the value string keeps the AUTHORED characters (the quotes
    // are literal value-string chars; WPF lexes until `,`/`]`, :243-307), so
    // `['k']` yields the string "'k'" (with quotes), not a stripped "k".
    SHOULD(ShouldlyString(seg3.index.str).ShouldBe("'k'"));
}

TEST_CASE("S240 parse: negative integer index keeps its sign")
{
    PropertyPath p = PropertyPath::parse("Items[-1]");
    const auto& seg1 = p.segments()[1];
    SHOULD(ShouldlyIntegral(seg1.index.integer).ShouldBe(-1));
}

TEST_CASE("S240 parse: each syntax error throws PropertyPathException with the right kind")
{
    // Sprint 11af (WPF-faithful, PathParser.cs): an EMPTY / `.` path is a VALID
    // Direct self-binding (PathParser.cs:83-89, :262-270) — NOT an error; `.`
    // and `..` select DrillIn.Never per level (a leading / repeated dot is a
    // legal separator, PathParser.cs:120-136). Unquoted indexer values are legal
    // string keys (PathParser.cs:243-307), so `[abc]`/`[1.5]`/`[-]`/`["abc]`
    // are VALID string-key indexers — NOT MalformedIndex/UnclosedQuote. The
    // surviving Class A errors are the ones the WPF parser actually emits:
    // unmatched bracket (UnterminatedIndexer), empty bracket (EmptyIndexer),
    // a trailing `/` gate with no step (TrailingDot), and an unbalanced `( )`
    // paren in a property name (UnmatchedParen).
    expect_kind("Items[0",   PropertyPathError::UnterminatedIndexer);
    expect_kind("Items[(p)", PropertyPathError::UnterminatedIndexer);
    expect_kind("Items[^]",  PropertyPathError::UnterminatedIndexer);
    expect_kind("Items[]",   PropertyPathError::EmptyIndexer);
    expect_kind("Address/",  PropertyPathError::TrailingDot);
    expect_kind("Items[0]]", PropertyPathError::TrailingDot);
    expect_kind("(Name",     PropertyPathError::UnmatchedParen);
    expect_kind("A(",        PropertyPathError::UnmatchedParen);
    expect_kind("A)",        PropertyPathError::UnmatchedParen);
}

TEST_CASE("S240 parse: the exception carries the path and a non-empty diagnostic")
{
    std::string path;
    std::string what;
    PropertyPathError kind = PropertyPathError::Empty;
    try
    {
        (void)PropertyPath::parse("Items[0");
    }
    catch (const PropertyPathException& e)
    {
        path = e.path();
        what = e.what();
        kind = e.kind();
    }
    SHOULD(ShouldlyString(path).ShouldBe("Items[0"));
    SHOULD(ShouldlyIntegral(kind_int(kind)).ShouldBe(kind_int(PropertyPathError::UnterminatedIndexer)));
    SHOULD(ShouldlyIntegral(what.empty()).ShouldBe(false));
    SHOULD(ShouldlyIntegral(what.find("UnterminatedIndexer") != std::string::npos || what.find("unterminated") != std::string::npos).ShouldBe(true));
}

TEST_CASE("S240 parse: empty / dot paths are VALID Direct self-bindings (WPF PathParser.cs:83-89)")
{
    // Sprint 11af — empty, whitespace-only, and `.` are a Direct binding to the
    // whole object, NOT syntax errors (the pre-11af 'Empty' Class A error is
    // retired). try_parse succeeds and the path reports is_direct().
    {
        PropertyPath p; PropertyPathError err = PropertyPathError::Empty;
        SHOULD(ShouldlyIntegral(PropertyPath::try_parse("", p, &err)).ShouldBe(true));
        SHOULD(ShouldlyIntegral(p.valid()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(p.is_direct()).ShouldBe(true));
    }
    {
        PropertyPath p; PropertyPathError err = PropertyPathError::Empty;
        SHOULD(ShouldlyIntegral(PropertyPath::try_parse("   ", p, &err)).ShouldBe(true));
        SHOULD(ShouldlyIntegral(p.is_direct()).ShouldBe(true));
    }
    {
        PropertyPath p = PropertyPath::parse(".");
        SHOULD(ShouldlyIntegral(p.valid()).ShouldBe(true));
        SHOULD(ShouldlyIntegral(p.is_direct()).ShouldBe(true));
    }
}

TEST_CASE("S240 parse: dot / double-dot separators are legal DrillIn (not errors)")
{
    // Sprint 11af — leading `.` and repeated `.` are legal per-level drill-in
    // separators (WPF DrillIn.Never, PathParser.cs:120-136, AddProperty :262).
    // A trailing `.` with no following step is a single property step.
    PropertyPath p1 = PropertyPath::parse(".Address");
    SHOULD(ShouldlyIntegral(p1.valid()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(p1.segment_count()).ShouldBe(1));
    SHOULD(ShouldlyString(p1.segments()[0].name).ShouldBe("Address"));

    PropertyPath p2 = PropertyPath::parse("Address..Street");
    SHOULD(ShouldlyIntegral(p2.valid()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(p2.segment_count()).ShouldBe(2));
    SHOULD(ShouldlyString(p2.segments()[0].name).ShouldBe("Address"));
    SHOULD(ShouldlyString(p2.segments()[1].name).ShouldBe("Street"));

    PropertyPath p3 = PropertyPath::parse("Address.");
    SHOULD(ShouldlyIntegral(p3.valid()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(p3.segment_count()).ShouldBe(1));
    SHOULD(ShouldlyString(p3.segments()[0].name).ShouldBe("Address"));
}

TEST_CASE("S240 parse: unquoted indexer values are valid string keys (WPF PathParser.cs:243-307)")
{
    // WPF value-string lexing: any chars until `,` or `]` are a value; an
    // unquoted run (incl. '.', '-', '"') is a legal StringKey indexer.
    PropertyPath pa = PropertyPath::parse("Items[abc]");
    SHOULD(ShouldlyIntegral(pa.valid()).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(pa.segments()[1].index.kind)).ShouldBe(static_cast<int>(vaml::PropertyIndex::Kind::StringKey)));
    SHOULD(ShouldlyString(pa.segments()[1].index.str).ShouldBe("abc"));

    PropertyPath pb = PropertyPath::parse("Items[1.5]");
    SHOULD(ShouldlyIntegral(static_cast<int>(pb.segments()[1].index.kind)).ShouldBe(static_cast<int>(vaml::PropertyIndex::Kind::StringKey)));
    SHOULD(ShouldlyString(pb.segments()[1].index.str).ShouldBe("1.5"));

    PropertyPath pc = PropertyPath::parse("Items[\"abc]");
    SHOULD(ShouldlyIntegral(static_cast<int>(pc.segments()[1].index.kind)).ShouldBe(static_cast<int>(vaml::PropertyIndex::Kind::StringKey)));
    SHOULD(ShouldlyString(pc.segments()[1].index.str).ShouldBe("\"abc"));
}

TEST_CASE("S240 parse: try_parse never throws, sets the kind, leaves out untouched on error")
{
    PropertyPath ok;
    PropertyPathError err = PropertyPathError::Empty;
    SHOULD(ShouldlyIntegral(PropertyPath::try_parse("S240Title", ok, &err)).ShouldBe(true));
    SHOULD(ShouldlyIntegral(ok.segment_count()).ShouldBe(1));

    PropertyPath untouched;
    err = PropertyPathError::Empty;
    SHOULD(ShouldlyIntegral(PropertyPath::try_parse("Items[0", untouched, &err)).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(err)).ShouldBe(kind_int(PropertyPathError::UnterminatedIndexer)));
    SHOULD(ShouldlyIntegral(untouched.valid()).ShouldBe(false));
}

TEST_CASE("S240 parse: parse() throws PropertyPathException (typed, catchable)")
{
    SHOULD(ShouldThrow<PropertyPathException>([] {
        (void)PropertyPath::parse("Items[0");
    }));
}

// ===========================================================================
// EVALUATOR SUITE (Class B â€” data outcomes)
// ===========================================================================

TEST_CASE("S240 eval: full property chain resolves via the test reader")
{
    TestRecord street;
    street.emplace("S240Name", Value::String("Main"));
    TestRecord city;
    city.emplace("S240Street", Value::UntypedPtr(&street));

    Value root = Value::UntypedPtr(&city);
    PropertyPath p = PropertyPath::parse("S240Street.S240Name");
    PropertyRead r = vaml::evaluate_property_path(root, p, test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::String("Main")));
}

TEST_CASE("S240 eval: indexer chain ([0].Name) resolves via the test reader")
{
    TestRecord item0;
    item0.emplace("S240Name", Value::String("zero"));
    TestRecord item1;
    item1.emplace("S240Name", Value::String("one"));
    TestList list;
    list.items.push_back(Value::UntypedPtr(&item0));
    list.items.push_back(Value::UntypedPtr(&item1));

    Value root = Value::UntypedPtr(&list);
    PropertyPath p = PropertyPath::parse("[1].S240Name");
    PropertyRead r = vaml::evaluate_property_path(root, p, test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::String("one")));
}

TEST_CASE("S240 eval: indexer from-end (-1) resolves")
{
    TestRecord item0;
    item0.emplace("S240Name", Value::String("zero"));
    TestList list;
    list.items.push_back(Value::UntypedPtr(&item0));
    list.items.push_back(Value::UntypedPtr(&item0));

    Value root = Value::UntypedPtr(&list);
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse("[-1].S240Name"), test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::String("zero")));
}

TEST_CASE("S240 eval: final null is a valid resolved result (ok=true)")
{
    TestRecord rec;
    rec.emplace("S240Empty", Value::Null());
    Value root = Value::UntypedPtr(&rec);
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse("S240Empty"), test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value.isNull()).ShouldBe(true));
}

TEST_CASE("S240 eval: intermediate null -> NullIntermediate at the right segmentIndex")
{
    TestRecord rec;
    rec.emplace("S240Empty", Value::Null()); // resolved as a null on a property step
    TestList list;
    list.items.push_back(Value::UntypedPtr(&rec));
    Value root = Value::UntypedPtr(&list);

    // "[0].S240Empty.S240Name": segment0 = index (ok), segment1 = S240Empty -> Null
    // (non-final) -> NullIntermediate at segmentIndex 1.
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse("[0].S240Empty.S240Name"), test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(r.error)).ShouldBe(kind_int(PropertyPathError::NullIntermediate)));
    SHOULD(ShouldlyIntegral(r.segmentIndex).ShouldBe(1));
}

TEST_CASE("S240 eval: missing property propagates MissingProperty with its segmentIndex")
{
    TestRecord rec;
    Value root = Value::UntypedPtr(&rec);
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse("Nope.S240Name"), test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(r.error)).ShouldBe(kind_int(PropertyPathError::MissingProperty)));
    SHOULD(ShouldlyIntegral(r.segmentIndex).ShouldBe(0));
}

TEST_CASE("S240 eval: empty/`.` Direct path binds the whole object (WPF PathParser.cs:83-89, :262-270)")
{
    // Sprint 11af — an empty / `.` path is a VALID Direct self-binding, so the
    // evaluator returns ok=true with the ROOT value (the WPF
    // SourceValueInfo(Direct) result), NOT an Empty error.
    TestRecord rec;
    rec.emplace("S240Title", Value::String("whole"));
    Value root = Value::UntypedPtr(&rec);

    PropertyPath p; PropertyPathError err;
    (void)PropertyPath::try_parse("", p, &err);
    SHOULD(ShouldlyIntegral(p.is_direct()).ShouldBe(true));
    PropertyRead r = vaml::evaluate_property_path(root, p, test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyIntegral(static_cast<int>(r.error)).ShouldBe(static_cast<int>(PropertyPathError::Empty))); // the no-error tag
    // The Direct path binds the WHOLE object root (an UntypedPtr transport).
    SHOULD(ShouldlyIntegral(static_cast<int>(r.value.type())).ShouldBe(static_cast<int>(ValueType::UntypedPtr)));
}
