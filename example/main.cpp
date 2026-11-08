#include <rj/JsonWriter.h>
#include <rj/JsonReader.h>

#include <iostream>
#include <sstream>
#include <string>

namespace {
    struct Human {
        [[= rj::rename{"first_name"}]]
        std::string name;
    };

    struct Employee final : Human {
        int id;
        double salary;
        [[= rj::ignore{}]]
        std::string password;
    };
}

int main() {
    std::stringstream stream(R"({"id": 42, "first_name": "Alice", "salary": 5000.5})");
    rj::toJson(rj::fromJson<Employee>(stream), std::cout);
    std::cout << '\n';
    return 0;
}
