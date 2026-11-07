#include <rj/JsonReader.h>
#include <rj/detail/JsonLimits.h>

#include <charconv>
#include <cmath>
#include <new>
#include <streambuf>
#include <unordered_set>

namespace rj {
    namespace {
        std::string escapeDiagnostic(const std::string_view message) {
            constexpr std::string_view hex = "0123456789abcdef";
            std::string escaped;
            for (const unsigned char ch: message) {
                if (ch == '\\') {
                    escaped += "\\\\";
                } else if (ch < 0x20) {
                    escaped += "\\u00";
                    escaped += hex[ch >> 4];
                    escaped += hex[ch & 0x0f];
                } else {
                    escaped += static_cast<char>(ch);
                }
            }
            return escaped;
        }
    }

    JsonError::JsonError(const std::string &message) : std::runtime_error(escapeDiagnostic(message)) {
    }
}

namespace rj::detail {
    namespace {
        bool isDigit(const char ch) {
            return ch >= '0' && ch <= '9';
        }

        bool isUnderflow(const std::string_view text) {
            using Wide = __int128;
            constexpr Wide exponentLimit = static_cast<Wide>(1) << 120;
            const auto exponentPosition = text.find_first_of("eE");
            const auto mantissa = text.substr(0, exponentPosition);
            Wide exponent = 0;
            if (exponentPosition != std::string_view::npos) {
                size_t position = exponentPosition + 1;
                bool negative = false;
                if (text[position] == '+' || text[position] == '-') {
                    negative = text[position] == '-';
                    ++position;
                }
                for (; position < text.size(); ++position) {
                    if (exponent < exponentLimit) {
                        exponent = exponent * 10 + (text[position] - '0');
                        if (exponent > exponentLimit) {
                            exponent = exponentLimit;
                        }
                    }
                }
                exponent = negative ? -exponent : exponent;
            }

            size_t decimalPosition = std::string_view::npos;
            size_t firstNonzero = std::string_view::npos;
            size_t digitIndex = 0;
            for (const char ch: mantissa) {
                if (ch == '-') {
                    continue;
                }
                if (ch == '.') {
                    decimalPosition = digitIndex;
                    continue;
                }
                if (ch != '0' && firstNonzero == std::string_view::npos) {
                    firstNonzero = digitIndex;
                }
                ++digitIndex;
            }
            if (firstNonzero == std::string_view::npos) {
                return true;
            }
            if (decimalPosition == std::string_view::npos) {
                decimalPosition = digitIndex;
            }
            const Wide adjustedExponent = exponent + static_cast<Wide>(decimalPosition)
                                          - static_cast<Wide>(firstNonzero) - 1;
            return adjustedExponent <= -324;
        }

        class Parser final {
        public:
            explicit Parser(const std::string_view input) : input(input) {
            }

            JsonValue parse() {
                auto value = parseValue(0);
                skipWhitespace();
                if (position != input.size()) {
                    fail("Unexpected trailing data");
                }
                return value;
            }

        private:
            const std::string_view input;
            size_t position = 0;

            [[noreturn]] static void fail(const std::string &message) {
                throw JsonError(message);
            }

            void skipWhitespace() {
                while (position < input.size()) {
                    if (const char ch = input[position];
                        ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
                        break;
                    }
                    ++position;
                }
            }

            bool consume(const char expected) {
                if (position < input.size() && input[position] == expected) {
                    ++position;
                    return true;
                }
                return false;
            }

            void expect(const char expected) {
                if (!consume(expected)) {
                    fail(std::string("Expected '") + expected + "'");
                }
            }

            void literal(const std::string_view text) {
                for (const char ch: text) {
                    expect(ch);
                }
            }

            unsigned readHexQuad() {
                unsigned value = 0;
                for (int i = 0; i < 4; ++i) {
                    if (position == input.size()) {
                        fail("Incomplete Unicode escape");
                    }
                    const char ch = input[position];
                    unsigned digit;
                    if (ch >= '0' && ch <= '9') {
                        digit = static_cast<unsigned>(ch - '0');
                    } else if (ch >= 'a' && ch <= 'f') {
                        digit = static_cast<unsigned>(ch - 'a') + 10;
                    } else if (ch >= 'A' && ch <= 'F') {
                        digit = static_cast<unsigned>(ch - 'A') + 10;
                    } else {
                        fail("Invalid Unicode escape");
                    }
                    value = (value << 4) + digit;
                    ++position;
                }
                return value;
            }

            static void appendUtf8(std::string &text, const unsigned codepoint) {
                if (codepoint <= 0x7f) {
                    text.push_back(static_cast<char>(codepoint));
                } else if (codepoint <= 0x7ff) {
                    text.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
                    text.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
                } else if (codepoint <= 0xffff) {
                    text.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
                    text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                    text.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
                } else {
                    text.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
                    text.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
                    text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
                    text.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
                }
            }

            void readUtf8(std::string &text) {
                const auto start = position;
                const auto first = static_cast<unsigned char>(input[position]);
                size_t length;
                unsigned codepoint;
                unsigned minimum;
                if (first >= 0xc2 && first <= 0xdf) {
                    length = 2;
                    codepoint = first & 0x1f;
                    minimum = 0x80;
                } else if (first >= 0xe0 && first <= 0xef) {
                    length = 3;
                    codepoint = first & 0x0f;
                    minimum = 0x800;
                } else if (first >= 0xf0 && first <= 0xf4) {
                    length = 4;
                    codepoint = first & 0x07;
                    minimum = 0x10000;
                } else {
                    fail("Invalid UTF-8 leading byte");
                }
                if (input.size() - position < length) {
                    fail("Incomplete UTF-8 sequence");
                }
                for (size_t i = 1; i < length; ++i) {
                    const auto ch = static_cast<unsigned char>(input[position + i]);
                    if ((ch & 0xc0) != 0x80) {
                        fail("Invalid UTF-8 continuation byte");
                    }
                    codepoint = (codepoint << 6) | (ch & 0x3f);
                }
                if (codepoint < minimum || codepoint > 0x10ffff
                    || (codepoint >= 0xd800 && codepoint <= 0xdfff)) {
                    fail("Invalid UTF-8 code point");
                }
                text.append(input.substr(start, length));
                position += length;
            }

            std::string parseString() {
                expect('"');
                std::string text;
                while (position < input.size()) {
                    const auto ch = static_cast<unsigned char>(input[position]);
                    if (ch == '"') {
                        ++position;
                        return text;
                    }
                    if (ch < 0x20) {
                        fail("Unescaped control character in string");
                    }
                    if (ch >= 0x80) {
                        readUtf8(text);
                        continue;
                    }
                    ++position;
                    if (ch != '\\') {
                        text.push_back(static_cast<char>(ch));
                        continue;
                    }
                    if (position == input.size()) {
                        fail("Incomplete string escape");
                    }
                    switch (input[position++]) {
                        case '"': text.push_back('"');
                            break;
                        case '\\': text.push_back('\\');
                            break;
                        case '/': text.push_back('/');
                            break;
                        case 'b': text.push_back('\b');
                            break;
                        case 'f': text.push_back('\f');
                            break;
                        case 'n': text.push_back('\n');
                            break;
                        case 'r': text.push_back('\r');
                            break;
                        case 't': text.push_back('\t');
                            break;
                        case 'u': {
                            unsigned codepoint = readHexQuad();
                            if (codepoint >= 0xd800 && codepoint <= 0xdbff) {
                                expect('\\');
                                expect('u');
                                const unsigned low = readHexQuad();
                                if (low < 0xdc00 || low > 0xdfff) {
                                    fail("Invalid Unicode surrogate pair");
                                }
                                codepoint = 0x10000 + ((codepoint - 0xd800) << 10) + low - 0xdc00;
                            } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
                                fail("Unpaired Unicode low surrogate");
                            }
                            appendUtf8(text, codepoint);
                            break;
                        }
                        default: fail("Invalid string escape");
                    }
                }
                fail("Unterminated string");
            }

            std::string_view scanNumber(bool &floating) {
                const auto start = position;
                consume('-');
                if (!consume('0')) {
                    if (position == input.size() || input[position] < '1' || input[position] > '9') {
                        fail("Expected a digit");
                    }
                    while (position < input.size() && isDigit(input[position])) {
                        ++position;
                    }
                } else if (position < input.size() && isDigit(input[position])) {
                    fail("Leading zero in number");
                }
                floating = false;
                if (consume('.')) {
                    floating = true;
                    if (position == input.size() || !isDigit(input[position])) {
                        fail("Expected a digit after decimal point");
                    }
                    while (position < input.size() && isDigit(input[position])) {
                        ++position;
                    }
                }
                if (consume('e') || consume('E')) {
                    floating = true;
                    if (!consume('+')) {
                        consume('-');
                    }
                    if (position == input.size() || !isDigit(input[position])) {
                        fail("Expected a digit in exponent");
                    }
                    while (position < input.size() && isDigit(input[position])) {
                        ++position;
                    }
                }
                return input.substr(start, position - start);
            }

            JsonValue parseNumber() {
                bool floating;
                const auto text = scanNumber(floating);
                const char *const begin = text.data();
                const char *const end = begin + text.size();
                JsonValue value;
                if (floating) {
                    double number = 0;
                    const auto [ptr, error] = std::from_chars(begin, end, number);
                    if (error == std::errc::result_out_of_range && isUnderflow(text)) {
                        value.kind = JsonKind::Double;
                        value.floatingNumber = text.front() == '-' ? -0.0 : 0.0;
                        return value;
                    }
                    if (error != std::errc{} || ptr != end || !std::isfinite(number)) {
                        fail("Floating-point number out of range");
                    }
                    value.kind = JsonKind::Double;
                    value.floatingNumber = number;
                    return value;
                }
                if (text.front() == '-') {
                    int64_t number = 0;
                    if (const auto [ptr, error] = std::from_chars(begin, end, number);
                        error != std::errc{} || ptr != end) {
                        fail("Integer out of range");
                    }
                    value.kind = JsonKind::Int64;
                    value.signedNumber = number;
                    return value;
                }
                uint64_t number = 0;
                if (const auto [ptr, error] = std::from_chars(begin, end, number);
                    error != std::errc{} || ptr != end) {
                    fail("Integer out of range");
                }
                if (number <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                    value.kind = JsonKind::Int64;
                    value.signedNumber = static_cast<int64_t>(number);
                } else {
                    value.kind = JsonKind::UInt64;
                    value.unsignedNumber = number;
                }
                return value;
            }

            JsonValue parseValue(const size_t depth) {
                skipWhitespace();
                if (position == input.size()) {
                    fail("Expected a JSON value");
                }
                JsonValue value;
                const char ch = input[position];
                if (ch == '{' || ch == '[') {
                    if (depth == MaxDepth) {
                        fail("JSON nesting exceeds 256 containers");
                    }
                    ++position;
                    skipWhitespace();
                    if (ch == '{') {
                        value.kind = JsonKind::Object;
                        std::unordered_set<std::string> names;
                        if (consume('}')) {
                            return value;
                        }
                        do {
                            skipWhitespace();
                            auto name = parseString();
                            if (!names.insert(name).second) {
                                throw JsonError("Duplicate object key: " + name);
                            }
                            skipWhitespace();
                            expect(':');
                            auto member = parseValue(depth + 1);
                            value.members.push_back({std::move(name), std::move(member)});
                            skipWhitespace();
                            if (consume('}')) {
                                return value;
                            }
                            expect(',');
                        } while (true);
                    }
                    value.kind = JsonKind::Array;
                    if (consume(']')) {
                        return value;
                    }
                    do {
                        value.elements.push_back(parseValue(depth + 1));
                        skipWhitespace();
                        if (consume(']')) {
                            return value;
                        }
                        expect(',');
                    } while (true);
                }
                if (ch == '"') {
                    value.kind = JsonKind::String;
                    value.text = parseString();
                } else if (ch == 't' || ch == 'f') {
                    value.kind = JsonKind::Boolean;
                    value.boolean = ch == 't';
                    literal(value.boolean ? "true" : "false");
                } else if (ch == 'n') {
                    value.kind = JsonKind::Null;
                    literal("null");
                } else if (ch == '-' || isDigit(ch)) {
                    return parseNumber();
                } else {
                    fail("Unexpected character; expected a JSON value");
                }
                return value;
            }
        };

        std::string readJsonStream(std::istream &input) {
            std::string text;
            if (input.bad() || (input.fail() && !input.eof())) {
                throw JsonError("Input stream is not readable");
            }
            if (input.eof()) {
                return text;
            }
            char buffer[4096];
            while (true) {
                std::streamsize count;
                try {
                    count = input.rdbuf()->sgetn(buffer, sizeof(buffer));
                } catch (const std::bad_alloc &) {
                    throw;
                } catch (...) {
                    try {
                        input.setstate(std::ios::badbit);
                    } catch (const std::ios_base::failure &) {
                    }
                    throw JsonError("Failed to read JSON stream");
                }
                if (count < 0 || count > static_cast<std::streamsize>(sizeof(buffer))) {
                    throw JsonError("Failed to read JSON stream");
                }
                text.append(buffer, static_cast<size_t>(count));
                if (count < static_cast<std::streamsize>(sizeof(buffer))) {
                    try {
                        input.setstate(std::ios::eofbit | std::ios::failbit);
                    } catch (const std::ios_base::failure &) {
                    }
                    return text;
                }
            }
        }
    }

    JsonValue readDocument(std::istream &input) {
        const auto text = readJsonStream(input);
        if (text.starts_with("\xef\xbb\xbf")) {
            throw JsonError("Leading BOM is not supported");
        }
        return Parser(text).parse();
    }

    float checkedFloat(const double number) {
        char buffer[64];
        const auto [end, formatError] = std::to_chars(buffer, buffer + sizeof(buffer), number);
        float result = 0;
        if (formatError != std::errc{}) {
            throw JsonError("Could not format DOM number");
        }
        if (const auto [ptr, error] = std::from_chars(buffer, end, result);
            error != std::errc{} || ptr != end || !std::isfinite(result)) {
            throw JsonError("Floating-point number out of range for target type");
        }
        return result;
    }
}
