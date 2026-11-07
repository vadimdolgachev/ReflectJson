#pragma once
#include <rj/MetaClass.h>
#include <rj/Annotations.h>
#include <string>
#include <vector>
struct Payload {
    int id;
    std::string name;
    std::vector<int> values;
};
const meta::MetaProperty *peer_properties();
std::string peer_json(const Payload &value);
Payload peer_read(std::string_view text);

struct Identity { [[= rj::rename{"full_name"}]] std::string name; };
struct InheritedPayload : virtual Identity {
    int id;
    [[= rj::ignore{}]] std::string password = "local";
};
const meta::MetaProperty *peer_inherited_properties();
std::string peer_json(const InheritedPayload &value);
InheritedPayload peer_read_inherited(std::string_view text);
