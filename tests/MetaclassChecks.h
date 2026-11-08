#pragma once

#include <sstream>

namespace {
    using PersonMeta = meta::MetaClass<Person>;
    static_assert(PersonMeta::fieldCount == 4);
    static_assert(PersonMeta::fields.size() == PersonMeta::fieldCount);
    static_assert(PersonMeta::fields[0].property().name == "id");
    static_assert(PersonMeta::fields[1].property().name == "name");
    static_assert(PersonMeta::fields[1].property().typeName == "std::string");
    static_assert(PersonMeta::fields[2].property().name == "salary");
    static_assert(PersonMeta::fields[3].property().name == "values");
    static_assert(std::is_same_v<meta::MetaClass<const Person &>::Class, Person>);
    static_assert(meta::MetaClass<const Person &>::fieldCount == PersonMeta::fieldCount);
    static_assert(meta::MetaClass<const Person &>::fields[1].property().typeName == "std::string");

    struct Scalar { int value; };
    constexpr bool check_constexpr_visit() {
        int visited = 0;
        meta::MetaClass<const Scalar &>::forEach(
            Scalar{7}, [&](const meta::MetaProperty &property, const auto &value) {
                visited = property.name == "value" ? value : -1;
            });
        meta::MetaClass<Empty>::forEach(Empty{}, [&](const auto &, const auto &) {
            visited = -1;
        });
        return visited == 7;
    }
    static_assert(check_constexpr_visit());
    static_assert(meta::MetaClass<Empty>::fieldCount == 0);
    static_assert(meta::MetaClass<Empty>::fields.empty());

    struct NonCopyable {
        int value = 17;
        NonCopyable() = default;
        NonCopyable(const NonCopyable &) = delete;
        NonCopyable(NonCopyable &&) = delete;
    };
    struct Holder { NonCopyable member; };

    struct CountingVisitor {
        int &calls;
        explicit CountingVisitor(int &calls) : calls(calls) {}
        CountingVisitor(const CountingVisitor &) = delete;
        CountingVisitor(CountingVisitor &&) = delete;
        template<typename T>
        void operator()(const meta::MetaProperty &, const T &) { ++calls; }
    };
    struct VisitStopped {};

    void verify_metadata(const Person &person) {
        std::vector<std::string_view> names;
        PersonMeta::forEach(person, [&](const meta::MetaProperty &property, const auto &value) {
            static_assert(std::is_lvalue_reference_v<decltype(value)>);
            static_assert(std::is_const_v<std::remove_reference_t<decltype(value)>>);
            using Value = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::is_same_v<Value, std::vector<int>>) {
                require(&value == &person.values, "Vector field was copied");
            } else if constexpr (std::is_same_v<Value, std::string>) {
                require(&value == &person.name, "String field was copied");
            } else if constexpr (std::is_same_v<Value, int>) {
                require(&value == &person.id, "Integer field was copied");
            } else if constexpr (std::is_same_v<Value, double>) {
                require(&value == &person.salary, "Double field was copied");
            }
            const auto &expected = PersonMeta::properties[names.size()];
            require(property.name == expected.name && property.typeName == expected.typeName,
                    "Visitor received incorrect property metadata");
            names.push_back(property.name);
        });
        require(names == std::vector<std::string_view>{"id", "name", "salary", "values"},
                "Visitor order is incorrect");

        int calls = 0;
        CountingVisitor visitor(calls);
        PersonMeta::forEach(person, visitor);
        require(calls == 4, "Lvalue visitor call count is incorrect");
        PersonMeta::forEach(person, CountingVisitor(calls));
        require(calls == 8, "Rvalue visitor call count is incorrect");
        meta::MetaClass<Empty>::forEach(Empty{}, visitor);
        require(calls == 8, "Empty structure invoked the visitor");

        const Holder holder{};
        int holder_visits = 0;
        meta::MetaClass<Holder>::forEach(holder, [&](const auto &, const auto &value) {
            require(&value == &holder.member, "Noncopyable field address changed");
            ++holder_visits;
        });
        require(holder_visits == 1, "Noncopyable field was not visited");

        int interrupted_visits = 0;
        require_throws<VisitStopped>([&] {
            PersonMeta::forEach(person, [&](const auto &, const auto &) {
                if (++interrupted_visits == 2) {
                    throw VisitStopped{};
                }
            });
        });
        require(interrupted_visits == 2, "Visitor exception was not propagated");

        std::cerr << "Metadata and typed visitor checks passed.\n";
    }
}
