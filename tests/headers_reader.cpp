#include <rj/JsonReader.h>
static_assert(std::is_base_of_v<std::runtime_error, rj::JsonError>);
template<typename T>
concept HasOffset = requires(const T &error) { error.offset(); };
static_assert(!HasOffset<rj::JsonError>);
static_assert(std::is_constructible_v<rj::JsonError, std::string>);
static_assert(!std::is_constructible_v<rj::JsonError, size_t, std::string>);
