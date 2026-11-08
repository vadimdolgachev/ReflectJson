#pragma once

#include <array>
#include <meta>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace meta {
    namespace detail {
        consteval std::string_view typeName(const std::meta::info &type) {
            if (std::meta::is_same_type(type, ^^std::string)) {
                return "std::string";
            }
            return std::meta::display_string_of(type);
        }

        struct FieldSchema final {
            std::span<const std::meta::info> fields;
            std::span<const std::meta::info> ignoredFields;
            std::string_view annotationError;
            bool isPublicBases = true;
            bool isAccessibleFields = true;
            bool isNamedFields = true;
            bool isUniqueNames = true;
        };

        struct FieldProjection final {
            bool ignored = false;
            std::string_view name{};
            std::string_view error{};
        };

        struct FullFieldPolicy final {
            static constexpr std::string_view DuplicateNameMessage = "MetaClass requires unique data member names";

            static consteval FieldProjection project(const std::meta::info field) {
                return {.name = std::meta::identifier_of(field)};
            }
        };

        consteval void collectFields(const std::meta::info type,
                                     const std::meta::access_context context,
                                     std::vector<std::meta::info> &fields,
                                     std::vector<std::meta::info> &virtualBases,
                                     FieldSchema &schema) {
            for (const auto base: std::meta::bases_of(type, std::meta::access_context::unchecked())) {
                // Check every edge, even a second path to an already visited virtual base.
                schema.isPublicBases &= std::meta::is_public(base);
                const auto baseType = std::meta::type_of(base);
                if (std::meta::is_virtual(base)) {
                    bool visited = false;
                    for (const auto prior: virtualBases) {
                        visited |= std::meta::is_same_type(prior, baseType);
                    }
                    if (visited) {
                        continue;
                    }
                    virtualBases.push_back(baseType);
                }
                collectFields(baseType, context, fields, virtualBases, schema);
            }
            for (const auto field: std::meta::nonstatic_data_members_of(type, std::meta::access_context::unchecked())) {
                fields.push_back(field);
            }
        }

        template<typename FieldPolicy>
        consteval FieldSchema getFieldSchema(const std::meta::info type,
                                             const std::meta::access_context context) {
            FieldSchema schema{};
            if (!std::meta::is_class_type(type)) {
                return schema;
            }
            std::vector<std::meta::info> collected;
            std::vector<std::meta::info> virtualBases;
            collectFields(type, context, collected, virtualBases, schema);
            std::vector<std::meta::info> fields;
            std::vector<std::meta::info> ignored;
            std::vector<std::string_view> names;
            for (const auto field: collected) {
                if (!std::meta::has_identifier(field)) {
                    schema.isNamedFields = false;
                    continue;
                }
                const auto projection = FieldPolicy::project(field);
                if (!projection.error.empty()) {
                    if (schema.annotationError.empty()) {
                        schema.annotationError = projection.error;
                    }
                    continue;
                }
                if (projection.ignored) {
                    ignored.push_back(field);
                    continue;
                }
                schema.isAccessibleFields &= std::meta::is_accessible(field, context);
                for (const auto name: names) {
                    if (name == projection.name) {
                        schema.isUniqueNames = false;
                    }
                }
                names.push_back(projection.name);
                fields.push_back(field);
            }
            schema.fields = std::define_static_array(fields);
            schema.ignoredFields = std::define_static_array(ignored);
            return schema;
        }
    }

    struct MetaProperty final {
        std::string_view name;
        std::string_view typeName;
    };

    template<typename T, typename FieldPolicy = detail::FullFieldPolicy>
    struct MetaClass final {
        using Class = std::remove_cvref_t<T>;

        static_assert(std::is_class_v<Class>, "MetaClass requires a class type");

        static constexpr auto context = std::meta::access_context::current();

    private:
        static constexpr auto schema = detail::getFieldSchema<FieldPolicy>(^^Class, context);
        static constexpr bool isValidSchema = schema.isPublicBases
                                              && schema.isAccessibleFields
                                              && schema.isNamedFields
                                              && schema.isUniqueNames
                                              && schema.annotationError.empty();

    public:
        static_assert(schema.isPublicBases, "MetaClass requires all base classes to be public");
        static_assert(schema.isAccessibleFields, "MetaClass requires all data members to be public");
        static_assert(schema.isNamedFields, "MetaClass requires named data members");
        static_assert(schema.annotationError.empty(), schema.annotationError);
        static_assert(schema.isUniqueNames, FieldPolicy::DuplicateNameMessage);

        static constexpr auto fields = schema.fields;
        static constexpr size_t fieldCount = fields.size();

        static constexpr auto ignoredNames = [] {
            std::array<std::string_view, schema.ignoredFields.size()> result{};
            [[maybe_unused]] size_t index = 0;
            template for (constexpr auto field: schema.ignoredFields) {
                result[index++] = std::meta::identifier_of(field);
            }
            return result;
        }();

        static constexpr auto properties = [] {
            std::array<MetaProperty, fieldCount> result{};
            [[maybe_unused]] size_t index = 0;
            if constexpr (isValidSchema) {
                template for (constexpr auto field: fields) {
                    result[index++] = MetaProperty{
                        .name = FieldPolicy::project(field).name,
                        .typeName = detail::typeName(std::meta::type_of(field))
                    };
                }
            }

            return result;
        }();

        template<typename Visitor>
        static constexpr void forEach(const Class &obj, Visitor &&visitor) {
            [[maybe_unused]] size_t index = 0;
            if constexpr (isValidSchema) {
                template for (constexpr auto field: fields) {
                    visitor(properties[index++], obj.[:field:]);
                }
            }
        }

        template<typename Visitor>
        static constexpr void forEachMutable(Class &obj, Visitor &&visitor) {
            [[maybe_unused]] size_t index = 0;
            if constexpr (isValidSchema) {
                template for (constexpr auto field: fields) {
                    constexpr auto type = std::meta::type_of(field);
                    static_assert(!std::meta::is_reference_type(type),
                                  "MetaClass mutable traversal does not support reference members");
                    static_assert(!std::meta::is_const_type(type),
                                  "MetaClass mutable traversal does not support const members");
                    static_assert(!std::meta::is_bit_field(field),
                                  "MetaClass mutable traversal does not support bitfields");
                    if constexpr (!std::meta::is_reference_type(type)
                                  && !std::meta::is_const_type(type)
                                  && !std::meta::is_bit_field(field)) {
                        visitor(properties[index++], obj.[:field:]);
                    }
                }
            }
        }
    };
}
