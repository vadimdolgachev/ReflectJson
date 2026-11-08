#include <rj/JsonReader.h>
#include <rj/JsonWriter.h>

#include <iostream>
#include <sstream>

namespace {
    int checks = 0;
    void require(bool condition, const char *message) {
        ++checks;
        if (!condition) { throw std::runtime_error(message); }
    }

    template<typename T>
    T read(std::string_view text) {
        std::istringstream input{std::string(text)};
        return rj::fromJson<T>(input);
    }

    template<typename T>
    void rejects(std::string_view text, std::string_view reason) {
        try { (void) read<T>(text); }
        catch (const rj::JsonError &error) {
            require(std::string_view(error.what()).find(reason) != std::string_view::npos, "Wrong reader error");
            return;
        }
        throw std::runtime_error("Expected reader rejection");
    }

    struct Human { std::string name; };
    struct Employee final : Human { int id; double salary; };
    using EmployeeMeta = meta::MetaClass<Employee>;
    static_assert(EmployeeMeta::fieldCount == 3 && EmployeeMeta::fields.size() == 3);
    static_assert(EmployeeMeta::fields[0].property().name == "name"
                  && EmployeeMeta::fields[0].property().typeName == "std::string");
    static_assert(EmployeeMeta::fields[1].property().name == "id"
                  && EmployeeMeta::fields[2].property().name == "salary");
    static_assert(meta::MetaClass<const Employee &>::fieldCount == EmployeeMeta::fieldCount);

    struct Root { int root = 1; };
    struct Middle : Root { int middle = 2; };
    struct Side { int side = 3; };
    struct Multiple : Middle, Side { int own = 4; };
    using MultipleMeta = meta::MetaClass<Multiple>;
    static_assert(MultipleMeta::fieldCount == 4);
    static_assert(MultipleMeta::fields[0].property().name == "root"
                  && MultipleMeta::fields[1].property().name == "middle");
    static_assert(MultipleMeta::fields[2].property().name == "side"
                  && MultipleMeta::fields[3].property().name == "own");
    static_assert([] {
        Multiple value{};
        int sum = 0;
        MultipleMeta::forEachMutable(value, [](const auto &, auto &field) { field += 10; });
        MultipleMeta::forEach(value, [&](const auto &, const auto &field) { sum += field; });
        return sum == 50 && value.root == 11 && value.side == 13;
    }());

    struct Foundation { int origin = 1; };
    struct Shared : Foundation { int shared = 2; std::optional<int> keep = 17; };
    struct Left : virtual Shared { int left = 3; };
    using SharedAlias = Shared;
    struct Right : virtual SharedAlias { int right = 4; };
    struct Diamond : Left, Right { int own = 5; };
    using DiamondMeta = meta::MetaClass<Diamond>;
    static_assert(DiamondMeta::fieldCount == 6);
    static_assert(DiamondMeta::fields[0].property().name == "origin"
                  && DiamondMeta::fields[1].property().name == "shared");
    static_assert(DiamondMeta::fields[2].property().name == "keep"
                  && DiamondMeta::fields[3].property().name == "left");
    static_assert(DiamondMeta::fields[4].property().name == "right"
                  && DiamondMeta::fields[5].property().name == "own");

    struct Empty {};
    struct EmptyLeft : Empty {};
    struct EmptyRight : Empty {};
    struct EmptyDiamond : EmptyLeft, EmptyRight {};
    struct EmptyDerived : EmptyDiamond { int value; };
    static_assert(meta::MetaClass<EmptyDiamond>::fieldCount == 0);
    static_assert(meta::MetaClass<EmptyDerived>::fieldCount == 1);

    struct Defaults { int required = 1; std::optional<int> keep = 18; };
    struct CustomDefaults : virtual Defaults {
        int own = 2;
        CustomDefaults() { keep = 99; }
    };

    void checkJson() {
        const auto employee = read<Employee>(R"({"id":42,"name":"Alice","salary":5000.5})");
        require(employee.name == "Alice" && employee.id == 42 && employee.salary == 5000.5, "Employee fields");
        const auto text = rj::toJson(employee, 0);
        require(text == R"({"name":"Alice","id":42,"salary":5000.5})", "Flat base-first JSON");
        require(rj::toJson(read<Employee>(text), 0) == text, "Employee round trip");
        const auto collection = read<std::vector<std::optional<Employee>>>('[' + text + ",null]");
        require(collection.size() == 2 && collection[0]->name == "Alice" && !collection[1], "Inherited container element");
        require(rj::toJson(collection, 0) == '[' + text + ",null]", "Inherited container round trip");
        rejects<Employee>(R"({"id":42,"salary":1})", "Missing required field: name");
        rejects<Employee>(R"({"id":42,"name":"A","salary":1,"extra":2})", "Unknown field: extra");
        rejects<Employee>(R"({"id":42,"name":"A","salary":1,"\u006eame":"B"})", "Duplicate object key: name");
        rejects<Employee>(R"({"id":42,"name":null,"salary":1})", "Expected string");

        const auto multiple = read<Multiple>(R"({"own":8,"side":7,"middle":6,"root":5})");
        require(multiple.root == 5 && multiple.middle == 6 && multiple.side == 7 && multiple.own == 8, "Multiple bases");
        require(rj::toJson(multiple, 0) == R"({"root":5,"middle":6,"side":7,"own":8})", "Multiple base order");
        require(rj::toJson(EmptyDiamond{}, 0) == "{}", "Empty repeated nonvirtual base");
        require(read<EmptyDerived>(R"({"value":9})").value == 9, "Empty bases omitted");

        auto diamond = read<Diamond>(R"({"own":50,"right":40,"left":30,"shared":20,"origin":10})");
        require(diamond.origin == 10 && diamond.shared == 20 && diamond.keep == 17
                && diamond.left == 30 && diamond.right == 40 && diamond.own == 50, "Virtual diamond fields/defaults");
        require(&static_cast<Left &>(diamond).shared == &static_cast<Right &>(diamond).shared, "Shared virtual object");
        const auto encoded = rj::toJson(diamond, 0);
        require(encoded == R"({"origin":10,"shared":20,"keep":17,"left":30,"right":40,"own":50})",
                "Shared virtual base and its nonvirtual ancestor visited once");
        require(rj::toJson(read<Diamond>(encoded), 0) == encoded, "Diamond round trip");
        auto defaults = read<CustomDefaults>(R"({"required":7,"own":8})");
        require(defaults.keep == 99 && defaults.required == 7 && defaults.own == 8, "Derived constructor's base defaults retained");
        require(!read<CustomDefaults>(R"({"required":7,"own":8,"keep":null})").keep, "Base optional reset");
        rejects<CustomDefaults>(R"({"own":8})", "Missing required field: required");

        size_t index = 0;
        DiamondMeta::forEachMutable(diamond, [&](const auto &property, auto &field) {
            const auto expected = DiamondMeta::properties[index++];
            require(property.name == expected.name && property.typeName == expected.typeName,
                    "Derived metadata changed");
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(field)>, int>) {
                const int *address = property.name == "origin" ? &diamond.origin
                    : property.name == "shared" ? &diamond.shared : property.name == "left" ? &diamond.left
                    : property.name == "right" ? &diamond.right : &diamond.own;
                require(&field == address, "Mutable visitor copied a base field");
                ++field;
            }
        });
        require(index == 6 && diamond.origin == 11 && diamond.shared == 21 && diamond.right == 41,
                "Mutable visitor updates actual base fields once");
    }

    struct NonCopyable {
        int value = 5;
        NonCopyable() = default;
        NonCopyable(const NonCopyable &) = delete;
        NonCopyable(NonCopyable &&) = delete;
    };
    struct ReadonlyBase {
        const int fixed;
        mutable int mutableValue;
        int &reference;
        unsigned bit : 1;
        NonCopyable payload;
    };
    struct ReadonlyDerived : ReadonlyBase { int own; };

    struct CountingVisitor {
        int &calls;
        explicit CountingVisitor(int &calls) : calls(calls) {}
        CountingVisitor(const CountingVisitor &) = delete;
        template<typename T> void operator()(const meta::MetaProperty &, const T &) { ++calls; }
    };
    struct Stop {};

    void checkConstTraversal() {
        int referenced = 3;
        const ReadonlyDerived object{{1, 2, referenced, 1, {}}, 4};
        using Meta = meta::MetaClass<ReadonlyDerived>;
        static_assert(Meta::fieldCount == 6);
        size_t index = 0;
        Meta::forEach(object, [&](const auto &property, auto &field) {
            const auto expected = Meta::properties[index++];
            require(property.name == expected.name && property.typeName == expected.typeName,
                    "Const visitor metadata changed");
            using Value = std::remove_cvref_t<decltype(field)>;
            if constexpr (std::is_same_v<Value, int>) {
                const int *expected = property.name == "fixed" ? &object.fixed
                    : property.name == "mutableValue" ? &object.mutableValue
                    : property.name == "reference" ? &referenced : &object.own;
                require(&field == expected, "Const visitor copied or detached a base field");
            } else if constexpr (std::is_same_v<Value, NonCopyable>) {
                require(&field == &object.payload, "Noncopyable base payload copied");
            } else {
                require(field == 1, "Inherited bitfield temporary");
            }
        });
        require(index == 6 && referenced == 3 && object.mutableValue == 2, "Const traversal modified data");
        require(rj::toJson(object, 0) == R"({"fixed":1,"mutableValue":2,"reference":3,"bit":1,"payload":{"value":5},"own":4})",
                "Output-only inherited fields still supported");
        int calls = 0;
        CountingVisitor visitor(calls);
        Meta::forEach(object, visitor);
        Meta::forEach(object, CountingVisitor(calls));
        require(calls == 12, "Noncopyable visitor");
        calls = 0;
        try {
            Meta::forEach(object, [&](const auto &, const auto &) { if (++calls == 2) { throw Stop{}; } });
            throw std::runtime_error("Expected visitor exception");
        } catch (const Stop &) { require(calls == 2, "Visitor exception not propagated"); }
    }
}

int main() {
    try {
        checkJson();
        checkConstTraversal();
        std::cout << "Inheritance checks passed: " << checks << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Inheritance failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
