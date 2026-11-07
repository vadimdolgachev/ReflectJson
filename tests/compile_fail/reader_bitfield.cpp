#include <rj/JsonReader.h>
#include <sstream>
struct Value { unsigned bit : 1; };
int main() {
    std::istringstream input{R"({"bit":1})"};
    (void) rj::fromJson<Value>(input);
}
