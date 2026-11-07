#include <rj/JsonReader.h>
#include <sstream>
int main() {
    std::istringstream input{"1"};
    (void) rj::fromJson<int &>(input);
}
