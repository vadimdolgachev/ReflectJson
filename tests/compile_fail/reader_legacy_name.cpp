#include <rj/JsonReader.h>
#include <sstream>
int main() {
    std::istringstream input("1");
    (void) rj::from_json<int>(input);
}
