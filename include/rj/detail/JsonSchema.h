#pragma once

#include <rj/Annotations.h>
#include <rj/MetaClass.h>

namespace rj::detail {
    struct JsonFieldPolicy {
        static constexpr std::string_view DuplicateNameMessage = "ReflectJson requires unique JSON field names";

        static consteval meta::detail::FieldProjection project(const std::meta::info field) {
            const bool ignored = !std::meta::annotations_of_with_type(field, ^^ignore).empty();
            const auto names = std::meta::annotations_of_with_type(field, ^^rename);
            if (ignored && !names.empty()) {
                return {.error = "ReflectJson: ignore and rename cannot be combined"};
            }
            if (names.size() > 1) {
                return {.error = "ReflectJson: multiple rename annotations are not allowed"};
            }
            if (ignored) {
                return {.ignored = true};
            }
            if (!names.empty()) {
                const auto name = std::meta::extract<rename>(names.front());
                return {.name = std::string_view(name.data, name.size)};
            }
            return {.name = std::meta::identifier_of(field)};
        }
    };

    template<typename T>
    using JsonMetaClass = meta::MetaClass<T, JsonFieldPolicy>;
}
