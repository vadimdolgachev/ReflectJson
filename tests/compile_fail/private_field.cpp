#include <rj/JsonWriter.h>
#include <map>
#include <variant>
#include <utility>

class Value { int hidden = 1; public: int visible = 2; }; int main() { rj::toJson(Value{}); }
