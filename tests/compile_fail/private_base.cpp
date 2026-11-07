#include <rj/JsonWriter.h>
#include <map>
#include <variant>
#include <utility>

struct Base {}; class Value : private Base {}; int main() { rj::toJson(Value{}); }
