#include "example_class.h"
#include "../outer-libs/mindwerks-plate-tectonics/platecapi.hpp"

void ExampleClass::_bind_methods() {
	ClassDB::bind_method(D_METHOD("print_type", "variant"),
	    &ExampleClass::print_type);
}

void ExampleClass::print_type(const Variant &p_variant) const {
	print_line(vformat("Type: %d", p_variant.get_type()));
}
