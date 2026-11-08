#include <rj/JsonReader.h>
#include <rj/JsonWriter.h>

#include <iostream>
#include <map>

namespace other {
    struct ignore {};
    struct rename { int value; };
}

namespace {
    int checks = 0;
    int referenced = 91;
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
    void rejects(std::string_view text, std::string_view reason = {}) {
        try { (void) read<T>(text); }
        catch (const rj::JsonError &error) {
            require(std::string_view(error.what()).find(reason) != std::string_view::npos, "Unexpected reader diagnostic");
            return;
        }
        throw std::runtime_error("Expected JsonError");
    }

    struct Account {
        [[= rj::rename{"full_name"}]] std::string name;
        int id;
        [[= rj::ignore{}]] std::string password = "local";
    };
    using Raw = meta::MetaClass<Account>;
    using Json = rj::detail::JsonMetaClass<Account>;
    static_assert(Raw::fieldCount == 3 && Raw::fields[0].property().name == "name"
                  && Raw::fields[2].property().name == "password");
    static_assert(Json::fieldCount == 2 && Json::fields[0].property().name == "full_name");
    static_assert(Json::fields[0].property().typeName == "std::string" && Json::ignoredNames[0] == "password");

    void checkBasic() {
        Account account{"Alice", 42, "secret-original"};
        const auto text = rj::toJson(account, 0);
        require(text == R"({"full_name":"Alice","id":42})", "Password leaked or rename missing");
        auto decoded = read<Account>(text);
        require(decoded.name == "Alice" && decoded.id == 42 && decoded.password == "local", "Constructor defaults changed");
        require(rj::toJson(decoded, 0) == text, "Projected round trip changed JSON");
        for (std::string_view value: {R"("remote")", "null", "123", R"({"nested":[1,true]})"}) {
            decoded = read<Account>(R"({"id":42,"full_name":"Alice","password":)" + std::string(value) + '}');
            require(decoded.password == "local", "Ignored input assigned a value");
        }
        decoded = read<Account>(R"({"id":42,"full_name":"Alice","pass\u0077ord":false})");
        require(decoded.password == "local", "Escaped ignored key not recognized");
        rejects<Account>(R"({"id":42,"name":"Alice"})", "Missing required field: full_name");
        rejects<Account>(R"({"id":42,"full_name":"Alice","name":"old"})", "Unknown field: name");
        rejects<Account>(R"({"id":42,"full_name":"Alice","other":1})", "Unknown field: other");
        size_t index = 0;
        Raw::forEach(account, [&](const auto &property, const auto &value) {
            const auto &expected = Raw::properties[index++];
            require(property.name == expected.name && property.typeName == expected.typeName,
                    "Raw property metadata changed");
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, std::string>) {
                if (property.name == "password") { require(&value == &account.password, "Raw visitor skipped ignored field"); }
            }
        });
        require(index == 3, "Raw visitor filtered JSON annotations");
        Raw::forEachMutable(account, [](const auto &property, auto &value) {
            if constexpr (std::is_same_v<std::remove_cvref_t<decltype(value)>, std::string>) {
                if (property.name == "password") { value = "manual"; }
            }
        });
        require(account.password == "manual", "Raw mutable traversal changed semantics");
        std::ostringstream manual;
        rj::JsonWriter writer(manual, 0);
        writer.beginObject();
        writer.write("password", std::string("explicit"));
        writer.endObject();
        require(manual.str() == R"({"password":"explicit"})", "Manual writer interpreted field annotations");
    }

    using IgnoreAlias = rj::ignore;
    using RenameAlias = rj::rename;
    inline constexpr rj::ignore skip;
    struct IgnoredTypes {
        int id;
        [[= IgnoreAlias{}]] const int fixed = 7;
        [[= skip]] int &reference = referenced;
        [[= rj::ignore{}, = rj::ignore{}]] unsigned bit : 1 = 1;
        [[= rj::ignore{}]] int *pointer = &referenced;
        [[= rj::ignore{}]] std::map<std::string, int> cache{{"local", 5}};
        [[= rj::ignore{}]] std::string_view view = "owned elsewhere";
        [[= other::ignore{}]] int visible = 3;
        [[= other::rename{1}]] int ordinary = 4;
        [[= RenameAlias{"renamed"}]] std::optional<int> maybe = 18;
    };
    class SecretBase {
        [[= rj::ignore{}]] std::string token = "base-secret";
    public:
        [[= rj::rename{"full_name"}]] std::string name;
        const std::string &tokenValue() const { return token; }
    };
    struct Left : virtual SecretBase { int left; };
    struct Right : virtual SecretBase { int right; };
    struct Diamond : Left, Right { [[= rj::ignore{}]] std::string password = "default"; int id; };

    void checkIgnoredTypesAndInheritance() {
        const auto value = read<IgnoredTypes>(R"({"id":42,"visible":3,"ordinary":4,"fixed":0,"reference":0,
            "bit":0,"pointer":{},"cache":false,"view":null})");
        require(value.fixed == 7 && value.reference == 91 && referenced == 91 && value.bit == 1,
                "Ignored immutable/reference fields were modified");
        require(value.pointer == &referenced && value.cache.at("local") == 5 && value.view == "owned elsewhere", "Ignored unsupported types changed");
        require(value.maybe == 18, "Absent renamed optional lost initializer");
        require(!read<IgnoredTypes>(R"({"id":42,"visible":3,"ordinary":4,"renamed":null})").maybe, "Renamed optional null");
        require(rj::toJson(value, 0) == R"({"id":42,"visible":3,"ordinary":4,"renamed":18})", "Unrelated annotations or alias matching");
        const auto diamond = read<Diamond>(R"({"full_name":"Alice","left":1,"right":2,"id":42,
            "token":{"ignored":true},"password":"remote"})");
        require(diamond.name == "Alice" && diamond.tokenValue() == "base-secret" && diamond.password == "default", "Inherited private ignore/default");
        const auto text = rj::toJson(diamond, 0);
        require(text == R"({"full_name":"Alice","left":1,"right":2,"id":42})", "Virtual annotations or ordering");
        require(rj::toJson(read<std::vector<Diamond>>('[' + text + ']'), 0) == '[' + text + ']', "Nested annotated values");
    }

    struct Keys {
        [[= rj::rename{""}]] int empty;
        [[= rj::rename{"x\0y"}]] int nul;
        [[= rj::rename{"quote\"\\\n"}]] int escaped;
        [[= rj::rename{"имя"}]] int unicode;
    };
    struct Base { [[= rj::rename{"base_id"}]] int id; };
    struct Derived : Base { int id; };
    struct IgnoredBase { [[= rj::ignore{}]] int id = 99; };
    struct VisibleDerived : IgnoredBase { int id; };
    struct AllIgnored { [[= rj::ignore{}]] std::string password = "default"; };

    void checkNamesAndFiltering() {
        constexpr std::string_view text = R"({"":1,"x\u0000y":2,"quote\"\\\n":3,"имя":4})";
        const auto keys = read<Keys>(text);
        require(keys.empty == 1 && keys.nul == 2 && keys.escaped == 3 && keys.unicode == 4, "Special renamed keys");
        require(rj::toJson(keys, 0) == text, "Escaping or name length changed");
        const auto derived = read<Derived>(R"({"base_id":1,"id":2})");
        require(derived.Base::id == 1 && derived.id == 2, "Renamed shadowed member access");
        require(rj::toJson(derived, 0) == R"({"base_id":1,"id":2})", "JSON view rejected resolvable C++ name collision");
        const auto visible = read<VisibleDerived>(R"({"id":5})");
        require(visible.IgnoredBase::id == 99 && visible.id == 5, "Active field did not take precedence over ignored name");
        rejects<VisibleDerived>(R"({"id":null})", "Expected number");
        require(rj::toJson(visible, 0) == R"({"id":5})", "Ignored duplicate C++ name not removed");
        const auto ignored = read<AllIgnored>(R"({"password":false})");
        require(ignored.password == "default" && rj::toJson(ignored, 0) == "{}", "Empty JSON projection");
        rejects<AllIgnored>(R"({"other":1})", "Unknown field: other");
    }

    struct Constructed {};
    struct SyntaxProbe {
        [[= rj::ignore{}]] std::string password;
        SyntaxProbe() { throw Constructed{}; }
    };
    void checkFullValidation() {
        rejects<SyntaxProbe>(R"({"password":1,"pass\u0077ord":2})", "Duplicate object key");
        rejects<SyntaxProbe>(R"({"password":18446744073709551616})");
        rejects<SyntaxProbe>(R"({"password":[1,]})");
        rejects<SyntaxProbe>("{\"password\":" + std::string(256, '[') + "0" + std::string(256, ']') + '}');
        try {
            (void) read<SyntaxProbe>(R"({"password":{"valid":true}})");
            throw std::runtime_error("Constructor exception disappeared");
        } catch (const Constructed &) { ++checks; }
    }
}

int main() {
    try {
        checkBasic();
        checkIgnoredTypesAndInheritance();
        checkNamesAndFiltering();
        checkFullValidation();
        std::cout << "Annotation checks passed: " << checks << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Annotation failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
