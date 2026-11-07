#pragma once

#include <limits>
#include <type_traits>

static_assert(!std::is_copy_constructible_v<rj::JsonWriter>);
static_assert(!std::is_copy_assignable_v<rj::JsonWriter>);
static_assert(!std::is_move_constructible_v<rj::JsonWriter>);
static_assert(!std::is_move_assignable_v<rj::JsonWriter>);

void verify_wide_integers() {
    require(rj::toJson(std::numeric_limits<std::int64_t>::min(), 0) == "-9223372036854775808", "int64 min");
    require(rj::toJson(std::numeric_limits<std::uint64_t>::max(), 0) == "18446744073709551615", "uint64 max");
#ifdef __SIZEOF_INT128__
    using Signed = __int128;
    using Unsigned = unsigned __int128;
    require(rj::toJson(static_cast<Signed>(std::numeric_limits<std::int64_t>::min()), 0)
            == "-9223372036854775808", "Wide target preserves int64 minimum");
    require(rj::toJson(static_cast<Signed>(std::numeric_limits<std::uint64_t>::max()), 0)
            == "18446744073709551615", "Wide signed target preserves uint64 maximum");
    require(rj::toJson(static_cast<Unsigned>(std::numeric_limits<std::uint64_t>::max()), 0)
            == "18446744073709551615", "Wide unsigned target preserves uint64 maximum");
    for (const Signed invalid: {static_cast<Signed>(std::numeric_limits<std::int64_t>::min()) - 1,
                               static_cast<Signed>(std::numeric_limits<std::uint64_t>::max()) + 1,
                               std::numeric_limits<Signed>::max(), std::numeric_limits<Signed>::min()}) {
        require_throws<std::domain_error>([&] { rj::toJson(invalid, 0); });
    }
    require_throws<std::domain_error>([&] { rj::toJson(std::numeric_limits<Unsigned>::max(), 0); });
#endif
}
