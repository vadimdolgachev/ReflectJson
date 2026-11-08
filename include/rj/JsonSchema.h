#pragma once

#include <rj/JsonWriter.h>

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace rj {
    namespace detail {
        inline void writeSchemaType(JsonWriter &writer, const std::string_view type, const bool nullable) {
            if (!nullable || type == "null") {
                writer.write("type", type);
                return;
            }
            writer.beginArray("type");
            writer.write(type);
            writer.write(std::string_view("null"));
            writer.endArray();
        }

        template<typename T>
        void writeSchemaBody(JsonWriter &writer, const bool nullable = false) {
            using Value = std::remove_cvref_t<T>;
            if constexpr (is_json_optional<Value>) {
                writeSchemaBody<typename Value::value_type>(writer, true);
            } else if constexpr (std::is_same_v<Value, std::nullptr_t>) {
                writeSchemaType(writer, "null", nullable);
            } else if constexpr (std::is_same_v<Value, bool>) {
                writeSchemaType(writer, "boolean", nullable);
            } else if constexpr (std::is_integral_v<Value>) {
                writeSchemaType(writer, "integer", nullable);
            } else if constexpr (std::is_floating_point_v<Value>) {
                writeSchemaType(writer, "number", nullable);
            } else if constexpr (std::is_same_v<Value, std::string>
                                 || std::is_same_v<Value, std::string_view>) {
                writeSchemaType(writer, "string", nullable);
            } else if constexpr (is_json_array<Value>) {
                writeSchemaType(writer, "array", nullable);
                writer.beginObject("items");
                if constexpr (std::is_array_v<Value>) {
                    writeSchemaBody<std::remove_extent_t<Value> >(writer);
                } else {
                    writeSchemaBody<typename Value::value_type>(writer);
                }
                writer.endObject();
            } else if constexpr (std::is_class_v<Value>) {
                using Meta = JsonMetaClass<Value>;
                writeSchemaType(writer, "object", nullable);
                writer.beginObject("properties");
                size_t index = 0;
                template for (constexpr auto field: Meta::fields) {
                    using Field = [:std::meta::type_of(field.reflection):];
                    writer.beginObject(Meta::properties[index++].name);
                    writeSchemaBody<Field>(writer);
                    writer.endObject();
                }
                writer.endObject();
                writer.beginArray("required");
                index = 0;
                template for (constexpr auto field: Meta::fields) {
                    using FieldType = [:std::meta::type_of(field.reflection):];
                    using Field = std::remove_cvref_t<FieldType>;
                    if constexpr (!is_json_optional<Field>) {
                        writer.write(Meta::properties[index].name);
                    }
                    ++index;
                }
                writer.endArray();
            } else if constexpr (std::is_enum_v<Value>) {
                using Meta = meta::MetaEnum<Value>;
                if constexpr (Meta::enumerators.empty()) {
                    if (nullable) {
                        writeSchemaType(writer, "null", false);
                    } else {
                        writer.beginObject("not");
                        writer.endObject();
                    }
                } else {
                    writeSchemaType(writer, "string", nullable);
                    writer.beginArray("enum");
                    for (const auto name: Meta::properties) {
                        writer.write(name);
                    }
                    if (nullable) {
                        writer.write(nullptr);
                    }
                    writer.endArray();
                }
            } else {
                static_assert(false, "JsonSchema: unsupported type");
            }
        }

        inline bool isMultilineSchemaObject(const std::string_view text, const size_t open) {
            bool inString = false;
            bool escaped = false;
            for (size_t i = open + 1; i < text.size(); ++i) {
                const char ch = text[i];
                if (inString) {
                    if (escaped) {
                        escaped = false;
                    } else if (ch == '\\') {
                        escaped = true;
                    } else if (ch == '"') {
                        inString = false;
                    }
                } else if (ch == '"') {
                    inString = true;
                } else if (ch == '{') {
                    return true;
                } else if (ch == '}') {
                    return false;
                }
            }
            return false;
        }

        inline void appendSchemaIndent(std::string &output, const size_t depth, const size_t indent) {
            output += '\n';
            output.append(depth * indent, ' ');
        }

        inline std::string formatSchema(const std::string_view text, const size_t indent) {
            std::string output;
            output.reserve(text.size() * 2);
            std::vector<bool> multiline;
            bool inString = false;
            bool escaped = false;
            for (size_t i = 0; i < text.size(); ++i) {
                const char ch = text[i];
                if (inString) {
                    output += ch;
                    if (escaped) {
                        escaped = false;
                    } else if (ch == '\\') {
                        escaped = true;
                    } else if (ch == '"') {
                        inString = false;
                    }
                    continue;
                }
                if (ch == '"') {
                    inString = true;
                    output += ch;
                    continue;
                }
                switch (ch) {
                    case '{': {
                        const bool expanded = isMultilineSchemaObject(text, i);
                        multiline.push_back(expanded);
                        output += ch;
                        if (expanded && text[i + 1] != '}') {
                            appendSchemaIndent(output, multiline.size(), indent);
                        }
                        break;
                    }
                    case '[':
                        multiline.push_back(false);
                        output += ch;
                        break;
                    case '}': {
                        const bool expanded = multiline.back();
                        multiline.pop_back();
                        if (expanded) {
                            appendSchemaIndent(output, multiline.size(), indent);
                        }
                        output += ch;
                        break;
                    }
                    case ']':
                        multiline.pop_back();
                        output += ch;
                        break;
                    case ',':
                        if (multiline.back()) {
                            output += ch;
                            appendSchemaIndent(output, multiline.size(), indent);
                        } else {
                            output += ", ";
                        }
                        break;
                    case ':':
                        output += ": ";
                        break;
                    default:
                        output += ch;
                        break;
                }
            }
            return output;
        }
    }

    template<typename T>
    std::string schema(const size_t indent = 2) {
        std::ostringstream output;
        output.exceptions(std::ios::badbit | std::ios::failbit);
        JsonWriter writer(output, 0);
        writer.beginObject();
        detail::writeSchemaBody<T>(writer);
        writer.endObject();
        const auto compact = output.str();
        return indent == 0 ? compact : detail::formatSchema(compact, indent);
    }
}
