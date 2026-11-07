#include <rj/JsonReader.h>
#include <rj/JsonWriter.h>

#include <algorithm>
#include <iostream>
#include <locale>
#include <sstream>
#include <streambuf>

namespace {
    size_t checks = 0;

    void require(const bool condition, const std::string &message) {
        ++checks;
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    // Text fixtures enter the library through its public stream API.
    template<typename T>
    T readDocument(const std::string_view document) {
        std::istringstream input{std::string(document)};
        return rj::fromJson<T>(input);
    }

    template<typename Function>
    void rejects(Function &&function, const std::string_view reason = {}) {
        try {
            function();
        } catch (const rj::JsonError &error) {
            require(std::string_view(error.what()).find(reason) != std::string_view::npos,
                    "Unexpected error: " + std::string(error.what()));
            return;
        }
        throw std::runtime_error("Expected JsonError: " + std::string(reason));
    }

    template<typename T>
    void rejectsValue(const std::string_view text, const std::string_view reason = {}) {
        rejects([&] { (void) readDocument<T>(text); }, reason);
    }

    struct SyntaxAccepted {};
    struct SyntaxProbe { SyntaxProbe() { throw SyntaxAccepted{}; } };

    void rejectSyntax(std::string_view input) {
        rejects([&] {
            try { (void) readDocument<SyntaxProbe>(input); }
            catch (const SyntaxAccepted &) { throw std::runtime_error("Invalid JSON was accepted"); }
        });
    }

    void acceptSyntax(std::string_view input) {
        try { (void) readDocument<SyntaxProbe>(input); }
        catch (const SyntaxAccepted &) { ++checks; return; }
        throw std::runtime_error("Syntax probe constructor was not called");
    }

    struct Person {
        int id;
        std::string name;
        double salary;
        std::vector<int> values;
        bool operator==(const Person &) const = default;
    };

    struct Required {
        int id = 41;
        std::optional<int> keep = 7;
    };

    struct Inner {
        int required = 5;
        std::optional<int> keep = 17;
    };

    struct Outer {
        Inner child{1, 99};
        std::optional<Inner> maybe = Inner{2, 100};
        std::vector<int> data{9};
    };

    struct OptionalChild {
        std::optional<Inner> maybe;
    };

    struct Collections {
        int raw[2][2];
        char letters[3];
        std::array<int, 0> empty;
        std::array<std::array<double, 2>, 1> block;
        std::vector<bool> bits;
        std::vector<Inner> children;
        std::optional<std::vector<int>> maybe;
        std::nullptr_t nothing;
    };

    struct Empty {};

    struct MoveOnly {
        int value;
        MoveOnly() = default;
        MoveOnly(const MoveOnly &) = delete;
        MoveOnly(MoveOnly &&) = default;
    };

    struct CopyOnly {
        int value;
        CopyOnly() = default;
        CopyOnly(const CopyOnly &) = default;
        CopyOnly(CopyOnly &&) = delete;
    };

    struct Coordinates {
        int x;
        int y;
    };

    static_assert([] {
        Coordinates value{};
        meta::MetaClass<Coordinates>::forEachMutable(value, [](const auto &, auto &field) { field = 3; });
        meta::MetaClass<Coordinates>::forEachMutable(value, [](const auto &, auto &field) { ++field; });
        return value.x == 4 && value.y == 4;
    }());

    struct MutableVisitor {
        int calls = 0;
        MutableVisitor() = default;
        MutableVisitor(const MutableVisitor &) = delete;
        void operator()(const meta::MetaProperty &, int &field) { ++calls; field += calls; }
    };

    void checkMutableMetadata() {
        Coordinates point{};
        MutableVisitor visitor;
        meta::MetaClass<Coordinates>::forEachMutable(point, visitor);
        require(visitor.calls == 2 && point.x == 1 && point.y == 2, "Noncopyable mutable visitor");
        size_t index = 0;
        meta::MetaClass<Coordinates>::forEachMutable(point, [&](const auto &property, auto &field) {
            require(&property == &meta::MetaClass<Coordinates>::properties[index], "Cached mutable property");
            require(&field == (index == 0 ? &point.x : &point.y), "Mutable field refers to original");
            ++index;
        });
        int calls = 0;
        try {
            meta::MetaClass<Coordinates>::forEachMutable(point, [&](const auto &, auto &) {
                ++calls;
                throw std::runtime_error("visitor error");
            });
            throw std::logic_error("Expected visitor error");
        } catch (const std::runtime_error &error) {
            require(std::string_view(error.what()) == "visitor error" && calls == 1,
                    "Mutable visitor exception stops traversal");
        }
    }

    void checkObjects() {
        const Person expected{42, "Alice", 5000.5, {1, 2, 3}};
        const auto person = readDocument<Person>(
            R"({"name":"Alice","values":[1,2,3],"salary":5000.5,"id":42})");
        require(person == expected, "Reordered object fields");
        require(readDocument<Person>(rj::toJson(expected)) == expected, "Person round trip");
        const std::string escaped_key = R"({"\u0069d":42})";
        require(readDocument<Required>(escaped_key).id == 42, "Decoded member names");
        require(readDocument<Required>(escaped_key).keep == 7, "Absent optional keeps initializer");
        require(!readDocument<Required>(R"({"id":42,"keep":null})").keep, "Null clears optional");
        require(readDocument<Required>(R"({"id":42,"keep":8})").keep == 8, "Present optional replaces scalar");
        rejectsValue<Required>("{}", "Missing required field: id");
        rejectsValue<Required>(R"({"keep":9})", "Missing required field: id");
        rejectsValue<Required>(R"({"id":null})", "Expected number");
        rejectsValue<Required>(R"({"id":1,"extra":2})", "Unknown field: extra");
        rejectsValue<Required>(R"({"id":1,"ID":2})", "Unknown field: ID");
        rejectsValue<Required>(R"({"id":1,"":2})", "Unknown field");
        rejectsValue<Required>(R"({"id":1,"x\u0000y":2})", "Unknown field");
        rejectsValue<Empty>(R"({"x":1})", "Unknown field: x");
        (void) readDocument<Empty>("{}");
        rejectsValue<Required>(R"({"id":1,"id":2})", "Duplicate object key: id");
        rejectsValue<Required>(R"({"id":1,"\u0069d":2})", "Duplicate object key: id");
        rejectsValue<Required>(R"([1])", "Expected object");

        const auto outer = readDocument<Outer>(R"({"child":{"required":3},"data":[]})");
        require(outer.child.required == 3 && outer.child.keep == 99, "Nested initialization retained");
        require(outer.maybe && outer.maybe->required == 2 && outer.maybe->keep == 100,
                "Absent optional object retained");
        require(outer.data.empty(), "Present vector replaces initialized vector");
        const auto updated = readDocument<Outer>(
            R"({"child":{"required":3,"keep":null},"maybe":{"required":4},"data":[1,2]})");
        require(!updated.child.keep && updated.maybe->required == 4 && updated.maybe->keep == 100,
                "Existing optional payload initialized in place");
        require(updated.data == std::vector<int>{1, 2}, "Vector contents");
        require(!readDocument<Outer>(R"({"child":{"required":3},"data":[],"maybe":null})").maybe,
                "Null clears optional object");
        require(readDocument<OptionalChild>(R"({"maybe":{"required":1}})").maybe->keep == 17,
                "Disengaged optional payload default constructed");
        rejectsValue<Outer>(R"({"child":{},"data":[]})", "Missing required field: required");
        rejectsValue<Outer>(R"({"child":{"required":1,"wrong":2},"data":[]})", "Unknown field: wrong");
        require(readDocument<MoveOnly>(R"({"value":13})").value == 13, "Move-only root");
        require(readDocument<CopyOnly>(R"({"value":14})").value == 14, "Copy-only root");
        require(readDocument<std::vector<MoveOnly>>(R"([{"value":15}])")[0].value == 15,
                "Move-only vector element");
        require(readDocument<std::vector<CopyOnly>>(R"([{"value":16}])")[0].value == 16,
                "Copy-only vector element");
    }

    void checkContainers() {
        const auto value = readDocument<Collections>(R"({
            "raw":[[1,2],[3,4]],"letters":[65,66,67],"empty":[],"block":[[1.5,2]],
            "bits":[true,false,true],"children":[{"required":8}],"maybe":[7,8],"nothing":null
        })");
        require(value.raw[0][1] == 2 && value.raw[1][0] == 3, "Nested C arrays");
        require(value.letters[0] == 'A' && value.letters[2] == 'C', "Char arrays contain numbers");
        require(value.block[0] == std::array<double, 2>{1.5, 2}, "Nested std::array");
        require(value.bits == std::vector<bool>{true, false, true}, "vector<bool>");
        require(value.children.size() == 1 && value.children[0].required == 8
                && value.children[0].keep == 17, "Vector of reflected values");
        require(value.maybe == std::vector<int>{7, 8} && value.nothing == nullptr, "Optional container and nullptr");
        require(readDocument<std::vector<std::optional<int>>>("[1,null,2]")
                == std::vector<std::optional<int>>{1, std::nullopt, 2}, "Vector of optional");
        require(readDocument<std::array<int, 2>>("[9,8]") == std::array<int, 2>{9, 8}, "Root fixed array");
        require(readDocument<std::array<int, 0>>("[]").empty(), "Root empty fixed array");
        require(readDocument<std::vector<int>>("[]").empty(), "Root empty vector");
        require(readDocument<std::optional<int>>("9") == 9, "Root optional");
        require(!readDocument<std::optional<int>>("null"), "Root empty optional");
        require(**readDocument<std::optional<std::optional<int>>>("9") == 9, "Nested optional");
        rejectsValue<std::array<int, 2>>("[1]", "Fixed array length mismatch");
        rejectsValue<std::array<int, 2>>("[1,2,3]", "Fixed array length mismatch");
        rejectsValue<std::array<int, 0>>("[1]", "Fixed array length mismatch");
        rejectsValue<std::array<std::array<int, 2>, 1>>("[[1]]", "Fixed array length mismatch");
        rejectsValue<std::vector<int>>("{}", "Expected array");
        rejectsValue<std::vector<bool>>("[1]", "Expected boolean");
        rejectsValue<std::vector<int>>("[null]", "Expected number");
        rejectsValue<std::optional<int>>(R"("1")", "Expected number");
    }

    template<typename T>
    void checkIntegral() {
        for (const T value: {T{0}, T{1}, std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max()}) {
            require(readDocument<T>(rj::toJson(value, 0)) == value, "Integer round trip");
        }
    }

    template<typename T>
    void checkFloating() {
        for (const T value: {T{0}, -T{0}, T{0.1}, std::nextafter(T{1}, T{2}),
                            std::numeric_limits<T>::min(), std::numeric_limits<T>::denorm_min(),
                            std::numeric_limits<T>::max(), std::numeric_limits<T>::lowest()}) {
            const auto decoded = readDocument<T>(rj::toJson(value, 0));
            require(decoded == value && std::signbit(decoded) == std::signbit(value),
                    "DOM floating round trip including negative zero");
        }
        rejectsValue<T>("1e99999");
        require(readDocument<T>("1e-99999") == 0, "DOM underflow to zero");
    }

    class DecimalComma final : public std::numpunct<char> {
        char do_decimal_point() const override { return ','; }
        char do_thousands_sep() const override { return '_'; }
        std::string do_grouping() const override { return "\3"; }
    };

    class LocaleGuard final {
    public:
        LocaleGuard() : previous(std::locale::global(std::locale(std::locale::classic(), new DecimalComma))) {}
        ~LocaleGuard() { std::locale::global(previous); }
    private:
        const std::locale previous;
    };

    void checkNumbers() {
        checkIntegral<char>();
        checkIntegral<signed char>();
        checkIntegral<unsigned char>();
        checkIntegral<short>();
        checkIntegral<unsigned short>();
        checkIntegral<int>();
        checkIntegral<unsigned>();
        checkIntegral<long>();
        checkIntegral<unsigned long>();
        checkIntegral<long long>();
        checkIntegral<unsigned long long>();
        checkIntegral<wchar_t>();
        checkIntegral<char8_t>();
        checkIntegral<char16_t>();
        checkIntegral<char32_t>();
        require(readDocument<char16_t>("937") == u'Ω', "Wide character uses integer conversion");
        require(readDocument<std::int64_t>("-9223372036854775808") == std::numeric_limits<std::int64_t>::min(),
                "Exact int64 minimum");
        require(readDocument<std::uint64_t>("18446744073709551615") == std::numeric_limits<std::uint64_t>::max(),
                "Exact uint64 maximum");
        rejectsValue<std::int64_t>("9223372036854775808");
        rejectsValue<std::int64_t>("-9223372036854775809");
        rejectsValue<std::uint64_t>("18446744073709551616");
        rejectsValue<unsigned>("-1", "Negative integer");
        require(readDocument<unsigned>("-0") == 0, "DOM integer zero has no negative sign");
        rejectsValue<signed char>("128", "out of range");
        rejectsValue<signed char>("-129", "out of range");
        rejectsValue<unsigned char>("256", "out of range");
        rejectsValue<char16_t>("65536", "out of range");
        require(readDocument<int>("-0") == 0, "Signed integer negative zero");
        for (const auto text: {"42.0", "4.2e1", "42e0"}) {
            rejectsValue<int>(text, "Expected an integer value");
        }
        rejectsValue<int>(R"("42")", "Expected number");
        rejectsValue<int>("true", "Expected number");
        rejectsValue<bool>("1", "Expected boolean");
        rejectsValue<bool>(R"("true")", "Expected boolean");
        require(readDocument<bool>("true") && !readDocument<bool>("false"), "Boolean values");
        require(readDocument<std::nullptr_t>("null") == nullptr, "Root nullptr");
        rejectsValue<std::nullptr_t>("0", "Expected null");
#if defined(__SIZEOF_INT128__)
        require(readDocument<__int128>("-9223372036854775808") == std::numeric_limits<std::int64_t>::min(),
                "Wide target accepts native signed integer");
        require(readDocument<__int128>("18446744073709551615") == static_cast<__int128>(std::numeric_limits<std::uint64_t>::max()),
                "Wide signed target accepts native uint64 maximum");
        require(readDocument<unsigned __int128>("18446744073709551615") == std::numeric_limits<std::uint64_t>::max(),
                "Wide unsigned target accepts native uint64 maximum");
        rejectsValue<__int128>("1267650600228229401496703205383");
        rejectsValue<unsigned __int128>("340282366920938463463374607431768211455");
#endif
        checkFloating<float>();
        checkFloating<double>();
        require(readDocument<long double>("0.1") == static_cast<long double>(0.1), "Long double uses DOM precision");
        rejectsValue<long double>("1e4932");
        require(!std::signbit(readDocument<double>("-0")), "DOM normalizes integer negative zero");
        require(std::signbit(readDocument<double>("-0.0")), "DOM preserves floating negative zero");
        require(readDocument<double>("1e-324") == 0, "Double underflow to zero");
        require(readDocument<double>("2e-324") == 0, "Double rounding below denorm minimum");
        require(readDocument<double>("3e-324") == std::numeric_limits<double>::denorm_min(),
                "Double rounding to denorm minimum");
        const auto negativeUnderflow = readDocument<double>("-1e-324");
        require(negativeUnderflow == 0 && std::signbit(negativeUnderflow),
                "Negative double underflow preserves zero sign");
        require(readDocument<double>("-12.5E+2") == -1250, "Exponent grammar");
        require(readDocument<double>("42") == 42.0, "Integer token to floating point");
        rejectsValue<float>("1e39", "out of range");
        rejectsValue<double>("1e309");
        const LocaleGuard locale;
        require(readDocument<double>("1234.5") == 1234.5, "Global locale cannot change JSON numbers");
    }

    void checkStrings() {
        require(readDocument<std::string>(R"("\"\\\/\b\f\n\r\t")") == "\"\\/\b\f\n\r\t", "All short escapes");
        require(readDocument<std::string>(R"("a\u0000b")") == std::string("a\0b", 3), "Escaped NUL owned");
        require(readDocument<std::string>(R"("\u0041\u03A9\u20ac\uD83D\uDE00")") == "AΩ€😀",
                "Unicode escapes and surrogate pair");
        require(readDocument<std::string>(R"("\u0000\u007f\u0080\u07ff\u0800\ud7ff\ue000\uffff\ud800\udc00\udbff\udfff")")
                == std::string("\0\x7f\xc2\x80\xdf\xbf\xe0\xa0\x80\xed\x9f\xbf\xee\x80\x80\xef\xbf\xbf\xf0\x90\x80\x80\xf4\x8f\xbf\xbf", 26),
                "Unicode encoding boundaries");
        const std::string utf8 = "Русский Ω € 😀";
        require(readDocument<std::string>('"' + utf8 + '"') == utf8, "Raw UTF-8 preserved");
        require(readDocument<std::string>(R"("")").empty(), "Empty string");
        const std::string owned = [] {
            std::string input = R"("text\nwith escape")";
            auto result = readDocument<std::string>(input);
            input.assign(input.size(), 'x');
            return result;
        }();
        require(owned == "text\nwith escape", "No references into parser/input storage");
        const std::string bounded = "xx\"value\"trailing";
        require(readDocument<std::string>(std::string_view(bounded).substr(2, 7)) == "value", "Bounded input view");
        rejectsValue<int>(std::string_view{});
    }

    void checkSyntax() {
        // Parser-level cases distinguish syntax errors from typed conversion errors.
        for (const std::string_view input: {
                 "", " ", "/", "/*x*/null", "//x\nnull", "undefined", "NULL", "True", "NaN", "Infinity",
                 "-Infinity", "+1", ".1", "01", "-01", "--1", "-", "1.", "1e", "1e+", "1e-", "1e+-1",
                 "0x1", "[1,]", "[,1]", "[1 2]", "[", "{", "{,}", "{1:2}", "{x:2}", "{\"x\" 2}",
                 "{\"x\":}", "{\"x\":1,}", "{\"x\":1 \"y\":2}", "true false", "nullx", "[1]]", "{}{}",
                 "\vnull", "\fnull", "\xef\xbb\xbfnull", "\"unterminated", "\"\\", "\"\\x\"", "\"\\u12\"",
                 "\"\\uZZZZ\"", "\"\\uD800\"", "\"\\uDC00\"", "\"\\uD800\\u0041\"", "\"\\uD800x\"",
                 "{\"a\":1,\"a\":2}", "{\"a\":1,\"\\u0061\":2}", "{\"\":1,\"\":2}",
                 "{\"x\\u0000y\":1,\"x\\u0000y\":2}",
                 "{\"unknown\":[{\"a\":1,\"\\u0061\":2}]}",
                 "{\"unknown\":1e99999}", "18446744073709551616", "-9223372036854775809"}) {
            rejectSyntax(input);
        }
        const std::string document = R"({"a":[true,false,null,-12.34e+5,{"s":"\uD83D\uDE00"}]})";
        for (size_t end = 0; end < document.size(); ++end) {
            const auto prefix = std::string_view(document).substr(0, end);
            rejectSyntax(prefix);
        }
        for (int ch = 0; ch < 0x20; ++ch) {
            const auto input = '"' + std::string(1, static_cast<char>(ch)) + '"';
            rejectsValue<std::string>(input);
        }
        for (const std::string bytes: {
                 "\x80", "\xbf", "\xc0\x80", "\xc1\xbf", "\xc2 ", "\xe0\x80\x80", "\xed\xa0\x80",
                 "\xf0\x80\x80\x80", "\xf4\x90\x80\x80", "\xf5\x80\x80\x80", "\xff", "\xe2\x82", "\xf0"}) {
            rejectsValue<std::string>('"' + bytes + '"');
        }
        rejectsValue<std::string>(std::string("\"\xe2\x82", 3));
        rejectsValue<std::string>(std::string("\"\xc2\"", 3));
        rejectsValue<int>("  1 2");
        rejectsValue<int>(" \n");
        rejectsValue<Required>("{\"id\":");
        rejectsValue<std::vector<int>>(" [1,\"é\",2]", "Expected number");
        const std::string after_utf8 = "[\"é\",true]";
        rejectsValue<std::vector<std::string>>(after_utf8, "Expected string");
        require(readDocument<int>(" \t\r\n42 \n") == 42, "Only JSON whitespace around root");
        const std::string nul_trailing("0\0", 2);
        rejectsValue<int>(nul_trailing);
    }

    struct Chain {
        std::vector<Chain> children;
    };

    std::string nestedChain(const size_t count) {
        std::string text;
        for (size_t i = 0; i < count; ++i) { text += "{\"children\":["; }
        for (size_t i = 0; i < count; ++i) { text += "]}"; }
        return text;
    }

    void checkDepth() {
        auto chain = readDocument<Chain>(nestedChain(128));
        const Chain *node = &chain;
        size_t count = 1;
        while (!node->children.empty()) {
            require(node->children.size() == 1, "Chain width");
            node = &node->children[0];
            ++count;
        }
        require(count == 128, "256 containers accepted");
        rejectsValue<Chain>(nestedChain(129));
        const std::string max_arrays = std::string(256, '[') + "0" + std::string(256, ']');
        acceptSyntax(max_arrays);
        const std::string too_deep = std::string(257, '[') + "0" + std::string(257, ']');
        rejectSyntax(too_deep);
    }

    class ThrowingBuffer final : public std::streambuf {
        std::streamsize xsgetn(char *, std::streamsize) override {
            throw std::runtime_error("input device failure");
        }
    };

    class PartialFailureBuffer final : public std::streambuf {
        bool delivered = false;
        std::streamsize xsgetn(char *output, const std::streamsize count) override {
            if (delivered) { throw std::runtime_error("input device failure"); }
            delivered = true;
            std::fill_n(output, count, ' ');
            return count;
        }
    };

    class AllocationFailureBuffer final : public std::streambuf {
        std::streamsize xsgetn(char *, std::streamsize) override { throw std::bad_alloc(); }
    };

    struct ConstructorError {};

    struct ThrowingConstructor {
        static inline int calls = 0;
        int value;
        ThrowingConstructor() { ++calls; throw ConstructorError{}; }
    };

    void checkStreamsAndExceptions() {
        for (const auto mask: {std::ios::goodbit, std::ios::badbit, std::ios::failbit,
                              std::ios::eofbit, std::ios::badbit | std::ios::failbit | std::ios::eofbit}) {
            std::istringstream input(R"({"id":42})");
            input.imbue(std::locale(std::locale::classic(), new DecimalComma));
            input.setf(std::ios::hex, std::ios::basefield);
            input.unsetf(std::ios::skipws);
            input.precision(3);
            input.width(11);
            input.fill('_');
            input.exceptions(mask);
            const auto flags = input.flags();
            const auto locale = input.getloc();
            require(rj::fromJson<Required>(input).id == 42, "Stream input and normal EOF");
            require(input.exceptions() == mask && input.flags() == flags && input.getloc() == locale
                    && input.precision() == 3 && input.width() == 11 && input.fill() == '_',
                    "Stream settings unchanged");
            require(input.eof() && !input.bad(), "Stream consumed to EOF");
            std::istringstream empty;
            empty.exceptions(mask);
            rejects([&] { (void) rj::fromJson<int>(empty); });
        }
        std::istringstream positioned("skip 42");
        positioned.seekg(5);
        require(rj::fromJson<int>(positioned) == 42, "Read from current stream position");
        for (const size_t length: {4094u, 9000u}) {
            const std::string expected(length, 'x');
            std::istringstream input('"' + expected + '"');
            input.exceptions(std::ios::eofbit | std::ios::failbit | std::ios::badbit);
            require(rj::fromJson<std::string>(input) == expected, "Chunked stream and exact block size");
        }
        for (const auto mask: {std::ios::goodbit, std::ios::badbit, std::ios::badbit | std::ios::failbit}) {
            ThrowingBuffer buffer;
            std::istream input(&buffer);
            input.exceptions(mask);
            rejects([&] { (void) rj::fromJson<int>(input); }, "Failed to read JSON stream");
            require(input.exceptions() == mask, "Failure preserves stream exception mask");
            PartialFailureBuffer partial_buffer;
            std::istream partial(&partial_buffer);
            partial.exceptions(mask);
            rejects([&] { (void) rj::fromJson<int>(partial); }, "Failed to read JSON stream");
        }
        std::istringstream failed("42");
        failed.setstate(std::ios::failbit);
        rejects([&] { (void) rj::fromJson<int>(failed); }, "not readable");
        std::istringstream bad("42");
        bad.setstate(std::ios::badbit | std::ios::eofbit);
        rejects([&] { (void) rj::fromJson<int>(bad); }, "not readable");
        std::istringstream trailing("42 true");
        rejects([&] { (void) rj::fromJson<int>(trailing); });
        for (const auto mask: {std::ios::goodbit, std::ios::badbit}) {
            AllocationFailureBuffer allocation_buffer;
            std::istream allocation(&allocation_buffer);
            allocation.exceptions(mask);
            try {
                (void) rj::fromJson<int>(allocation);
                throw std::runtime_error("Expected bad_alloc");
            } catch (const std::bad_alloc &) { ++checks; }
        }
        rejectsValue<ThrowingConstructor>("{} trailing");
        require(ThrowingConstructor::calls == 0, "Full syntax validation precedes construction");
        try {
            (void) readDocument<ThrowingConstructor>(R"({"value":1})");
            throw std::runtime_error("Expected user constructor exception");
        } catch (const ConstructorError &) { ++checks; }
        require(ThrowingConstructor::calls == 1, "User constructor exception preserved");
    }
}

int main() {
    try {
        checkMutableMetadata();
        checkObjects();
        checkContainers();
        checkNumbers();
        checkStrings();
        checkSyntax();
        checkDepth();
        checkStreamsAndExceptions();
        std::cout << "Reader checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Reader failure after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
