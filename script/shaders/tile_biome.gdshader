shader_type spatial;

// Biome colour lookup — indices match BiomeType enum (BiomeType.h).
// INSTANCE_CUSTOM.x carries the biome id set from C++ via MultiMesh custom data.
// INSTANCE_CUSTOM is only available in vertex(), so we forward it via a varying.

varying flat float biome_id;

void vertex() {
	biome_id = INSTANCE_CUSTOM.x;
}

void fragment() {
	int biome = clamp(int(biome_id + 0.5), 0, 12);

	if (biome == 0)      { ALBEDO = vec3(0.110, 0.157, 0.259); } // DEEP_OCEAN
	else if (biome == 1) { ALBEDO = vec3(0.117, 0.439, 0.497); } // SHALLOW_OCEAN
	else if (biome == 2) { ALBEDO = vec3(0.110, 0.157, 0.259); } // INLAND_SEA
	else if (biome == 3) { ALBEDO = vec3(0.117, 0.439, 0.497); } // SHALLOW_INLAND_SEA
	else if (biome == 4) { ALBEDO = vec3(0.537, 0.628, 0.210); } // GRASSLAND
	else if (biome == 5) { ALBEDO = vec3(0.250, 0.336, 0.153); } // HIGHLAND
	else if (biome == 6) { ALBEDO = vec3(1.000, 1.000, 1.000); } // ALPINE
	else if (biome == 7) { ALBEDO = vec3(0.620, 0.660, 0.540); } // TUNDRA
	else if (biome == 8) { ALBEDO = vec3(0.190, 0.290, 0.160); } // BOREAL_FOREST
	else if (biome == 9) { ALBEDO = vec3(0.240, 0.450, 0.180); } // TEMPERATE_FOREST
	else if (biome == 10) { ALBEDO = vec3(0.660, 0.640, 0.280); } // SAVANNA
	else if (biome == 11) { ALBEDO = vec3(0.780, 0.680, 0.440); } // DESERT
	else if (biome == 12) { ALBEDO = vec3(0.100, 0.360, 0.140); } // TROPICAL_FOREST
	else                 { ALBEDO = vec3(1.000, 0.000, 1.000); } // ERROR — magenta
}
