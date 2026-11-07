#include <rj/JsonWriter.h>
#include <map>
#include <variant>
#include <utility>

union Value { int n; }; int main() { rj::toJson(Value{1}); }
