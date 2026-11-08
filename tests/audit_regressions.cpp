#include <rj/JsonReader.h>

#include <concepts>
#include <fstream>
#include <iostream>
#include <sstream>

template<typename Input>
concept JsonInput = requires(Input &input) {
    { rj::fromJson<int>(input) } -> std::same_as<int>;
};

static_assert(std::same_as<decltype(static_cast<int (*)(std::istream &)>(&rj::fromJson<int>)), int (*)(std::istream &)>);
static_assert(JsonInput<std::istream> && JsonInput<std::istringstream>);
static_assert(JsonInput<std::stringstream> && JsonInput<std::ifstream>);
static_assert(!JsonInput<const std::istream> && !JsonInput<std::ostringstream>);
static_assert(!JsonInput<std::string> && !JsonInput<std::string_view>);
static_assert(!JsonInput<const char *> && !JsonInput<const char[2]>);

// Ordinary application helpers must not participate in ReflectJson conversion.
namespace generic_adl {
    struct Value { int number; };
    inline int calls = 0;
    template<typename Input>
    void readValue(const Input &, Value &value) {
        ++calls;
        value.number = 999;
    }
}

namespace exact_adl {
    struct Value { int number; };
    inline int calls = 0;
    void readValue(const rj::detail::JsonValue &, Value &value) {
        ++calls;
        value.number = 999;
    }
}

namespace {
    void require(const bool condition, const char *message) {
        if (!condition) { throw std::runtime_error(message); }
    }

    template<typename T>
    struct ContainerFields {
        T direct;
        std::optional<T> maybe;
        std::vector<T> dynamic;
        std::array<T, 1> fixed;
        T raw[1];
    };

    template<typename T>
    void checkAdl() {
        std::istringstream input(R"({"direct":{"number":1},"maybe":{"number":2},
            "dynamic":[{"number":3}],"fixed":[{"number":4}],"raw":[{"number":5}]})");
        const auto result = rj::fromJson<ContainerFields<T>>(input);
        require(result.direct.number == 1, "Object conversion used ADL");
        require(result.maybe && result.maybe->number == 2, "Optional conversion used ADL");
        require(result.dynamic.size() == 1 && result.dynamic[0].number == 3, "Vector conversion used ADL");
        require(result.fixed[0].number == 4, "std::array conversion used ADL");
        require(result.raw[0].number == 5, "C array conversion used ADL");

        std::istringstream invalid(R"({"direct":{"unexpected":1},"maybe":null,
            "dynamic":[],"fixed":[{"number":4}],"raw":[{"number":5}]})");
        try {
            (void) rj::fromJson<ContainerFields<T>>(invalid);
            throw std::runtime_error("ADL bypassed field validation");
        } catch (const rj::JsonError &error) {
            require(std::string_view(error.what()).starts_with("Missing required field: number"),
                    "Wrong nested field error");
        }
    }

    struct NonCopyable {
        int number = 5;
        NonCopyable() = default;
        NonCopyable(const NonCopyable &) = delete;
        NonCopyable(NonCopyable &&) = delete;
    };

    struct Fields {
        int ordinary;
        mutable int mutableValue;
        int &reference;
        unsigned bit : 1;
        int array[2];
        NonCopyable nonCopyable;
    };

    void checkConstTraversal() {
        int referenced = 3;
        const Fields object{1, 2, referenced, 1, {4, 5}, {}};
        size_t index = 0;
        meta::MetaClass<Fields>::forEach(object, [&](const auto &property, auto &field) {
            static_assert(std::is_lvalue_reference_v<decltype(field)>);
            const auto &expected = meta::MetaClass<Fields>::properties[index++];
            require(property.name == expected.name && property.typeName == expected.typeName,
                    "Metadata order/content changed");
            using Type = std::remove_cvref_t<decltype(field)>;
            if constexpr (std::is_same_v<Type, int>) {
                const int *expected = property.name == "ordinary" ? &object.ordinary
                    : property.name == "mutableValue" ? &object.mutableValue : &referenced;
                require(&field == expected, "Const scalar/reference field was copied");
                require(field == (property.name == "ordinary" ? 1 : property.name == "mutableValue" ? 2 : 3),
                        "Scalar value changed");
            } else if constexpr (std::is_array_v<Type>) {
                require(&field == &object.array && field[0] == 4 && field[1] == 5, "Array was copied");
            } else if constexpr (std::is_same_v<Type, NonCopyable>) {
                require(&field == &object.nonCopyable && field.number == 5, "Noncopyable field was copied");
            } else {
                require(field == 1, "Bitfield value changed");
            }
        });
        require(index == 6 && object.mutableValue == 2 && referenced == 3, "Const traversal changed state");
    }

    struct Empty {};

    void checkDiagnostics() {
        constexpr std::array<std::string_view, 32> controls{
            "\\u0000", "\\u0001", "\\u0002", "\\u0003", "\\u0004", "\\u0005", "\\u0006", "\\u0007",
            "\\u0008", "\\u0009", "\\u000a", "\\u000b", "\\u000c", "\\u000d", "\\u000e", "\\u000f",
            "\\u0010", "\\u0011", "\\u0012", "\\u0013", "\\u0014", "\\u0015", "\\u0016", "\\u0017",
            "\\u0018", "\\u0019", "\\u001a", "\\u001b", "\\u001c", "\\u001d", "\\u001e", "\\u001f"
        };
        for (size_t ch = 0; ch < controls.size(); ++ch) {
            const auto message = std::string("left") + static_cast<char>(ch) + "right\\suffixΩ";
            const rj::JsonError error(message);
            const std::string expected = "left" + std::string(controls[ch]) + "right\\\\suffixΩ";
            require(std::string_view(error.what()) == expected, "Control byte truncated or corrupted what()");
        }
        const rj::JsonError ordinary("Expected number");
        require(std::string_view(ordinary.what()) == "Expected number", "Ordinary diagnostic changed");

        struct Case { std::string_view input; std::string_view expected; };
        for (const auto &test: {
                 Case{R"( {"a\u0000b":1})", "Unknown field: a\\u0000b"},
                 Case{R"({"a\u0000b":1,"a\u0000b":2})", "Duplicate object key: a\\u0000b"},
                 Case{R"({"a\\b":1})", "Unknown field: a\\\\b"},
                 Case{R"({"a\nb":1})", "Unknown field: a\\u000ab"}}) {
            std::istringstream input{std::string(test.input)};
            try {
                (void) rj::fromJson<Empty>(input);
                throw std::runtime_error("Expected key diagnostic");
            } catch (const rj::JsonError &error) {
                require(std::string_view(error.what()) == test.expected, "Key diagnostic is not escaped correctly");
            }
        }
        std::istringstream input(R"("a\u0000b\\c\nΩ")");
        constexpr char expected[] = "a\0b\\c\nΩ";
        require(rj::fromJson<std::string>(input) == std::string(expected, sizeof(expected) - 1),
                "Diagnostic escaping affected JSON payload");
    }
}

int main() {
    try {
        checkAdl<generic_adl::Value>();
        checkAdl<exact_adl::Value>();
        require(generic_adl::calls == 0 && exact_adl::calls == 0, "Application readValue was called");
        checkConstTraversal();
        checkDiagnostics();
        std::cout << "Audit regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
