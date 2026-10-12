// S036_2 — doctest::StringMaker domain exposure for VAML core types.
//
// Proves the specializations in vaml_test_stringmakers.h compile at the same
// /WX bar and that failed assertions now stringify vaml values as readable
// text ("<Int32 42>", "SetResult::APPLIED", "PropertyId(Width)") instead of
// default/garbage output. This is the framework-native "source-tool
// readability" scaffold the re-formulated Shouldly-for-doctest relies on.

#include <doctest/doctest.h>

#include "vaml_test_stringmakers.h"

#include "vaml/core/DependencyObject.h"
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"

using vaml::PropertyId;
using vaml::SetResult;
using vaml::Value;

// StringMaker<Value> produces "<Tag payload>", readable for all payload kinds.
TEST_CASE("S036_2: StringMaker<Value> renders readable, tagged text")
{
    CHECK(doctest::String{doctest::StringMaker<Value>::convert(Value::Int32(42))} ==
          doctest::String{"<Int32 42>"});
    CHECK(doctest::String{doctest::StringMaker<Value>::convert(Value::Bool(true))} ==
          doctest::String{"<Bool true>"});
    CHECK(doctest::String{doctest::StringMaker<Value>::convert(Value::Null())} ==
          doctest::String{"<Null null>"});
    CHECK(doctest::String{doctest::StringMaker<Value>::convert(Value::String("hi"))} ==
          doctest::String{"<String \"hi\">"});
    CHECK(doctest::String{doctest::StringMaker<Value>::convert(Value::Double(1.5))} ==
          doctest::String{"<Double 1.5>"});
}

// StringMaker<SetResult> names the enum member.
TEST_CASE("S036_2: StringMaker<SetResult> names the outcome")
{
    CHECK(doctest::String{doctest::StringMaker<SetResult>::convert(SetResult::APPLIED)} ==
          doctest::String{"SetResult::APPLIED"});
    CHECK(doctest::String{doctest::StringMaker<SetResult>::convert(SetResult::REJECTED)} ==
          doctest::String{"SetResult::REJECTED"});
    CHECK(doctest::String{doctest::StringMaker<SetResult>::convert(SetResult::NO_CHANGE)} ==
          doctest::String{"SetResult::NO_CHANGE"});
}

// StringMaker<PropertyId> shows the name (and marks an empty id).
TEST_CASE("S036_2: StringMaker<PropertyId> shows the name")
{
    PropertyId w = PropertyId::of("Width");
    CHECK(doctest::String{doctest::StringMaker<PropertyId>::convert(w)} ==
          doctest::String{"PropertyId(Width)"});
    CHECK(doctest::String{doctest::StringMaker<PropertyId>::convert(PropertyId{})} ==
          doctest::String{"PropertyId(empty)"});
}
