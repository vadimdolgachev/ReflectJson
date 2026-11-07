#include <rj/JsonWriter.h>

#include <cstdint>
#include <iostream>

namespace {
    struct Person {
        int id;
        std::string name;
        double salary;
        std::vector<int> values;
    };
}

#include <limits>
#include <locale>
#include <utility>

namespace {
    struct Empty {
        static constexpr int ignored = 7;
    };
    struct Location {
        std::string city;
        int postcode;
    };
    struct Complex {
        Person owner;
        std::optional<Location> address;
        std::vector<Person> team;
        std::array<int, 3> numbers;
        std::vector<bool> flags;
        std::vector<std::optional<std::string>> aliases;
        int grid[2][2];
        std::string_view note;
        std::optional<Location> missing;
    };
    struct Nested {
        std::vector<std::optional<int>> items;
        Empty empty;
    };
    struct Bits {
        unsigned enabled : 1;
        signed code : 3;
    };
    struct Reference {
        const int &value;
    };
    struct UnicodeKey {
        int имя;
    };
    struct JsonCase {
        std::string name;
        std::string document;
    };
    struct CommaDecimal : std::numpunct<char> {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '.'; }
        std::string do_grouping() const override { return "\3"; }
    };

    void require(bool condition, const char *message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    template<typename Error, typename Function>
    void require_throws(Function operation) {
        try {
            operation();
        } catch (const Error &) {
            return;
        }
        throw std::runtime_error("Expected exception was not thrown");
    }

    template<typename T, typename Emit>
    void check_floats(std::string_view label, Emit emit) {
        const std::array<T, 8> values{
            T{0}, -T{0}, T(0.1), std::nextafter(T{1}, T{2}),
            std::numeric_limits<T>::min(), std::numeric_limits<T>::denorm_min(),
            std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest()
        };
        const auto representable = [&] {
            if constexpr (std::is_same_v<T, long double>) {
                return std::array<T, 8>{0.0L, -0.0L, static_cast<T>(0.1), static_cast<T>(std::nextafter(1.0, 2.0)),
                    static_cast<T>(std::numeric_limits<double>::min()), static_cast<T>(std::numeric_limits<double>::denorm_min()),
                    static_cast<T>(std::numeric_limits<double>::max()), static_cast<T>(std::numeric_limits<double>::lowest())};
            } else {
                return values;
            }
        }();
        int index = 0;
        for (T value : representable) {
            const auto document = rj::toJson(value, 0);
            T decoded{};
            if constexpr (std::is_same_v<T, long double>) {
                double number{};
                const auto [end, error] = std::from_chars(document.data(), document.data() + document.size(), number);
                require(error == std::errc{} && end == document.data() + document.size(),
                        "DOM double value was not fully parsed");
                decoded = static_cast<T>(number);
            } else {
                const auto [end, error] = std::from_chars(
                    document.data(), document.data() + document.size(), decoded);
                require(error == std::errc{} && end == document.data() + document.size(),
                        "floating-point value was not fully parsed");
            }
            require(decoded == value && std::signbit(decoded) == std::signbit(value),
                    "floating-point value did not round-trip");
            emit(std::string(label) + "_" + std::to_string(index++), value);
        }
        for (T invalid : {std::numeric_limits<T>::quiet_NaN(),
                          std::numeric_limits<T>::infinity(),
                          -std::numeric_limits<T>::infinity()}) {
            require_throws<std::domain_error>([&] { rj::toJson(invalid); });
            require_throws<std::domain_error>([&] {
                rj::toJson(std::vector<std::optional<T>>{T{1}, invalid}, 0);
            });
        }
    }
}

#include "MetaclassChecks.h"
#include "WideIntegers.h"
#include "WriterChecks.h"

int main() {
    try {
        std::vector<JsonCase> cases;
        const auto emit = [&cases](std::string name, const auto &value, int indent = 0) {
            cases.push_back({std::move(name), rj::toJson(value, indent)});
        };
        const Person person{42, "Alice", 5000.50, {}};
        verify_metadata(person);
        verify_wide_integers();
        verify_writer(person, cases);
        require(meta::MetaClass<Person>::properties[1].typeName == "std::string", "type name regression");
        require(rj::toJson(person) ==
                "{\n  \"id\": 42,\n  \"name\": \"Alice\",\n  \"salary\": 5000.5,\n  \"values\": []\n}",
                "Default formatting mismatch");
        require(rj::toJson(person, 0) ==
                "{\"id\":42,\"name\":\"Alice\",\"salary\":5000.5,\"values\":[]}",
                "Compact formatting mismatch");
        require(rj::toJson(person, 4) ==
                "{\n    \"id\": 42,\n    \"name\": \"Alice\",\n    \"salary\": 5000.5,\n    \"values\": []\n}",
                "Custom indentation mismatch");
        emit("person_compact", person);
        emit("person_pretty", person, 2);
        emit("person_four_spaces", person, 4);

        const Nested nested{{1, std::nullopt}, {}};
        require(rj::toJson(nested) ==
                "{\n  \"items\": [\n    1,\n    null\n  ],\n  \"empty\": {}\n}",
                "Nested indentation mismatch");
        emit("nested_pretty", nested, 2);
        const Complex complex{
            person, Location{"Новокузнецк", 654000}, {person, {7, "Bob", 0.25, {}}},
            {1, -2, 3}, {true, false, true}, {std::string("alias"), std::nullopt},
            {{1, 2}, {3, 4}}, std::string_view("view"), std::nullopt
        };
        emit("complex_compact", complex);
        emit("complex_pretty", complex, 3);
        emit("array_objects", std::array<Person, 2>{person, {7, "Bob", 0.25, {}}});
        emit("optional_object", std::optional<Person>{person});
        emit("optional_empty", std::optional<Person>{});
        emit("optional_array", std::optional<std::vector<int>>{{1, 2}});
        emit("nested_optional", std::optional<std::optional<int>>{});
        emit("null", nullptr);
        emit("true", true);
        emit("false", false);
        emit("bits", Bits{1, -1});
        const int referenced = 17;
        emit("reference", Reference{referenced});
        emit("unicode_key", UnicodeKey{5});

        for (int indent : {0, 2, 4}) {
            require(rj::toJson(Empty{}, indent) == "{}", "Empty object formatting");
            require(rj::toJson(std::array<int, 0>{}, indent) == "[]", "Empty array formatting");
            require(rj::toJson(std::vector<int>{}, indent) == "[]", "Empty vector formatting");
            require(rj::toJson(std::vector<bool>{}, indent) == "[]", "Empty bool vector formatting");
        }
        emit("empty_object", Empty{});
        emit("empty_array", std::array<int, 0>{});
        emit("empty_vector", std::vector<int>{});
        emit("empty_bool_vector", std::vector<bool>{});
        emit("empty_string", std::string{});
        emit("empty_view", std::string_view{});
        emit("empty_containers", std::vector<std::array<int, 0>>(2), 2);
        const int c_array[]{4, -5, 6};
        emit("c_array", c_array);
        const char char_array[]{'A', '\0', 'B'};
        emit("char_array", char_array);

        std::string controls;
        for (int code = 0; code < 32; ++code) {
            controls += static_cast<char>(code);
        }
        emit("controls", controls);
        emit("escapes", std::string("\"\\/\b\f\n\r\t"));
        emit("nul", std::string("A\0B", 3));
        const char view_buffer[]{'x', '\0', 'z'};
        emit("bounded_view", std::string_view(view_buffer, 3));
        emit("utf8", std::string("Привет, café 😀"));

        emit("int64_min", std::numeric_limits<std::int64_t>::min());
        emit("int64_max", std::numeric_limits<std::int64_t>::max());
        emit("uint64_max", std::numeric_limits<std::uint64_t>::max());
        emit("signed_char", static_cast<signed char>(-128));
        emit("unsigned_char", static_cast<unsigned char>(255));
        emit("char16", u'\u03a9');

        const auto previous_locale = std::locale::global(
            std::locale(std::locale::classic(), new CommaDecimal));
        require(rj::toJson(person, 0) ==
                "{\"id\":42,\"name\":\"Alice\",\"salary\":5000.5,\"values\":[]}",
                "Numeric formatting depends on locale");
        emit("locale", person);
        std::locale::global(previous_locale);

        check_floats<float>("float", emit);
        check_floats<double>("double", emit);
        check_floats<long double>("long_double", emit);
        require_throws<std::domain_error>([&] {
            rj::toJson(Person{42, "Alice", std::numeric_limits<double>::infinity(), {}});
        });
        std::cout << rj::toJson(cases, 0) << '\n';
        std::cerr << "C++ formatting, numeric round-trip and exception checks passed.\n";
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
