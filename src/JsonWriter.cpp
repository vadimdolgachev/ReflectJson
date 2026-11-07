#include <rj/JsonWriter.h>

#include "rj/detail/JsonLimits.h"

namespace rj {
    JsonWriter::JsonWriter(std::ostream &os, const size_t indent)
        : os(os), indent(indent) {
    }

    void JsonWriter::beginObject() {
        beforeValue(std::nullopt);
        openContainer(ContainerKind::Object);
    }

    void JsonWriter::beginObject(std::string_view name) {
        beforeValue(name);
        openContainer(ContainerKind::Object);
    }

    void JsonWriter::endObject() {
        closeContainer(ContainerKind::Object);
    }

    void JsonWriter::beginArray() {
        beforeValue(std::nullopt);
        openContainer(ContainerKind::Array);
    }

    void JsonWriter::beginArray(std::string_view name) {
        beforeValue(name);
        openContainer(ContainerKind::Array);
    }

    void JsonWriter::endArray() {
        closeContainer(ContainerKind::Array);
    }

    void JsonWriter::writeRaw(std::string_view text) {
        if (!text.empty()) {
            os.write(text.data(), static_cast<std::streamsize>(text.size()));
        }
    }

    void JsonWriter::writeIndent(const size_t depth) {
        if (indent > 0) {
            os.put('\n');
            writeRaw(std::string(depth * indent, ' '));
        }
    }

    void JsonWriter::writeNumberText(const char *begin, const char *end, const std::errc error, const bool floating) {
        if (error != std::errc{}) {
            throw std::runtime_error("JsonWriter could not format a number");
        }
        const std::string_view text(begin, end);
        writeRaw(text);
        if (floating && text.find_first_of(".eE") == std::string_view::npos) {
            writeRaw(".0");
        }
    }

    void JsonWriter::beforeValue(const std::optional<std::string_view> &name) {
        if (containers.empty()) {
            if (name) {
                throw std::logic_error("JsonWriter: named values require an object");
            }
            if (root_written) {
                throw std::logic_error("JsonWriter: only one root value is allowed");
            }
            root_written = true;
            return;
        }

        auto &[kind, first, names] = containers.back();
        if ((kind == ContainerKind::Object) != name.has_value()) {
            throw std::logic_error("JsonWriter: value name does not match container");
        }
        if (name && !names.emplace(*name).second) {
            throw std::domain_error("JsonWriter: duplicate object key");
        }
        if (!first) {
            os.put(',');
        }
        writeIndent(containers.size());
        if (name) {
            writeString(*name);
            writeRaw(indent > 0 ? ": " : ":");
        }
        first = false;
    }

    void JsonWriter::openContainer(const ContainerKind kind) {
        if (containers.size() == detail::MaxDepth) {
            throw std::domain_error("JSON nesting exceeds 256 containers");
        }
        os.put(kind == ContainerKind::Object ? '{' : '[');
        containers.push_back({.kind = kind, .names = {}});
    }

    void JsonWriter::closeContainer(const ContainerKind kind) {
        if (containers.empty() || containers.back().kind != kind) {
            throw std::logic_error("JsonWriter: mismatched container end");
        }
        const bool empty = containers.back().first;
        containers.pop_back();
        if (!empty) {
            writeIndent(containers.size());
        }
        os.put(kind == ContainerKind::Object ? '}' : ']');
    }

    void JsonWriter::writeString(std::string_view text) {
        // Input text is assumed to be valid UTF-8.
        constexpr std::string_view hex = "0123456789abcdef";
        os.put('"');
        for (const unsigned char ch: text) {
            switch (ch) {
                case '"': writeRaw("\\\"");
                    break;
                case '\\': writeRaw("\\\\");
                    break;
                case '\b': writeRaw("\\b");
                    break;
                case '\f': writeRaw("\\f");
                    break;
                case '\n': writeRaw("\\n");
                    break;
                case '\r': writeRaw("\\r");
                    break;
                case '\t': writeRaw("\\t");
                    break;
                default:
                    if (ch < 0x20) {
                        writeRaw("\\u00");
                        os.put(hex[ch >> 4]);
                        os.put(hex[ch & 0x0f]);
                    } else {
                        os.put(static_cast<char>(ch));
                    }
            }
        }
        os.put('"');
    }
}
