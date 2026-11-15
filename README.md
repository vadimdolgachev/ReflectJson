# ReflectJson

The C++ examples below are independent fragments for `main()`, using these includes
and types. Signatures omit template declarations; `T` denotes the converted type and
`E` an enum type.

```cpp
#include <rj/Annotations.h>
#include <rj/JsonReader.h>
#include <rj/JsonSchema.h>
#include <rj/JsonWriter.h>
#include <rj/MetaClass.h>

#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

struct Person {
    int id;
    std::string name;
};

enum class State {
    Hired [[= rj::rename{"hired"}]],
    Fired [[= rj::rename{"fired"}]]
};
```

## JSON API

### toJson: return a string

`std::string rj::toJson(const T &value, size_t indent = 2)`

Serializes `value` and returns its JSON text. `indent` sets spaces per nesting level;
`0` selects compact output. Values that cannot be represented throw `std::domain_error`.
Enum values use their JSON names. An `rj::rename` annotation on an enumerator
replaces its C++ name in JSON output.

```cpp
const auto text = rj::toJson(Person{42, "Alice"}, 0);
std::cout << text; // {"id":42,"name":"Alice"}
```

```cpp
std::cout << rj::toJson(State::Hired, 0); // "hired"
```

### toJson: write to a stream

`void rj::toJson(const T &value, std::ostream &output, size_t indent = 2)`

Writes directly to `output` with the same formatting rules as the string overload.
The caller owns the stream; the function does not close or flush it. Stream failures
follow its exception settings, and output can be partial if an exception occurs.

```cpp
std::ostringstream output;
rj::toJson(Person{42, "Alice"}, output, 0);
std::cout << output.str(); // {"id":42,"name":"Alice"}
```

### fromJson: create an object

`T rj::fromJson<T>(std::istream &input)`

Reads the remaining stream through EOF, parses and validates the document, then
constructs `T{}` and fills its fields. `T` must be a mutable value type, default
constructible and copyable or movable. Non-optional fields are required even when
they have initializers; absent optionals retain their defaults. Duplicate and
unknown keys are errors. JSON, type-validation, and I/O failures throw `rj::JsonError`.
Renamed enumerators are matched by their JSON names; the original C++ identifier
is not an additional input alias.

```cpp
std::istringstream input(R"({"id":42,"name":"Alice"})");
const auto person = rj::fromJson<Person>(input);
std::cout << person.name; // Alice
```

```cpp
std::istringstream input(R"("hired")");
const auto state = rj::fromJson<State>(input);
std::cout << (state == State::Hired); // 1
```

### fromJson: update an object

`void rj::fromJson(std::istream &input, T &result)`

Reads and validates a complete document before populating the existing mutable
`result`. Required fields must still be present; missing optionals retain their
current values. The root object does not need a default constructor or copying.
Use [mergePatch](#mergepatch) when required fields may be omitted.

```cpp
Person person{7, "Before"};
std::istringstream input(R"({"id":42,"name":"Alice"})");
rj::fromJson(input, person);
std::cout << person.name; // Alice
```

### mergePatch

`void rj::mergePatch(std::istream &input, T &result)`

Applies a typed JSON Merge Patch to an existing mutable value. Missing fields stay
unchanged, nested objects are patched recursively, and arrays are replaced as whole
values. `null` clears an optional; other targets accept it only for `std::nullptr_t`.
Unknown fields are rejected. Parsing and validation finish before population begins;
invalid JSON or field values throw `rj::JsonError`.
Enum values follow the same naming rules as `fromJson`, including `rj::rename`.

```cpp
Person person{42, "Alice"};
std::istringstream patch(R"({"name":"Bob"})");
rj::mergePatch(patch, person);
std::cout << person.id << ' ' << person.name; // 42 Bob
```

### schema

`std::string rj::schema<T>(size_t indent = 2)`

Returns a JSON Schema description without constructing `T`. It emits `type`,
`properties`, `items`, `required`, and `enum` where applicable. Optional fields are
nullable and omitted from `required`. `indent = 0` selects compact output.
The reader additionally enforces numeric ranges, fixed array lengths, and unknown-key
rejection; these constraints are not emitted by this function.

```cpp
const auto text = rj::schema<std::optional<State>>(0);
std::cout << text;
// {"type":["string","null"],"enum":["hired","fired",null]}
```

Enum schemas use the JSON spelling of enumerator values, including `rj::rename`.
For aliases sharing a value, serialization and schema entries use the first declared
enumerator's JSON name. An empty enum has schema `{"not":{}}`;
an optional empty enum has schema `{"type":"null"}`.

## Reflection and enum API

Header: `<rj/MetaClass.h>`. `MetaClass<T>` visits the static class type, with public
bases first and then its own fields. Generic metadata includes fields marked
`rj::ignore` and uses C++ member names; JSON conversion applies annotations separately.
Similarly, `meta::MetaEnum<E>` uses C++ enumerator names by default and does not apply
`rj::rename`. Both metadata templates accept an optional `FieldPolicy` parameter
for name projection; the JSON API selects its annotation-aware policy internally.

### forEach

`static constexpr void meta::MetaClass<T>::forEach(const Class &object, Visitor &&visitor)`

Calls `visitor(property, field)` for every field. `Class` is `T` without cv/ref
qualifiers; `property` is a cached `const meta::MetaProperty &`, and field access is
through the const object. Visitor exceptions propagate to the caller.

```cpp
const Person person{42, "Alice"};
meta::MetaClass<Person>::forEach(person, [](const auto &property, const auto &field) {
    std::cout << property.name << " = " << field << '\n';
});
```

### forEachMutable

`static constexpr void meta::MetaClass<T>::forEachMutable(Class &object, Visitor &&visitor)`

Calls the visitor with mutable access to each field of the existing object.
Const, reference, and bit-field members are rejected at compile time. The same
field order, metadata, and exception propagation as `forEach` apply.

```cpp
Person person{42, "Alice"};
meta::MetaClass<Person>::forEachMutable(person, [](const auto &, auto &field) {
    field = {};
}); // person is now {0, ""}
```

### MetaEnum::fromString

`[[nodiscard]] static constexpr std::optional<std::remove_cvref_t<E>> meta::MetaEnum<E>::fromString(std::string_view name) noexcept`

Returns the enumerator matching `name` exactly, or `std::nullopt`. The returned
enum type is `E` without cv/ref qualifiers.
With the default policy, all declared C++ aliases are accepted and JSON rename
annotations are ignored. An empty enum never matches. No destination object is modified.

```cpp
constexpr auto state = meta::MetaEnum<State>::fromString("Hired");
static_assert(state == State::Hired);
static_assert(!meta::MetaEnum<State>::fromString("hired"));
static_assert(!meta::MetaEnum<State>::fromString("Unknown"));
```

### MetaEnum::toString

`[[nodiscard]] static constexpr std::optional<std::string_view> meta::MetaEnum<E>::toString(std::remove_cvref_t<E> value) noexcept`

Returns the first declared name for `value`, including when several enumerators
share a value. The default policy returns the C++ name even when `rj::rename` is
present. A value without a declared name returns `std::nullopt`. Returned views
refer to static name storage.

```cpp
constexpr auto name = meta::MetaEnum<State>::toString(State::Hired);
static_assert(name == "Hired");
static_assert(!meta::MetaEnum<State>::toString(static_cast<State>(99)));
```

```cpp
enum class Aliased { First, Alias = First };
static_assert(meta::MetaEnum<Aliased>::fromString("Alias") == Aliased::First);
static_assert(meta::MetaEnum<Aliased>::toString(Aliased::Alias) == "First");
```

## Metadata utilities

Header: `<rj/MetaClass.h>`. The cached metadata is available without an object.

| Member | Contents |
| --- | --- |
| `MetaClass<T>::fields` | Flattened `MetaField` descriptors in visitation order. |
| `MetaClass<T>::fieldCount` | Number of fields in that metadata view. |
| `MetaClass<T>::properties` | Cached `MetaProperty` values in field order. |
| `MetaEnum<E>::enumerators` | Reflection descriptors for every declared enumerator. |
| `MetaProperty::name`, `typeName` | Non-owning string views for a field's name and type. |
| `StaticStrView::data`, `size` | Pointer and length of a text view descriptor. |

### MetaField::property

`[[nodiscard]] constexpr meta::MetaProperty meta::MetaField::property() const noexcept`

Returns the field's name and type name as a `MetaProperty` value. Text is referenced,
not copied; descriptors produced by `MetaClass` use static text storage.

```cpp
constexpr auto property = meta::MetaClass<Person>::fields[0].property();
static_assert(property.name == "id");
static_assert(property.typeName == "int");
```

### makeStrView

`consteval meta::StaticStrView meta::makeStrView(std::string_view text)`

Creates static text storage during compilation and returns a pointer/length
descriptor for it. This helper must be evaluated at compile time.

```cpp
constexpr auto text = meta::makeStrView("name");
static_assert(text.size == 4);
```

### StaticStrView conversion

`[[nodiscard]] explicit constexpr operator std::string_view() const noexcept`

Explicitly converts a descriptor to `std::string_view` over the same pointer and
length. It does not allocate, copy text, or take ownership.

```cpp
constexpr auto text = meta::makeStrView("name");
constexpr auto view = static_cast<std::string_view>(text);
static_assert(view == "name");
```
