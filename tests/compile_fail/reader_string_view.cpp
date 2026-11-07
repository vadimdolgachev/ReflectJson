#include <rj/JsonReader.h>
#include <sstream>
int main() {
    std::istringstream input{R"("text")"};
    (void) rj::fromJson<std::string_view>(input);
}
