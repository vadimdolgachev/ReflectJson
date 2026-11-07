#include <rj/JsonReader.h>
#include <sstream>
struct Value {
    int number;
    Value() = default;
    Value(const Value &) = delete;
    Value(Value &&) = delete;
};
int main() {
    std::istringstream input{R"({"number":1})"};
    (void) rj::fromJson<Value>(input);
}
