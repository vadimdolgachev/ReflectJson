#pragma once

#include <rj/MetaClass.h>
#include <rj/detail/JsonSchema.h>
#include <rj/detail/ContainerTraits.h>
#include <rj/detail/JsonValue.h>

#include <istream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace rj {
    class JsonError final : public std::runtime_error {
    public:
        explicit JsonError(const std::string &message);
    };

    namespace detail {
        float checkedFloat(double number);

        template<typename T, typename Integer>
        void assignInteger(const Integer number, T &result) {
            if constexpr (std::is_signed_v<Integer>) {
                if constexpr (!std::is_signed_v<T>) {
                    if (number < 0) {
                        throw JsonError("Negative integer for unsigned target");
                    }
                } else if constexpr (std::numeric_limits<T>::digits < std::numeric_limits<Integer>::digits) {
                    if (number < static_cast<Integer>(std::numeric_limits<T>::lowest())) {
                        throw JsonError("Integer out of range for target type");
                    }
                }
            }
            if constexpr (std::numeric_limits<T>::digits < std::numeric_limits<Integer>::digits) {
                if (number > static_cast<Integer>(std::numeric_limits<T>::max())) {
                    throw JsonError("Integer out of range for target type");
                }
            }
            result = static_cast<T>(number);
        }

        template<typename T>
        void readNumber(const JsonValue &value, T &result) {
            const bool isNumber = value.kind == JsonKind::Int64 || value.kind == JsonKind::UInt64
                                  || value.kind == JsonKind::Double;
            if (!isNumber) {
                throw JsonError("Expected number");
            }
            if constexpr (std::is_integral_v<T>) {
                switch (value.kind) {
                    case JsonKind::Int64:
                        detail::assignInteger(value.signedNumber, result);
                        break;
                    case JsonKind::UInt64:
                        detail::assignInteger(value.unsignedNumber, result);
                        break;
                    default:
                        throw JsonError("Expected an integer value");
                }
            } else {
                double number{};
                switch (value.kind) {
                    case JsonKind::Int64:
                        number = static_cast<double>(value.signedNumber);
                        break;
                    case JsonKind::UInt64:
                        number = static_cast<double>(value.unsignedNumber);
                        break;
                    default:
                        number = value.floatingNumber;
                        break;
                }
                if constexpr (std::is_same_v<T, float>) {
                    result = checkedFloat(number);
                } else {
                    static_assert(std::numeric_limits<T>::digits >= std::numeric_limits<double>::digits
                                  && std::numeric_limits<T>::max_exponent >= std::numeric_limits<double>::max_exponent,
                                  "JsonReader: unsupported floating-point type");
                    result = static_cast<T>(number);
                }
            }
        }

        template<typename T>
        void readValue(const JsonValue &value, T &result) {
            using Value = std::remove_cv_t<T>;
            static_assert(!std::is_const_v<T>, "JsonReader does not support const targets");
            if constexpr (std::is_same_v<Value, std::nullptr_t>) {
                if (value.kind != JsonKind::Null) {
                    throw JsonError("Expected null");
                }
                result = nullptr;
            } else if constexpr (std::is_same_v<Value, bool>) {
                if (value.kind != JsonKind::Boolean) {
                    throw JsonError("Expected boolean");
                }
                result = value.boolean;
            } else if constexpr (std::is_integral_v<Value> || std::is_floating_point_v<Value>) {
                detail::readNumber(value, result);
            } else if constexpr (std::is_same_v<Value, std::string>) {
                if (value.kind != JsonKind::String) {
                    throw JsonError("Expected string");
                }
                result = value.text;
            } else if constexpr (std::is_same_v<Value, std::string_view>) {
                static_assert(false, "JsonReader does not support std::string_view targets");
            } else if constexpr (is_json_optional<Value>) {
                if (value.kind == JsonKind::Null) {
                    result.reset();
                } else {
                    static_assert(std::is_default_constructible_v<typename Value::value_type>,
                                  "JsonReader requires default-constructible optional values");
                    if (!result) {
                        result.emplace();
                    }
                    detail::readValue(value, *result);
                }
            } else if constexpr (is_json_array<Value>) {
                if (value.kind != JsonKind::Array) {
                    throw JsonError("Expected array");
                }
                if constexpr (is_vector<Value>) {
                    using Element = Value::value_type;
                    static_assert(std::is_default_constructible_v<Element>,
                                  "JsonReader requires default-constructible vector elements");
                    result.clear();
                    result.reserve(value.elements.size());
                    for (const auto &element: value.elements) {
                        Element item{};
                        detail::readValue(element, item);
                        result.push_back(std::move_if_noexcept(item));
                    }
                } else {
                    if (value.elements.size() != std::size(result)) {
                        throw JsonError("Fixed array length mismatch");
                    }
                    auto item = value.elements.begin();
                    for (auto &element: result) {
                        detail::readValue(*item++, element);
                    }
                }
            } else if constexpr (std::is_class_v<Value>) {
                if (value.kind != JsonKind::Object) {
                    throw JsonError("Expected object");
                }
                std::unordered_map<std::string_view, const JsonMember *> remaining;
                for (const auto &member: value.members) {
                    remaining.emplace(member.name, &member);
                }
                JsonMetaClass<Value>::forEachMutable(result, [&](const auto &property, auto &field) {
                    const auto found = remaining.find(property.name);
                    if (found == remaining.end()) {
                        if constexpr (!is_json_optional<std::remove_cvref_t<decltype(field)> >) {
                            throw JsonError("Missing required field: " + std::string(property.name));
                        }
                    } else {
                        detail::readValue(found->second->value, field);
                        remaining.erase(found);
                    }
                });
                for (const auto name: JsonMetaClass<Value>::ignoredNames) {
                    remaining.erase(name);
                }
                for (const auto &member: value.members) {
                    if (remaining.contains(member.name)) {
                        throw JsonError("Unknown field: " + member.name);
                    }
                }
            } else {
                static_assert(false, "JsonReader: unsupported type");
            }
        }
    }

    template<typename T>
    T fromJson(std::istream &input) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T> && !std::is_array_v<T>,
                      "JsonReader requires a mutable value target (use std::array for root arrays)");
        static_assert(std::is_default_constructible_v<T>, "JsonReader requires a default-constructible target");
        static_assert(std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                      "JsonReader requires a copyable or movable target");
        const auto value = detail::readDocument(input);
        T result{};
        detail::readValue(value, result);
        if constexpr (std::is_move_constructible_v<T>) {
            return result;
        } else {
            return static_cast<const T &>(result);
        }
    }
}
