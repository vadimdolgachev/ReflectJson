#include <rj/JsonReader.h>
#include <sstream>
struct Value { const int number = 0; };
int main() {
    std::istringstream input{R"({"number":1})"};
    (void) rj::fromJson<Value>(input);
}
