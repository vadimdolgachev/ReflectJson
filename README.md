# ReflectJson

A static C++26 library for JSON serialization and deserialization using reflection.
It supports reflected structs, inheritance, numbers, strings, arrays, `std::vector`,
and `std::optional`. JSON parsing is implemented internally without external dependencies.

## Requirements

- CMake 3.30 or newer.
- GCC 17.0.0 (20261104, experimental) or another compiler with equivalent C++26
  reflection support, including `<meta>`, `-freflection`, and P3394 annotations.

## Build and test

```sh
cmake -S . -B build/library
cmake --build build/library --parallel 2
cmake --install build/library --prefix "$PWD/build/install"

cmake -S example -B build/example -DCMAKE_PREFIX_PATH="$PWD/build/install"
cmake --build build/example --parallel 2
./build/example/example

cmake -S tests -B build/tests
cmake --build build/tests --parallel 2
ctest --test-dir build/tests --output-on-failure
```

Add `-DCMAKE_CXX_COMPILER=/path/to/g++` to configure commands when selecting
the experimental compiler explicitly. A top-level build also builds the example.

## Usage

```cpp
#include <rj/JsonWriter.hpp>
#include <rj/JsonReader.hpp>
#include <iostream>
#include <sstream>
#include <string>

struct Person {
    int id;
    std::string name;
};

int main() {
    const auto text = rj::toJson(Person{42, "Alice"});
    std::istringstream input(text);
    const auto person = rj::fromJson<Person>(input);
    std::cout << person.name << '\n';
}
```

`toJson(value, 0)` produces compact JSON. Field annotations `rj::rename` and
`rj::ignore` control JSON names and participation. Non-optional fields are required;
missing optionals retain their defaults. Unknown and duplicate keys are rejected.

## CMake integration

```cmake
add_subdirectory(ReflectJson)
target_link_libraries(app PRIVATE ReflectJson::ReflectJson)
```

For an installed package, use `find_package(ReflectJson CONFIG REQUIRED)` and set
`CMAKE_PREFIX_PATH` to the installation prefix.
