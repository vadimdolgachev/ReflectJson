#include <rj/JsonWriter.h>
#include <map>
#include <variant>
#include <utility>

int main() { rj::toJson(std::variant<int, double>{}); }
