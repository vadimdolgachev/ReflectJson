#pragma once

#include <rj/MetaClass.h>
#include <rj/detail/JsonSchema.h>
#include <rj/detail/ContainerTraits.h>

#include <charconv>
#include <cmath>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace rj {
    class JsonWriter final {
    public:
        explicit JsonWriter(std::ostream &os, size_t indent = 2);

        JsonWriter(const JsonWriter &) = delete;

        JsonWriter &operator=(const JsonWriter &) = delete;

        JsonWriter(JsonWriter &&) = delete;

        JsonWriter &operator=(JsonWriter &&) = delete;

        void beginObject();

        void beginObject(std::string_view name);

        void endObject();

        void beginArray();

        void beginArray(std::string_view name);

        void endArray();

        template<typename T>
        void write(const T &value) {
            beforeValue(std::nullopt);
            writeValue(value);
        }

        template<typename T>
        void write(std::string_view name, const T &value) {
            beforeValue(name);
            writeValue(value);
        }

        std::ostream &os;

    private:
        enum class ContainerKind {
            Object,
            Array
        };

        struct Frame final {
            ContainerKind kind;
            bool first = true;
            std::unordered_set<std::string> names;
        };

        const size_t indent;
        std::vector<Frame> containers;
        bool root_written = false;

        void writeRaw(std::string_view text);

        void writeIndent(size_t depth);

        // A missing name denotes an array element or root; an empty key is valid.
        void beforeValue(const std::optional<std::string_view> &name);

        // The caller has already emitted this value's prefix.
        void openContainer(ContainerKind kind);

        void closeContainer(ContainerKind kind);

        void writeString(std::string_view text);

        template<typename T>
        void writeNumber(T value) {
            char buffer[128];
            if constexpr (std::is_floating_point_v<T>) {
                if (!std::isfinite(value)) {
                    throw std::domain_error("JsonWriter does not support NaN or infinity");
                }
                if constexpr (!std::is_same_v<T, float>) {
                    static_assert(std::numeric_limits<T>::digits >= std::numeric_limits<double>::digits
                                  && std::numeric_limits<T>::max_exponent >= std::numeric_limits<double>::max_exponent,
                                  "JsonWriter: unsupported floating-point type");
                    if (value < -static_cast<T>(std::numeric_limits<double>::max())
                        || value > static_cast<T>(std::numeric_limits<double>::max())) {
                        throw std::domain_error("JsonWriter: number exceeds the double range");
                    }
                }
                const auto number = static_cast<double>(value);
                if (static_cast<T>(number) != value) {
                    throw std::domain_error("JsonWriter: number loses precision in the double DOM");
                }
                const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), number);
                writeNumberText(buffer, end, error, true);
            } else {
                if constexpr (std::is_signed_v<T>) {
                    if constexpr (std::numeric_limits<T>::digits > std::numeric_limits<int64_t>::digits) {
                        if (value < static_cast<T>(std::numeric_limits<int64_t>::min())
                            || value > static_cast<T>(std::numeric_limits<uint64_t>::max())) {
                            throw std::domain_error("JsonWriter: integer exceeds the 64-bit DOM range");
                        }
                    }
                } else if constexpr (std::numeric_limits<T>::digits > std::numeric_limits<uint64_t>::digits) {
                    if (value > static_cast<T>(std::numeric_limits<uint64_t>::max())) {
                        throw std::domain_error("JsonWriter: integer exceeds the 64-bit DOM range");
                    }
                }
                const auto [end, error] = std::to_chars(buffer, buffer + sizeof(buffer), value);
                writeNumberText(buffer, end, error, false);
            }
        }

        void writeNumberText(const char *begin, const char *end, std::errc error, bool floating);

        template<typename T>
        static bool encodesNull(const T &value) {
            if constexpr (std::is_same_v<T, std::nullptr_t>) {
                return true;
            } else if constexpr (is_json_optional<T>) {
                return !value || encodesNull(*value);
            } else {
                return false;
            }
        }

        template<typename T>
        void writeValue(const T &value) {
            using ValueT = std::remove_cvref_t<T>;

            if constexpr (std::is_same_v<ValueT, std::nullptr_t>) {
                writeRaw("null");
            } else if constexpr (std::is_same_v<ValueT, bool>) {
                writeRaw(value ? "true" : "false");
            } else if constexpr (std::is_integral_v<ValueT>
                                 || std::is_floating_point_v<ValueT>) {
                writeNumber(value);
            } else if constexpr (std::is_same_v<ValueT, std::string>
                                 || std::is_same_v<ValueT, std::string_view>) {
                writeString(value);
            } else if constexpr (is_json_optional<ValueT>) {
                if (value) {
                    if (encodesNull(*value)) {
                        throw std::domain_error("JsonWriter: engaged optional payload would be lost as null");
                    }
                    writeValue(*value);
                } else {
                    writeRaw("null");
                }
            } else if constexpr (is_json_array<ValueT>) {
                openContainer(ContainerKind::Array);
                for (const auto &element: value) {
                    write(element);
                }
                endArray();
            } else if constexpr (std::is_class_v<ValueT>) {
                openContainer(ContainerKind::Object);
                detail::JsonMetaClass<ValueT>::forEach(
                    value,
                    [this](const meta::MetaProperty &property, const auto &fieldValue) {
                        write(property.name, fieldValue);
                    }
                );
                endObject();
            } else {
                static_assert(false, "JsonWriter: unsupported type");
            }
        }
    };

    template<typename T>
    void toJson(const T &obj, std::ostream &output, const size_t indent = 2) {
        JsonWriter(output, indent).write(obj);
    }

    template<typename T>
    std::string toJson(const T &obj, const size_t indent = 2) {
        std::ostringstream os;
        os.exceptions(std::ios::badbit | std::ios::failbit);
        toJson(obj, os, indent);
        return os.str();
    }
}
