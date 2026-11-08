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
        void validateInteger(const Integer number) {
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
        }

        double numberAsDouble(const JsonValue &value);

        template<typename T>
        void validateNumber(const JsonValue &value) {
            const bool isNumber = value.getKind() == JsonKind::Int64
                                  || value.getKind() == JsonKind::UInt64
                                  || value.getKind() == JsonKind::Double;
            if (!isNumber) {
                throw JsonError("Expected number");
            }
            if constexpr (std::is_integral_v<T>) {
                switch (value.getKind()) {
                    case JsonKind::Int64:
                        detail::validateInteger<T>(value.asInt64());
                        break;
                    case JsonKind::UInt64:
                        detail::validateInteger<T>(value.asUInt64());
                        break;
                    default:
                        throw JsonError("Expected an integer value");
                }
            } else {
                if constexpr (std::is_same_v<T, float>) {
                    static_cast<void>(checkedFloat(numberAsDouble(value)));
                } else {
                    static_assert(std::numeric_limits<T>::digits >= std::numeric_limits<double>::digits
                                  && std::numeric_limits<T>::max_exponent >= std::numeric_limits<double>::max_exponent,
                                  "JsonReader: unsupported floating-point type");
                }
            }
        }

        template<typename T>
        void readNumber(const JsonValue &value, T &result) {
            if constexpr (std::is_integral_v<T>) {
                if (value.getKind() == JsonKind::Int64) {
                    result = static_cast<T>(value.asInt64());
                } else {
                    result = static_cast<T>(value.asUInt64());
                }
            } else {
                result = static_cast<T>(numberAsDouble(value));
            }
        }

        template<typename T, bool PatchMode = false>
        void validateJson(const JsonValue &json) {
            using Value = std::remove_cv_t<T>;
            static_assert(!std::is_const_v<T>, "JsonReader does not support const targets");
            if constexpr (std::is_same_v<Value, std::nullptr_t>) {
                if (json.getKind() != JsonKind::Null) {
                    throw JsonError("Expected null");
                }
            } else if constexpr (std::is_same_v<Value, bool>) {
                if (json.getKind() != JsonKind::Boolean) {
                    throw JsonError("Expected boolean");
                }
            } else if constexpr (std::is_integral_v<Value> || std::is_floating_point_v<Value>) {
                detail::validateNumber<Value>(json);
            } else if constexpr (std::is_same_v<Value, std::string>) {
                if (json.getKind() != JsonKind::String) {
                    throw JsonError("Expected string");
                }
            } else if constexpr (std::is_same_v<Value, std::string_view>) {
                static_assert(false, "JsonReader does not support std::string_view targets");
            } else if constexpr (is_json_optional<Value>) {
                if (json.getKind() != JsonKind::Null) {
                    detail::validateJson<typename Value::value_type, PatchMode>(json);
                }
            } else if constexpr (is_json_array<Value>) {
                if (json.getKind() != JsonKind::Array) {
                    throw JsonError("Expected array");
                }
                const auto &elements = json.asArray();
                if constexpr (std::is_array_v<Value>) {
                    if (elements.size() != std::extent_v<Value>) {
                        throw JsonError("Fixed array length mismatch");
                    }
                } else if constexpr (!is_vector<Value>) {
                    if (elements.size() != std::tuple_size_v<Value>) {
                        throw JsonError("Fixed array length mismatch");
                    }
                }
                for (const auto &element: elements) {
                    if constexpr (std::is_array_v<Value>) {
                        detail::validateJson<std::remove_extent_t<Value> >(element);
                    } else {
                        detail::validateJson<typename Value::value_type>(element);
                    }
                }
            } else if constexpr (std::is_class_v<Value>) {
                if (json.getKind() != JsonKind::Object) {
                    throw JsonError("Expected object");
                }
                std::unordered_map<std::string_view, const JsonMember *> remaining;
                const auto &members = json.asMembers();
                for (const auto &member: members) {
                    remaining.emplace(member.name, &member);
                }
                size_t index = 0;
                template for (constexpr auto field: JsonMetaClass<Value>::fields) {
                    using Field = [:std::meta::type_of(field.reflection):];
                    const auto &property = JsonMetaClass<Value>::properties[index++];
                    const auto found = remaining.find(property.name);
                    if (found == remaining.end()) {
                        if constexpr (!is_json_optional<std::remove_cvref_t<Field> > && !PatchMode) {
                            throw JsonError("Missing required field: " + std::string(property.name));
                        }
                    } else {
                        detail::validateJson<Field, PatchMode>(found->second->value);
                        remaining.erase(found);
                    }
                }
                for (const auto name: JsonMetaClass<Value>::ignoredNames) {
                    remaining.erase(name);
                }
                for (const auto &[name, _]: members) {
                    if (remaining.contains(name)) {
                        throw JsonError("Unknown field: " + name);
                    }
                }
            } else {
                static_assert(false, "JsonReader: unsupported type");
            }
        }

        template<typename T, typename Json>
        void populateValue(Json &&json, T &value) {
            using Value = std::remove_cvref_t<T>;
            static_assert(!std::is_const_v<T>, "JsonReader does not support const targets");
            if constexpr (std::is_same_v<Value, std::nullptr_t>) {
                value = nullptr;
            } else if constexpr (std::is_same_v<Value, bool>) {
                value = json.asBool();
            } else if constexpr (std::is_integral_v<Value> || std::is_floating_point_v<Value>) {
                detail::readNumber(json, value);
            } else if constexpr (std::is_same_v<Value, std::string>) {
                value = std::forward<Json>(json).asString();
            } else if constexpr (std::is_same_v<Value, std::string_view>) {
                static_assert(false, "JsonReader does not support std::string_view targets");
            } else if constexpr (is_json_optional<Value>) {
                if (json.getKind() == JsonKind::Null) {
                    value.reset();
                } else {
                    static_assert(std::is_default_constructible_v<typename Value::value_type>,
                                  "JsonReader requires default-constructible optional values");
                    if (!value) {
                        value.emplace();
                    }
                    detail::populateValue(std::forward<Json>(json), *value);
                }
            } else if constexpr (is_json_array<Value>) {
                if constexpr (is_vector<Value>) {
                    using Element = Value::value_type;
                    static_assert(std::is_default_constructible_v<Element>,
                                  "JsonReader requires default-constructible vector elements");
                    auto &&elements = std::forward<Json>(json).asArray();
                    value.clear();
                    value.reserve(elements.size());
                    for (auto &&element: elements) {
                        Element item{};
                        detail::populateValue(std::forward<Json>(element), item);
                        value.push_back(std::move_if_noexcept(item));
                    }
                } else {
                    auto item = json.asArray().begin();
                    for (auto &element: value) {
                        detail::populateValue(std::forward<Json>(*item), element);
                        ++item;
                    }
                }
            } else if constexpr (std::is_class_v<Value>) {
                using Member = std::conditional_t<
                    std::is_const_v<std::remove_reference_t<Json> >,
                    const JsonMember,
                    JsonMember>;
                std::unordered_map<std::string_view, Member *> members;
                for (auto &member: json.asMembers()) {
                    members.emplace(member.name, &member);
                }
                JsonMetaClass<Value>::forEachMutable(value, [&](const auto &property, auto &field) {
                    auto found = members.find(property.name);
                    if (found != members.end()) {
                        detail::populateValue(std::forward<Json>(found->second->value), field);
                    }
                });
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

        auto jsonVal = detail::readDocument(input);
        detail::validateJson<T>(jsonVal);
        T result{};
        detail::populateValue(std::move(jsonVal), result);
        if constexpr (std::is_move_constructible_v<T>) {
            return result;
        } else {
            return static_cast<const T &>(result);
        }
    }

    template<typename T>
    void fromJson(std::istream &input, T &result) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T> && !std::is_array_v<T>,
                      "JsonReader requires a mutable value target (use std::array for root arrays)");

        auto jsonVal = detail::readDocument(input);
        detail::validateJson<T>(jsonVal);
        detail::populateValue(std::move(jsonVal), result);
    }

    template<typename T>
    void mergePatch(std::istream &input, T &result) {
        static_assert(!std::is_reference_v<T> && !std::is_const_v<T> && !std::is_array_v<T>,
                      "JsonReader requires a mutable value target (use std::array for root arrays)");

        auto jsonVal = detail::readDocument(input);
        detail::validateJson<T, true>(jsonVal);
        detail::populateValue(std::move(jsonVal), result);
    }
}
