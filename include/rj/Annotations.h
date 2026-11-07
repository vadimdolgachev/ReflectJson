#pragma once

#include <meta>
#include <string_view>

namespace rj {
    struct ignore final {
    };

    struct rename final {
        const char *data;
        size_t size;

        template<size_t N>
        explicit consteval rename(const char (&text)[N])
            : data(std::define_static_string(std::string_view(text, N - 1))),
              size(N - 1) {
            if (text[N - 1] != '\0') {
                throw "rj::rename requires a null-terminated character array";
            }
        }
    };
}
