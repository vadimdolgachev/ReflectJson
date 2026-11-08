#include <rj/MetaClass.h>
struct HeaderProbe { int value; };
static_assert(meta::MetaClass<HeaderProbe>::fieldCount == 1);
static_assert(meta::MetaClass<HeaderProbe>::fields[0].property().name == "value");
