#pragma once

#include <array>
#include <optional>
#include <type_traits>
#include <vector>

namespace rj {
    template<typename T>
    inline constexpr bool is_json_optional = false;

    template<typename T>
    inline constexpr bool is_json_optional<std::optional<T> > = true;

    template<typename T>
    inline constexpr bool is_json_array = std::is_array_v<T>;

    template<typename T, size_t N>
    inline constexpr bool is_json_array<std::array<T, N> > = true;

    template<typename T, typename Allocator>
    inline constexpr bool is_json_array<std::vector<T, Allocator> > = true;

    namespace detail {
        template<typename T>
        inline constexpr bool is_vector = false;

        template<typename T, typename Allocator>
        inline constexpr bool is_vector<std::vector<T, Allocator> > = true;
    }
}
