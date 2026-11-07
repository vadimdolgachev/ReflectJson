#include <rj/JsonReader.h>
#include <sstream>
enum class Value { First };
int main() {
    std::istringstream input{"0"};
    (void) rj::fromJson<Value>(input);
}
