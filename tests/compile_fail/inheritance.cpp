#include <rj/JsonReader.h>
#include <sstream>

#if defined(ReflectJson_CASE_protected_base)
struct Base {};
struct Value : protected Base {};
#elif defined(ReflectJson_CASE_ancestor_private_base)
struct Base {};
struct Middle : private Base {};
struct Value : Middle {};
#elif defined(ReflectJson_CASE_base_private_field)
class Base { int hidden; };
struct Value : Base { int own; };
#elif defined(ReflectJson_CASE_base_protected_field)
class Base { protected: int hidden; };
struct Value : Base { int own; };
#elif defined(ReflectJson_CASE_hidden_field)
struct Base { int id; };
struct Value : Base { int id; };
#elif defined(ReflectJson_CASE_sibling_name)
struct Left { int id; };
struct Right { int id; };
struct Value : Left, Right {};
#elif defined(ReflectJson_CASE_diamond)
struct Base { int id; };
struct Left : Base {};
struct Right : Base {};
struct Value : Left, Right {};
#elif defined(ReflectJson_CASE_mixed_subobjects)
struct Base { int id; };
struct Left : virtual Base {};
struct Right : Base {};
struct Value : Left, Right {};
#elif defined(ReflectJson_CASE_private_virtual_path)
struct Base { int id; };
struct Left : virtual Base {};
struct Right : private virtual Base {};
struct Value : Left, Right {};
#elif defined(ReflectJson_CASE_read_const)
struct Base { const int id = 1; };
struct Value : Base {};
#elif defined(ReflectJson_CASE_read_ref)
inline int referenced = 1;
struct Base { int &id = referenced; };
struct Value : Base {};
#elif defined(ReflectJson_CASE_read_bit)
struct Base { unsigned id : 1; };
struct Value : Base {};
#else
#error Select an inheritance rejection case
#endif

int main() {
#if defined(ReflectJson_CASE_read_const) || defined(ReflectJson_CASE_read_ref) || defined(ReflectJson_CASE_read_bit)
    std::istringstream input(R"({"id":1})");
    (void) rj::fromJson<Value>(input);
#else
    (void) meta::MetaClass<Value>::count;
#endif
}
