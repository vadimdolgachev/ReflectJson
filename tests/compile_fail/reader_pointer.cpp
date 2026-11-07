#include <rj/JsonReader.h>
#include <sstream>
int main() {
    std::istringstream input{"null"};
    (void) rj::fromJson<int *>(input);
}
