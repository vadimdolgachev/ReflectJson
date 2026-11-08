#include <rj/JsonSchema.h>

#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace {
    size_t checks = 0;

    void require(const bool condition, const char *message) {
        ++checks;
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    struct Person {
        int id;
        std::string name;
        double salary;
        std::vector<int> values;
    };

    struct Nested {
        std::optional<int> count;
        std::array<std::string, 2> names;
        int values[2];
    };

    struct Base {
        [[= rj::rename{"renamed"}]]
        int value;
    };

    struct Derived final : Base {
        [[= rj::ignore{}]]
        std::string ignored;
        std::optional<bool> active;
    };

    struct Escaped {
        [[= rj::rename{"a\"b\n"}]]
        int value;
    };

    void checkPerson() {
        const std::string expected = R"({
  "type": "object",
  "properties": {
    "id": {"type": "integer"},
    "name": {"type": "string"},
    "salary": {"type": "number"},
    "values": {
      "type": "array",
      "items": {"type": "integer"}
    }
  },
  "required": ["id", "name", "salary", "values"]
})";
        require(rj::schema<Person>() == expected, "Pretty Person schema");
        require(rj::schema<Person>(0) == R"({"type":"object","properties":{"id":{"type":"integer"},"name":{"type":"string"},"salary":{"type":"number"},"values":{"type":"array","items":{"type":"integer"}}},"required":["id","name","salary","values"]})",
                "Compact Person schema");
    }

    void checkScalarsAndContainers() {
        require(rj::schema<bool>(0) == R"({"type":"boolean"})", "Boolean schema");
        require(rj::schema<int>(0) == R"({"type":"integer"})", "Integer schema");
        require(rj::schema<double>(0) == R"({"type":"number"})", "Number schema");
        require(rj::schema<std::string>(0) == R"({"type":"string"})", "String schema");
        require(rj::schema<std::string_view>(0) == R"({"type":"string"})", "String view schema");
        require(rj::schema<std::nullptr_t>(0) == R"({"type":"null"})", "Null schema");
        require(rj::schema<std::optional<int>>(0) == R"({"type":["integer","null"]})", "Optional schema");
        require(rj::schema<std::optional<std::optional<int>>>(0) == R"({"type":["integer","null"]})",
                "Nested optional schema");
        require(rj::schema<std::optional<std::nullptr_t>>(0) == R"({"type":"null"})",
                "Optional null schema");
        require(rj::schema<std::optional<std::vector<int>>>(0)
                == R"({"type":["array","null"],"items":{"type":"integer"}})", "Optional array schema");
        require(rj::schema<std::vector<std::optional<std::string>>>(0)
                == R"({"type":"array","items":{"type":["string","null"]}})", "Array items schema");
        require(rj::schema<Nested>(0)
                == R"({"type":"object","properties":{"count":{"type":["integer","null"]},"names":{"type":"array","items":{"type":"string"}},"values":{"type":"array","items":{"type":"integer"}}},"required":["names","values"]})",
                "Nested object schema");
    }

    void checkMetadata() {
        require(rj::schema<Derived>(0)
                == R"({"type":"object","properties":{"renamed":{"type":"integer"},"active":{"type":["boolean","null"]}},"required":["renamed"]})",
                "Inheritance and annotations schema");
        require(rj::schema<Escaped>(0)
                == R"({"type":"object","properties":{"a\"b\n":{"type":"integer"}},"required":["a\"b\n"]})",
                "Escaped property name");
    }
}

int main() {
    try {
        checkPerson();
        checkScalarsAndContainers();
        checkMetadata();
        std::cout << "Schema checks passed: " << checks << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Schema failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
