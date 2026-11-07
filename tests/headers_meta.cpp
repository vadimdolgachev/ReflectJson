#include <rj/MetaClass.h>
struct HeaderProbe { int value; };
static_assert(meta::MetaClass<HeaderProbe>::fieldCount == 1);
static_assert(meta::MetaClass<HeaderProbe>::properties[0].name == "value");
