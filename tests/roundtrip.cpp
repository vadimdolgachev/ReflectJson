#include <rj/JsonReader.h>
#include <rj/JsonWriter.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>

namespace {
    int checks = 0;

    void require(const bool condition, const char *message) {
        ++checks;
        if (!condition) { throw std::runtime_error(message); }
    }

    template<typename Function>
    void rejects(Function operation) {
        try { operation(); }
        catch (const std::domain_error &) { ++checks; return; }
        throw std::runtime_error("Expected writer domain error");
    }

    template<typename T>
    T roundTrip(const T &value, const size_t indent = 0) {
        const auto document = rj::toJson(value, indent);
        std::istringstream input(document);
        auto result = rj::fromJson<T>(input);
        require(result == value, "Value changed during writer/reader round trip");
        if constexpr (std::is_floating_point_v<T>) {
            require(std::signbit(result) == std::signbit(value), "Floating-point sign changed");
        }
        require(rj::toJson(result, indent) == document, "Second serialization changed the document");
        return result;
    }

    void checkNumbers() {
        for (const auto value: {std::numeric_limits<std::int64_t>::min(), std::int64_t{-1},
                               std::int64_t{0}, std::numeric_limits<std::int64_t>::max()}) { roundTrip(value); }
        roundTrip(std::numeric_limits<std::uint64_t>::max());
        for (const double value: {0.0, -0.0, 1.0, -1.0, 0.1, std::nextafter(1.0, 2.0),
                                  std::numeric_limits<double>::min(), std::numeric_limits<double>::denorm_min(),
                                  std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest()}) {
            roundTrip(value);
            roundTrip(static_cast<long double>(value));
        }
        for (const float value: {0.0f, -0.0f, 1.0f, 0.1f, std::nextafter(1.0f, 2.0f),
                                 std::numeric_limits<float>::min(), std::numeric_limits<float>::denorm_min(),
                                 std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest()}) { roundTrip(value); }
        require(rj::toJson(1.0, 0) == "1.0", "Whole floating value must retain its DOM type");
        require(rj::toJson(-0.0, 0) == "-0.0", "Negative zero must use a floating token");
        require(rj::toJson(0, 0) == "0", "Integer token changed");
        std::uint64_t state = 0x1bd11bdaa9fc1a22;
        for (int i = 0; i < 2048; ++i) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            const auto wide = std::bit_cast<double>(state);
            const auto narrow = std::bit_cast<float>(static_cast<std::uint32_t>(state));
            if (std::isfinite(wide)) { roundTrip(wide); }
            if (std::isfinite(narrow)) { roundTrip(narrow); }
        }
#if defined(__SIZEOF_INT128__)
        roundTrip(static_cast<__int128>(std::numeric_limits<std::int64_t>::min()));
        roundTrip(static_cast<__int128>(std::numeric_limits<std::uint64_t>::max()));
        roundTrip(static_cast<unsigned __int128>(std::numeric_limits<std::uint64_t>::max()));
        rejects([] { rj::toJson(static_cast<__int128>(std::numeric_limits<std::int64_t>::min()) - 1); });
        rejects([] { rj::toJson(static_cast<__int128>(std::numeric_limits<std::uint64_t>::max()) + 1); });
        rejects([] { rj::toJson(static_cast<unsigned __int128>(std::numeric_limits<std::uint64_t>::max()) + 1); });
#endif
        if constexpr (std::numeric_limits<long double>::digits > std::numeric_limits<double>::digits) {
            rejects([] { rj::toJson(std::nextafter(1.0L, 2.0L)); });
        }
        if constexpr (std::numeric_limits<long double>::max_exponent > std::numeric_limits<double>::max_exponent) {
            rejects([] { rj::toJson(std::numeric_limits<long double>::max()); });
            rejects([] { rj::toJson(std::numeric_limits<long double>::lowest()); });
            rejects([] { rj::toJson(std::numeric_limits<long double>::min()); });
        }
        rejects([] { rj::toJson(std::numeric_limits<double>::infinity()); });
        rejects([] { rj::toJson(std::numeric_limits<double>::quiet_NaN()); });
    }

    struct Payload {
        std::int64_t low;
        std::uint64_t high;
        double zero;
        std::vector<float> numbers;
        std::vector<bool> bits;
        std::vector<std::optional<std::string>> text;
        std::optional<std::optional<int>> nested;
        int grid[2][2];
        bool operator==(const Payload &) const = default;
    };

    void checkContainers() {
        const Payload value{std::numeric_limits<std::int64_t>::min(), std::numeric_limits<std::uint64_t>::max(),
            -0.0, {0.1f, -0.0f, std::numeric_limits<float>::max()}, {true, false},
            {std::string("a\0b", 3), std::string("Ω😀"), std::nullopt},
            std::optional<std::optional<int>>(std::in_place, 42), {{1, 2}, {3, 4}}};
        for (const size_t indent: {0u, 2u}) {
            const auto result = roundTrip(value, indent);
            require(std::signbit(result.zero) && std::signbit(result.numbers[1]), "Nested zero sign changed");
        }
        roundTrip(std::optional<std::optional<int>>{});
        roundTrip(std::optional<std::nullptr_t>{});
        roundTrip(std::array<double, 0>{});
        rejects([] { rj::toJson(std::optional<std::optional<int>>(std::in_place)); });
        rejects([] { rj::toJson(std::optional<std::nullptr_t>{nullptr}); });
        rejects([] { rj::toJson(std::vector<std::optional<std::optional<int>>>{
            std::optional<std::optional<int>>(std::in_place)}); });
    }

    void checkTextAndKeys() {
        roundTrip(std::string("\0\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf", 26));
        for (const std::string invalid: {"\x80", "\xc0\x80", "\xc2 ", "\xe0\x80\x80", "\xed\xa0\x80",
                                         "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\xff", "\xe2\x82"}) {
            require(rj::toJson(invalid, 0) == '"' + invalid + '"', "Writer changed input bytes");
            std::ostringstream output;
            rj::JsonWriter writer(output, 0);
            writer.beginObject();
            writer.write(invalid, 1);
            writer.endObject();
            require(output.str() == "{\"" + invalid + "\":1}", "Writer changed key bytes");
        }
        for (const auto &name: {std::string{}, std::string("a\0b", 3), std::string("Ω\\\n")}) {
            std::ostringstream output;
            rj::JsonWriter writer(output, 0);
            writer.beginObject();
            writer.write(name, 1);
            const auto before = output.str();
            rejects([&] { writer.write(std::string(name), 2); });
            require(output.str() == before, "Duplicate key changed output");
            writer.endObject();
        }
        std::ostringstream output;
        rj::JsonWriter writer(output, 0);
        writer.beginObject();
        std::string name = "original";
        writer.write(name, 1);
        name = "child";
        writer.beginObject(name);
        writer.write("original", 2);
        writer.endObject();
        rejects([&] { writer.write("original", 3); });
        writer.endObject();
        require(output.str() == R"({"original":1,"child":{"original":2}})", "Key lifetime or scope changed");
    }

    struct Chain {
        std::vector<Chain> children;
        bool operator==(const Chain &) const = default;
    };

    Chain chain(const int length) {
        Chain root;
        auto *current = &root;
        for (int i = 1; i < length; ++i) {
            current->children.emplace_back();
            current = &current->children.front();
        }
        return root;
    }

    void checkDepth() {
        roundTrip(chain(128)); // Each node contributes one object and one array.
        rejects([] { rj::toJson(chain(129), 0); });
        std::ostringstream output;
        rj::JsonWriter writer(output, 0);
        for (int i = 0; i < 256; ++i) { writer.beginArray(); }
        rejects([&] { writer.beginArray(); });
    }
}

int main() {
    try {
        checkNumbers();
        checkContainers();
        checkTextAndKeys();
        checkDepth();
        std::cout << "Round-trip checks passed: " << checks << '\n';
    } catch (const std::exception &error) {
        std::cerr << "Round-trip failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
