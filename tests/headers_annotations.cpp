#include <rj/Annotations.h>

struct AnnotationHeaderProbe {
    [[= rj::ignore{}]] int skipped;
    [[= rj::rename{"external"}]] int value;
};
static_assert(std::meta::annotations_of_with_type(^^AnnotationHeaderProbe::skipped, ^^rj::ignore).size() == 1);
static_assert(std::meta::extract<rj::rename>(std::meta::annotations_of(^^AnnotationHeaderProbe::value)[0]).size == 8);
