#include "payload.h"
#include <rj/JsonWriter.h>
#include <rj/JsonReader.h>
#include <sstream>
const meta::MetaProperty *peer_properties() {
    return meta::MetaClass<Payload>::properties.data();
}
std::string peer_json(const Payload &value) {
    return rj::toJson(value, 0);
}
Payload peer_read(std::string_view text) {
    std::istringstream input{std::string(text)};
    return rj::fromJson<Payload>(input);
}

const meta::MetaProperty *peer_inherited_properties() {
    return meta::MetaClass<InheritedPayload>::properties.data();
}
std::string peer_json(const InheritedPayload &value) {
    return rj::toJson(value, 0);
}
InheritedPayload peer_read_inherited(std::string_view text) {
    std::istringstream input{std::string(text)};
    return rj::fromJson<InheritedPayload>(input);
}
