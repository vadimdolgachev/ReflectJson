#include <rj/JsonReader.h>
#include <sstream>
union Value { int number; double floating; };
int main() {
    std::istringstream input{"{}"};
    (void) rj::fromJson<Value>(input);
}
