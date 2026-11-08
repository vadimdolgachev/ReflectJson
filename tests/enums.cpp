#include <rj/JsonReader.h>
#include <rj/JsonWriter.h>

#include <array>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
    size_t checks = 0;

    enum class Status { Hired, Fired = 5, Alias = Hired };
    enum Plain { Off = -3, On = 9 };
    enum class Wide : std::uint64_t { Max = std::numeric_limits<std::uint64_t>::max() };
    enum class Empty {};

    struct Base {
        [[= rj::rename{"state"}]]
        Status status = Status::Fired;
    };

    struct Record : Base {
        std::optional<Status> maybe;
        std::vector<Status> history;
        std::array<Status, 2> fixed;
        Status native[2];
        [[= rj::ignore{}]]
        Status ignored = static_cast<Status>(37);
    };

    struct Tracked {
        inline static int constructions = 0;
        int id = 10;
        Status status = Status::Hired;
        Tracked() { ++constructions; }
    };

    void require(const bool condition, const char *message) {
        ++checks;
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    template<typename T>
    T read(const std::string_view text) {
        std::istringstream input{std::string(text)};
        return rj::fromJson<T>(input);
    }

    template<typename Function>
    void rejectsJson(Function &&function, const std::string_view message) {
        try {
            function();
        } catch (const rj::JsonError &error) {
            require(std::string_view(error.what()) == message, "Unexpected enum reader diagnostic");
            return;
        }
        throw std::runtime_error("Expected enum JSON rejection");
    }

    template<typename T>
    void rejectsRead(const std::string_view text, const std::string_view message) {
        rejectsJson([&] { static_cast<void>(read<T>(text)); }, message);
    }

    template<typename T>
    void rejectsWrite(const T &value) {
        try {
            static_cast<void>(rj::toJson(value));
        } catch (const std::domain_error &error) {
            require(std::string_view(error.what()) == "JsonWriter: Enum not found",
                    "Unexpected enum writer diagnostic");
            return;
        }
        throw std::runtime_error("Expected unnamed enum rejection");
    }

    void checkConversions() {
        using Meta = meta::MetaEnum<Status>;
        require(Meta::fromString("Hired") == Status::Hired, "Zero-valued enumerator");
        require(Meta::fromString("Fired") == Status::Fired, "Named enumerator");
        require(Meta::fromString("Alias") == Status::Hired, "Alias input");
        require(!Meta::fromString("hired") && !Meta::fromString(""), "Names match exactly");
        require(!Meta::fromString(std::string_view("Fired\0x", 7)), "Embedded NUL does not truncate name");
        require(Meta::toString(Status::Alias) == "Hired", "First declared alias is canonical");
        require(Meta::toString(Status::Fired) == "Fired", "Enum name output");
        require(!Meta::toString(static_cast<Status>(37)), "Unnamed value has no name");
        require(!meta::MetaEnum<Empty>::fromString("") && !meta::MetaEnum<Empty>::toString(Empty{}),
                "Empty enum has no conversions");
    }

    void checkRoundTrips() {
        require(rj::toJson(Status::Alias, 0) == R"("Hired")", "Enum is written as canonical string");
        require(read<Status>(R"("Alias")") == Status::Hired, "JSON alias accepted");
        require(read<Status>(rj::toJson(Status::Fired, 0)) == Status::Fired, "Scoped enum round trip");
        require(read<Plain>(rj::toJson(Off, 0)) == Off && read<Plain>(rj::toJson(On, 0)) == On,
                "Unscoped enum round trip");
        require(read<Wide>(rj::toJson(Wide::Max, 0)) == Wide::Max, "Wide underlying value round trip");
        require(!read<std::optional<Status>>("null"), "Null optional enum");
        require(read<std::optional<Status>>(R"("Hired")") == Status::Hired, "Engaged optional enum");
        require(rj::toJson(std::optional<Status>{}, 0) == "null", "Empty optional output");
        require(rj::toJson(std::optional<Status>{Status::Fired}, 0) == R"("Fired")", "Optional enum output");

        Record original{};
        original.status = Status::Hired;
        original.maybe = Status::Fired;
        original.history = {Status::Alias, Status::Fired};
        original.fixed = {Status::Fired, Status::Hired};
        original.native[0] = Status::Hired;
        original.native[1] = Status::Fired;
        const auto text = rj::toJson(original, 0);
        require(text == R"({"state":"Hired","maybe":"Fired","history":["Hired","Fired"],"fixed":["Fired","Hired"],"native":["Hired","Fired"]})",
                "Inherited, renamed and ignored enum fields");
        const auto copy = read<Record>(text);
        require(rj::toJson(copy, 0) == text, "Reflected enum containers round trip");
        require(copy.ignored == static_cast<Status>(37), "Ignored enum retains default");

        require(!read<std::optional<Empty>>("null"), "Optional empty enum accepts null");
        require(rj::toJson(std::optional<Empty>{}, 0) == "null", "Optional empty enum writes null");
        require(read<std::vector<Empty>>("[]").empty(), "Empty enum array accepts no elements");
        require(rj::toJson(std::vector<Empty>{}, 0) == "[]", "Empty enum array output");
    }

    void checkErrors() {
        for (const auto text: {"0", "true", "null", "{}", "[]"}) {
            rejectsRead<Status>(text, "Expected string");
        }
        rejectsRead<Status>(R"("Missing")", "Enum not found: Missing");
        rejectsRead<Status>(R"("")", "Enum not found: ");
        rejectsRead<Status>(R"("Fired\u0000x")", "Enum not found: Fired\\u0000x");
        rejectsRead<std::vector<Status>>(R"(["Hired","Missing"])", "Enum not found: Missing");
        rejectsRead<Empty>(R"("Missing")", "Enum not found: Missing");
        rejectsRead<std::optional<Empty>>(R"("Missing")", "Enum not found: Missing");
        rejectsWrite(static_cast<Status>(37));
        rejectsWrite(Empty{});
        rejectsWrite(std::optional<Empty>{std::in_place});

        Tracked::constructions = 0;
        rejectsRead<Tracked>(R"({"id":42,"status":"Missing"})", "Enum not found: Missing");
        require(Tracked::constructions == 0, "Invalid enum rejected before construction");
        Tracked target;
        std::istringstream invalid(R"({"id":42,"status":"Missing"})");
        rejectsJson([&] { rj::fromJson(invalid, target); }, "Enum not found: Missing");
        require(target.id == 10 && target.status == Status::Hired, "Invalid enum preserves existing object");
        require(Tracked::constructions == 1, "Updating target does not construct another object");
    }

    void checkUpdates() {
        Status state = Status::Hired;
        std::istringstream root(R"("Fired")");
        rj::mergePatch(root, state);
        require(state == Status::Fired, "Root enum patch");
        std::istringstream invalidRoot(R"("Missing")");
        rejectsJson([&] { rj::mergePatch(invalidRoot, state); }, "Enum not found: Missing");
        require(state == Status::Fired, "Invalid patch preserves root enum");

        Tracked target;
        std::istringstream patch(R"({"status":"Alias"})");
        rj::mergePatch(patch, target);
        require(target.id == 10 && target.status == Status::Hired, "Enum field patch accepts alias");
        std::istringstream invalid(R"({"id":42,"status":"Missing"})");
        rejectsJson([&] { rj::mergePatch(invalid, target); }, "Enum not found: Missing");
        require(target.id == 10 && target.status == Status::Hired, "Invalid enum patch preserves all fields");
        std::istringstream complete(R"({"id":12,"status":"Fired"})");
        rj::fromJson(complete, target);
        require(target.id == 12 && target.status == Status::Fired, "Complete enum object update");
    }
}

int main() {
    try {
        checkConversions();
        checkRoundTrips();
        checkErrors();
        checkUpdates();
        std::cout << "Enum checks passed: " << checks << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Enum failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
