#include "example_class.h"
#include "../outer-libs/mindwerks-plate-tectonics/platecapi.hpp"

void ExampleClass::_bind_methods() {
	godot::ClassDB::bind_method(D_METHOD("print_type", "variant"), &ExampleClass::print_type);
}

void ExampleClass::print_type(const Variant &p_variant) const {
    platec_api_get_agemap(1);
	print_line(vformat("Type: %d", p_variant.get_type()));
}
