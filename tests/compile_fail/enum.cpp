#include <rj/JsonWriter.h>
#include <map>
#include <variant>
#include <utility>

enum class Value { item }; int main() { rj::toJson(Value::item); }
