shader_type spatial;

// Biome colour lookup — indices match BiomeType enum (BiomeType.h).
// INSTANCE_CUSTOM.x carries the biome id set from C++ via MultiMesh custom data.

void fragment() {
	int biome = clamp(int(INSTANCE_CUSTOM.x + 0.5), 0, 7);

	if (biome == 0)      { ALBEDO = vec3(0.110, 0.157, 0.259); } // DEEP_OCEAN
	else if (biome == 1) { ALBEDO = vec3(0.117, 0.439, 0.497); } // SHALLOW_OCEAN
	else if (biome == 2) { ALBEDO = vec3(0.110, 0.157, 0.259); } // INLAND_SEA
	else if (biome == 3) { ALBEDO = vec3(0.117, 0.439, 0.497); } // SHALLOW_INLAND_SEA
	else if (biome == 4) { ALBEDO = vec3(0.537, 0.628, 0.210); } // GRASSLAND
	else if (biome == 5) { ALBEDO = vec3(0.250, 0.336, 0.153); } // HIGHLAND
	else if (biome == 6) { ALBEDO = vec3(1.000, 1.000, 1.000); } // ALPINE
	else                 { ALBEDO = vec3(1.000, 0.000, 1.000); } // ERROR — magenta
}
