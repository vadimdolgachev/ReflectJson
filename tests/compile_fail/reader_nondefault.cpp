#include <rj/JsonReader.h>
#include <sstream>
struct Value { int number; explicit Value(int number) : number(number) {} };
int main() {
    std::istringstream input{R"({"number":1})"};
    (void) rj::fromJson<Value>(input);
}
