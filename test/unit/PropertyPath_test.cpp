// S240 — PropertyPath parser + evaluator (dep:S226). Sprint 6 first token.
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

// S240-unique property names (object-scoped; no global-name collision).
const char* kNameTitle = "S240Title";
const char* kNameEmpty = "S240Empty";

PropertyKey key(const std::string& name, ValueType t)
{
    return PropertyKey(name, t, "S240Test", PropertyFlags::None);
}

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
// PARSE SUITE (Class A — syntax)
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
    SHOULD(ShouldlyString(seg2.source).ShouldBe(".S240Name"));

    const auto& seg3 = p.segments()[3];
    SHOULD(ShouldlyIntegral(static_cast<int>(seg3.kind)).ShouldBe(static_cast<int>(PropertySegmentKind::Index)));
    SHOULD(ShouldlyIntegral(static_cast<int>(seg3.index.kind)).ShouldBe(static_cast<int>(vaml::PropertyIndex::Kind::StringKey)));
    SHOULD(ShouldlyString(seg3.index.str).ShouldBe("k"));
}

TEST_CASE("S240 parse: negative integer index keeps its sign")
{
    PropertyPath p = PropertyPath::parse("Items[-1]");
    const auto& seg1 = p.segments()[1];
    SHOULD(ShouldlyIntegral(seg1.index.integer).ShouldBe(-1));
}

TEST_CASE("S240 parse: each syntax error throws PropertyPathException with the right kind")
{
    expect_kind("", PropertyPathError::Empty);
    expect_kind("   ", PropertyPathError::Empty);
    expect_kind("Address.", PropertyPathError::TrailingDot);
    expect_kind(".Address", PropertyPathError::TrailingDot);
    expect_kind("Address..Street", PropertyPathError::ConsecutiveDot);
    expect_kind("Items[0", PropertyPathError::UnterminatedIndexer);
    expect_kind("Items[]", PropertyPathError::EmptyIndexer);
    expect_kind("Items[abc]", PropertyPathError::MalformedIndex);
    expect_kind("Items[1.5]", PropertyPathError::MalformedIndex);
    expect_kind("Items[-]", PropertyPathError::MalformedIndex);
    expect_kind("Items[\"abc]", PropertyPathError::UnclosedQuote);
}

TEST_CASE("S240 parse: the exception carries the path and a non-empty diagnostic")
{
    std::string path;
    std::string what;
    PropertyPathError kind = PropertyPathError::Empty;
    try
    {
        (void)PropertyPath::parse("Address..Street");
    }
    catch (const PropertyPathException& e)
    {
        path = e.path();
        what = e.what();
        kind = e.kind();
    }
    SHOULD(ShouldlyString(path).ShouldBe("Address..Street"));
    SHOULD(ShouldlyIntegral(kind_int(kind)).ShouldBe(kind_int(PropertyPathError::ConsecutiveDot)));
    SHOULD(ShouldlyIntegral(what.empty()).ShouldBe(false));
    SHOULD(ShouldlyIntegral(what.find("ConsecutiveDot") != std::string::npos || what.find("consecutive") != std::string::npos).ShouldBe(true));
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
        (void)PropertyPath::parse("Address.");
    }));
}

// ===========================================================================
// EVALUATOR SUITE (Class B — data outcomes)
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

TEST_CASE("S240 eval: empty/invalid path -> Empty")
{
    TestRecord rec;
    Value root = Value::UntypedPtr(&rec);
    PropertyPath p;
    PropertyPathError err;
    (void)PropertyPath::try_parse("", p, &err);
    PropertyRead r = vaml::evaluate_property_path(root, p, test_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(r.error)).ShouldBe(kind_int(PropertyPathError::Empty)));
}

// ===========================================================================
// EVALUATOR SUITE against the DEFAULT reader (DependencyObjects)
// ===========================================================================

TEST_CASE("S240 eval default reader: resolves a registered property on a DependencyObject")
{
    DependencyObject obj;
    obj.register_property(key(kNameTitle, ValueType::String),
        PropertyMetadata::WithDefault(Value::String("hello"), PropertyFlags::None));
    (void)obj.Set(vaml::PropertyId::of(kNameTitle), Value::String("world"));

    Value root = Value::UntypedPtr(&obj);
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse(kNameTitle), vaml::dependency_object_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(true));
    SHOULD(ShouldlyString(r.value).ShouldBe(Value::String("world")));
}

TEST_CASE("S240 eval default reader: unknown property -> MissingProperty (never a raw throw)")
{
    DependencyObject obj;
    Value root = Value::UntypedPtr(&obj);
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse("S240Nope"), vaml::dependency_object_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(r.error)).ShouldBe(kind_int(PropertyPathError::MissingProperty)));
}

TEST_CASE("S240 eval default reader: Index on a DependencyObject -> NotReadable")
{
    DependencyObject obj;
    Value root = Value::UntypedPtr(&obj);
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse("[0]"), vaml::dependency_object_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(r.error)).ShouldBe(kind_int(PropertyPathError::NotReadable)));
}

TEST_CASE("S240 eval default reader: non-object root -> NotReadable")
{
    Value root = Value::Int32(5);
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse("S240Title"), vaml::dependency_object_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(r.error)).ShouldBe(kind_int(PropertyPathError::NotReadable)));
}

TEST_CASE("S240 eval default reader: intermediate null on DependencyObject -> NullIntermediate")
{
    DependencyObject obj;
    // A property whose default (and value) is Value::Null.
    obj.register_property(key(kNameEmpty, ValueType::Null),
        PropertyMetadata::WithDefault(Value::Null(), PropertyFlags::None));

    Value root = Value::UntypedPtr(&obj);
    // "S240Empty.Foo": segment0 = S240Empty -> Get returns Null (non-final) ->
    // NullIntermediate at segmentIndex 0.
    PropertyRead r = vaml::evaluate_property_path(root, PropertyPath::parse(std::string(kNameEmpty) + ".Foo"), vaml::dependency_object_reader());
    SHOULD(ShouldlyIntegral(r.ok).ShouldBe(false));
    SHOULD(ShouldlyIntegral(kind_int(r.error)).ShouldBe(kind_int(PropertyPathError::NullIntermediate)));
    SHOULD(ShouldlyIntegral(r.segmentIndex).ShouldBe(0));
}
