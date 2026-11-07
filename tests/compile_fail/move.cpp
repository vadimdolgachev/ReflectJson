#include <rj/JsonWriter.h>
#include <map>
#include <variant>
#include <utility>

int main() { std::ostringstream os; rj::JsonWriter a(os); rj::JsonWriter b(std::move(a)); }
