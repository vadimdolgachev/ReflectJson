#pragma once

#include <iomanip>
#include <streambuf>

namespace {
    std::string manual_document(const Person &person, size_t indent, bool formatted) {
        std::ostringstream output;
        if (formatted) {
            output.imbue(std::locale(std::locale::classic(), new CommaDecimal));
            output << std::hex << std::showbase << std::boolalpha << std::scientific
                   << std::setprecision(1) << std::setfill('#') << std::setw(30);
        }
        const auto flags = output.flags();
        const auto precision = output.precision();
        const auto fill = output.fill();
        const auto width = output.width();
        const auto locale = output.getloc();
        const auto exceptions = output.exceptions();
        {
            rj::JsonWriter writer(output, indent);
            require(&writer.os == &output, "Writer changed the stream reference");
            writer.beginObject();
            writer.write("text", std::string("Привет\n\"\\") + std::string(1, '\0'));
            writer.beginArray("items");
            writer.write(1);
            writer.write(true);
            writer.write(nullptr);
            writer.write(std::optional<int>{2});
            writer.beginObject();
            writer.write("person", person);
            writer.beginArray("empty");
            writer.endArray();
            writer.endObject();
            writer.beginArray();
            writer.write(std::vector<bool>{true, false});
            writer.write(std::optional<std::vector<int>>{{3, 4}});
            writer.endArray();
            writer.endArray();
            writer.beginObject("object");
            writer.write("", std::string_view("empty-key"));
            writer.write(std::string("key\"\n\\") + std::string(1, '\0'),
                         std::string_view("value"));
            writer.endObject();
            writer.write("person", std::optional<Person>{person});
            writer.write("none", std::optional<std::string>{});
            writer.endObject();
        }
        require(output.flags() == flags && output.precision() == precision
                && output.fill() == fill && output.width() == width
                && output.getloc() == locale && output.exceptions() == exceptions,
                "Writer modified external stream settings");
        return output.str();
    }

    struct RejectBuffer : std::streambuf {
        int_type overflow(int_type) override { return traits_type::eof(); }
        std::streamsize xsputn(const char *, std::streamsize) override { return 0; }
    };
    struct SyncBuffer : std::stringbuf {
        int sync_calls = 0;
        int sync() override {
            ++sync_calls;
            return std::stringbuf::sync();
        }
    };

    void verify_writer(const Person &person, std::vector<JsonCase> &cases) {
        cases.push_back({"writer_compact", manual_document(person, 0, false)});
        cases.push_back({"writer_pretty", manual_document(person, 2, false)});
        cases.push_back({"writer_four_spaces", manual_document(person, 4, false)});
        cases.push_back({"writer_formatted", manual_document(person, 0, true)});
        require(cases[cases.size() - 1].document == cases[cases.size() - 4].document,
                "External formatting affected JSON");
        for (size_t indent : {0, 2, 4}) {
            std::ostringstream direct;
            rj::JsonWriter writer(direct, indent);
            writer.write(person);
            require(direct.str() == rj::toJson(person, indent),
                    "Direct and string serialization differ");

            std::ostringstream empty_object;
            rj::JsonWriter object_writer(empty_object, indent);
            object_writer.beginObject();
            object_writer.endObject();
            require(empty_object.str() == "{}", "Empty manual object formatting");
            std::ostringstream empty_array;
            rj::JsonWriter array_writer(empty_array, indent);
            array_writer.beginArray();
            array_writer.endArray();
            require(empty_array.str() == "[]", "Empty manual array formatting");
        }
        {
            std::ostringstream output;
            rj::JsonWriter writer(output);
            writer.beginObject();
            writer.write("value", 1);
            writer.endObject();
            require(output.str() == "{\n  \"value\": 1\n}", "Default writer indentation");
        }
        {
            std::ostringstream output;
            rj::JsonWriter writer(output, 0);
            writer.beginArray();
            for (int i = 0; i < 64; ++i) {
                writer.beginObject();
                writer.beginArray("child");
            }
            writer.write(person);
            for (int i = 0; i < 64; ++i) {
                writer.endArray();
                writer.endObject();
            }
            writer.endArray();
            cases.push_back({"writer_deep", output.str()});
        }
        int invalid_calls = 0;
        {
            std::ostringstream output;
            rj::JsonWriter writer(output, 0);
            const auto invalid = [&](auto operation) {
                const auto before = output.str();
                require_throws<std::logic_error>(operation);
                require(output.str() == before, "Invalid call changed output");
                ++invalid_calls;
            };
            invalid([&] { writer.write("bad", 1); });
            invalid([&] { writer.beginObject("bad"); });
            invalid([&] { writer.beginArray("bad"); });
            invalid([&] { writer.endObject(); });
            invalid([&] { writer.endArray(); });
            writer.beginObject();
            invalid([&] { writer.write(1); });
            invalid([&] { writer.beginObject(); });
            invalid([&] { writer.beginArray(); });
            invalid([&] { writer.endArray(); });
            writer.write("ok", 1);
            writer.beginArray("rows");
            invalid([&] { writer.write("bad", 1); });
            invalid([&] { writer.beginObject("bad"); });
            invalid([&] { writer.beginArray("bad"); });
            invalid([&] { writer.endObject(); });
            writer.beginObject();
            writer.write("ok", 2);
            writer.endObject();
            writer.write(3);
            writer.endArray();
            writer.endObject();
            invalid([&] { writer.write(4); });
            invalid([&] { writer.write("bad", 1); });
            invalid([&] { writer.beginObject(); });
            invalid([&] { writer.beginArray(); });
            invalid([&] { writer.beginObject("bad"); });
            invalid([&] { writer.beginArray("bad"); });
            invalid([&] { writer.endObject(); });
            invalid([&] { writer.endArray(); });
            cases.push_back({"writer_recovered", output.str()});
        }
        {
            std::ostringstream output;
            rj::JsonWriter writer(output, 0);
            writer.write(std::optional<int>{});
            require_throws<std::logic_error>([&] { writer.write(1); });
            require(output.str() == "null", "Null root was not tracked");
            cases.push_back({"writer_null_root", output.str()});
        }
        {
            SyncBuffer buffer;
            std::ostream output(&buffer);
            {
                rj::JsonWriter writer(output, 0);
                writer.beginObject();
                writer.write("value", 1);
            }
            require(buffer.str() == "{\"value\":1", "Destructor completed the document");
            require(buffer.sync_calls == 0, "Writer flushed its external stream");
            output.put('}');
            require(buffer.str() == "{\"value\":1}", "External stream is not usable");
        }
        {
            RejectBuffer buffer;
            std::ostream output(&buffer);
            rj::JsonWriter writer(output);
            writer.write(42);
            require(output.bad(), "Stream failure was not recorded");
            require(output.exceptions() == std::ios::goodbit, "Stream exception mask changed");
        }
        {
            RejectBuffer buffer;
            std::ostream output(&buffer);
            const auto mask = std::ios::badbit | std::ios::failbit;
            output.exceptions(mask);
            rj::JsonWriter writer(output);
            require_throws<std::ios_base::failure>([&] { writer.beginArray(); });
            require(output.exceptions() == mask, "Throwing stream exception mask changed");
        }
        {
            std::ostringstream output;
            rj::JsonWriter writer(output, 0);
            writer.beginObject();
            require_throws<std::domain_error>([&] {
                writer.write("bad", std::numeric_limits<double>::infinity());
            });
        }
        std::cerr << "Writer manual/automatic nesting, formatting, depth, stream failures/"
                     "lifetime and " << invalid_calls << " invalid context checks passed.\n";
    }
}

