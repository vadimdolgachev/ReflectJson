#include <rj/JsonReader.h>
#include <sstream>
int number = 0;
struct Value { int &reference = number; };
int main() {
    std::istringstream input{R"({"reference":1})"};
    (void) rj::fromJson<Value>(input);
}
