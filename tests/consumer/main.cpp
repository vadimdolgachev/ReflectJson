#include <rj/JsonWriter.h>
#include <rj/JsonReader.h>
#include "payload.h"
#include <iostream>
#include <sstream>
int main() {
    const Payload value{42, "external", {1, 2}};
    const auto actual = rj::toJson(value, 0);
    std::istringstream input(actual);
    if (actual != "{\"id\":42,\"name\":\"external\",\"values\":[1,2]}"
        || peer_json(value) != actual
        || rj::toJson(rj::fromJson<Payload>(input), 0) != actual
        || rj::toJson(peer_read(actual), 0) != actual
        || peer_properties() != meta::MetaClass<Payload>::properties.data()
        || meta::MetaClass<Payload>::properties[1].type != "std::string") {
        std::cerr << "Public header, multi-TU or JSON conversion regression\n";
        return 1;
    }
    InheritedPayload derived{};
    derived.name = "base";
    derived.id = 7;
    derived.password = "secret";
    const auto inherited = rj::toJson(derived, 0);
    std::istringstream inherited_input(inherited);
    if (inherited != R"({"full_name":"base","id":7})"
        || peer_json(derived) != inherited
        || rj::toJson(peer_read_inherited(inherited), 0) != inherited
        || rj::toJson(rj::fromJson<InheritedPayload>(inherited_input), 0) != inherited
        || peer_inherited_properties() != meta::MetaClass<InheritedPayload>::properties.data()
        || meta::MetaClass<InheritedPayload>::fieldCount != 3
        || meta::MetaClass<InheritedPayload>::properties[0].name != "name"
        || peer_read_inherited(inherited).password != "local") {
        std::cerr << "Inherited metadata or installed-consumer regression\n";
        return 1;
    }
}
