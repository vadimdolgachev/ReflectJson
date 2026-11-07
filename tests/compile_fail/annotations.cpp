#include <rj/JsonWriter.h>

#if defined(ReflectJson_CASE_combo)
struct Value { [[= rj::ignore{}, = rj::rename{"x"}]] int field; };
#elif defined(ReflectJson_CASE_combo_reverse)
struct Value { [[= rj::rename{"x"}]] [[= rj::ignore{}]] int field; };
#elif defined(ReflectJson_CASE_combo_private)
class Value { [[= rj::ignore{}, = rj::rename{"x"}]] int field; };
#elif defined(ReflectJson_CASE_repeated_rename)
struct Value { [[= rj::rename{"x"}, = rj::rename{"y"}]] int field; };
#elif defined(ReflectJson_CASE_repeated_identical)
struct Value { [[= rj::rename{"x"}, = rj::rename{"x"}]] int field; };
#elif defined(ReflectJson_CASE_collision)
struct Value { [[= rj::rename{"other"}]] int field; int other; };
#elif defined(ReflectJson_CASE_base_collision)
struct Base { [[= rj::rename{"key"}]] int field; };
struct Value : Base { [[= rj::rename{"key"}]] int other; };
#elif defined(ReflectJson_CASE_raw_private)
class Value { [[= rj::ignore{}]] int field; };
#elif defined(ReflectJson_CASE_private_active)
class Value { [[= rj::rename{"key"}]] int field; };
#elif defined(ReflectJson_CASE_private_base)
struct Base { [[= rj::ignore{}]] int field; };
struct Value : private Base {};
#else
#error Select an annotation rejection case
#endif

int main() {
#if defined(ReflectJson_CASE_raw_private)
    (void) meta::MetaClass<Value>::count;
#else
    (void) rj::toJson(Value{});
#endif
}
