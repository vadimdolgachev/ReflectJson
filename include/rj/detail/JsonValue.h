#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace rj::detail {
    enum class JsonKind {
        Null,
        Boolean,
        Int64,
        UInt64,
        Double,
        String,
        Array,
        Object
    };

    struct JsonMember;
    struct JsonValue;

    using JsonElements = std::vector<JsonValue>;
    using JsonMembers = std::vector<JsonMember>;

    struct JsonValue final {
    private:
        using Variant = std::variant<
            std::monostate,
            bool,
            int64_t,
            uint64_t,
            double,
            std::string,
            JsonElements,
            JsonMembers
        >;

        Variant value;

        template<typename T>
        static constexpr bool is_exact_alternative = []<typename... Types>(std::variant<Types...>) {
            return (std::is_same_v<T, Types> || ...);
        }(Variant{});

    public:
        template<typename Value>
            requires is_exact_alternative<std::decay_t<Value> >
        void setValue(Value &&newVal) {
            this->value = std::forward<Value>(newVal);
        }

        [[nodiscard]] JsonKind getKind() const noexcept {
            return static_cast<JsonKind>(value.index());
        }

        template<typename Self>
        [[nodiscard]] decltype(auto) asBool(this Self &&self) {
            return std::get<bool>(std::forward<Self>(self).value);
        }

        template<typename Self>
        [[nodiscard]] decltype(auto) asInt64(this Self &&self) {
            return std::get<int64_t>(std::forward<Self>(self).value);
        }

        template<typename Self>
        [[nodiscard]] decltype(auto) asUInt64(this Self &&self) {
            return std::get<uint64_t>(std::forward<Self>(self).value);
        }

        template<typename Self>
        [[nodiscard]] decltype(auto) asDouble(this Self &&self) {
            return std::get<double>(std::forward<Self>(self).value);
        }

        template<typename Self>
        [[nodiscard]] decltype(auto) asString(this Self &&self) {
            return std::get<std::string>(std::forward<Self>(self).value);
        }

        template<typename Self>
        [[nodiscard]] decltype(auto) asArray(this Self &&self) {
            return std::get<JsonElements>(std::forward<Self>(self).value);
        }

        template<typename Self>
        [[nodiscard]] decltype(auto) asMembers(this Self &&self) {
            return std::get<JsonMembers>(std::forward<Self>(self).value);
        }
    };

    struct JsonMember final {
        std::string name;
        JsonValue value;
    };

    JsonValue readDocument(std::istream &input);
}
