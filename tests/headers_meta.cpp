#include <rj/MetaClass.h>
struct HeaderProbe { int value; };
static_assert(meta::MetaClass<HeaderProbe>::fieldCount == 1);
static_assert(meta::MetaClass<HeaderProbe>::fields[0].property().name == "value");

enum class HeaderEnum { First, Second, Alias = First };
enum class HeaderEmptyEnum {};
using HeaderMetaEnum = meta::MetaEnum<const HeaderEnum &>;
static_assert(std::is_same_v<decltype(HeaderMetaEnum::fromString("First")), std::optional<HeaderEnum>>);
static_assert(std::is_same_v<decltype(HeaderMetaEnum::toString(HeaderEnum::First)), std::optional<std::string_view>>);
static_assert(noexcept(HeaderMetaEnum::fromString("First")));
static_assert(noexcept(HeaderMetaEnum::toString(HeaderEnum::First)));
static_assert(HeaderMetaEnum::fromString("Alias") == HeaderEnum::First);
static_assert(!HeaderMetaEnum::fromString("Missing"));
static_assert(HeaderMetaEnum::toString(HeaderEnum::Alias) == "First");
static_assert(!HeaderMetaEnum::toString(static_cast<HeaderEnum>(33)));
static_assert(meta::MetaEnum<HeaderEmptyEnum>::properties.empty());
static_assert(!meta::MetaEnum<HeaderEmptyEnum>::fromString(""));
static_assert(!meta::MetaEnum<HeaderEmptyEnum>::toString(HeaderEmptyEnum{}));
