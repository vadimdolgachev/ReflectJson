#pragma once

#include <istream>
#include <string>
#include <vector>

namespace rj::detail {
    enum class JsonKind {
        Null,
        Boolean,
        Int64,
        UInt64,
        Double,
        String,
        Array,
        Object
    };

    struct JsonMember;

    struct JsonValue final {
        JsonKind kind = JsonKind::Null;
        bool boolean = false;
        int64_t signedNumber = 0;
        uint64_t unsignedNumber = 0;
        double floatingNumber = 0;
        std::string text;
        std::vector<JsonValue> elements;
        std::vector<JsonMember> members;
    };

    struct JsonMember final {
        std::string name;
        JsonValue value;
    };

    JsonValue readDocument(std::istream &input);
}
