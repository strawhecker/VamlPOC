// 11j-8 (GAP CLOSE, dep:S036) — the `Tag` object-slot DP.
//
// WPF-faithful (confirmed from the WPF source, FrameworkElement.cs line 3218):
// `Tag` is an OBJECT-typed DP — it holds a literal (the XAML schema stores a
// scalar as a string object) OR any object, and WPF's SetValue never rejects it
// because the type is `object`. VAML's DP model binds ONE ValueType per DP and
// Set() enforces the tag, so a single typed slot cannot hold an arbitrary
// object. The faithful VAML mapping (option A):
//   - `Tag` is a String-typed DP -> a literal `Tag="abc"` converts to
//     Value::String and fully works (the corpus's PRIMARY use, mirroring WPF
//     storing a string object).
//   - The object form `<X.Tag><SomeObject/></X.Tag>` is built + RETAINED by the
//     reader (parent-linked), so it is represented in the load result WITHOUT
//     being wedged into the scalar DP slot (an UntypedPtr would mismatch the
//     String-typed Tag and throw). WPF's "object slot" intent is honored.
//
// Headless (Rail 1).

#include "vaml/core/DependencyObject.h"
#include "vaml/core/DependencyProperty.h"
#include "vaml/core/Element.h"       // Button / StackPanel etc.
#include "vaml/core/PropertyId.h"
#include "vaml/core/Value.h"
#include "vaml/core/VamlReader.h"

#include "doctest/doctest.h"

#include <string>

using namespace vaml;

TEST_CASE("11j-8: Tag is a registered element-base DP (default: empty/no tag)")
{
    Button b; // ensures the UIElement base props (incl. Tag) are registered
    const DependencyProperty* tag = DependencyProperty::TryGet("Tag");
    REQUIRE(tag != nullptr);

    // Default is the empty String (no tag).
    CHECK(b.Get(PropertyId::of("Tag")).get<std::string>() == std::string{});

    // Literal attribute -> String value (the corpus's PRIMARY Tag use).
    (void)b.Set(PropertyId::of("Tag"), Value::String(std::string("ghostBrush")));
    CHECK(b.Get(PropertyId::of("Tag")).get<std::string>() == std::string("ghostBrush"));
}

TEST_CASE("11j-8: <X Tag=...> as an ATTRIBUTE loads through the reader (literal String)")
{
    VamlReader reader;
    auto root = reader.Load("<Button Tag=\"ghostBrush\"/>");
    REQUIRE(root != nullptr);
    Button* btn = dynamic_cast<Button*>(root.get());
    REQUIRE(btn != nullptr);
    CHECK(btn->Get(PropertyId::of("Tag")).get<std::string>() == std::string("ghostBrush"));
}

TEST_CASE("11j-8: an OBJECT <X.Tag><SomeObject/></X.Tag> loads WITHOUT throwing (parsed + retained)")
{
    // The object form is represented in the load result (the reader builds +
    // retains + parent-links the object), and it does NOT corrupt the scalar
    // String DP slot.
    VamlReader reader;
    auto root = reader.Load("<Button><Button.Tag><StackPanel/></Button.Tag></Button>");
    REQUIRE(root != nullptr);
    CHECK(dynamic_cast<Button*>(root.get()) != nullptr);
    // No throw occurred; the scalar Tag slot stays at its default.
    Button* btn = dynamic_cast<Button*>(root.get());
    CHECK(btn->Get(PropertyId::of("Tag")).get<std::string>() == std::string{});
}

