shader_type spatial;

// Biome colour lookup — indices match BiomeType enum (BiomeType.h).
// INSTANCE_CUSTOM.x carries the biome id set from C++ via MultiMesh custom data.
// INSTANCE_CUSTOM is only available in vertex(), so we forward it via a varying.

varying flat float biome_id;

void vertex() {
	biome_id = INSTANCE_CUSTOM.x;
}

void fragment() {
	int biome = clamp(int(biome_id + 0.5), 0, 15);

	if (biome == 0)      { ALBEDO = vec3(0.110, 0.157, 0.259); } // DEEP_OCEAN
	else if (biome == 1) { ALBEDO = vec3(0.117, 0.439, 0.497); } // SHALLOW_OCEAN
	else if (biome == 2) { ALBEDO = vec3(0.110, 0.157, 0.259); } // INLAND_SEA
	else if (biome == 3) { ALBEDO = vec3(0.117, 0.439, 0.497); } // SHALLOW_INLAND_SEA
	else if (biome == 4) { ALBEDO = vec3(0.730, 0.710, 0.660); } // POLAR_DESERT
	else if (biome == 5) { ALBEDO = vec3(0.600, 0.650, 0.540); } // TUNDRA
	else if (biome == 6) { ALBEDO = vec3(0.170, 0.270, 0.150); } // BOREAL_FOREST
	else if (biome == 7) { ALBEDO = vec3(0.660, 0.620, 0.410); } // COLD_STEPPE
	else if (biome == 8) { ALBEDO = vec3(0.240, 0.450, 0.180); } // TEMPERATE_FOREST
	else if (biome == 9) { ALBEDO = vec3(0.720, 0.690, 0.360); } // TEMPERATE_STEPPE
	else if (biome == 10) { ALBEDO = vec3(0.610, 0.530, 0.320); } // XERIC_SHRUBLAND
	else if (biome == 11) { ALBEDO = vec3(0.800, 0.690, 0.430); } // HOT_DESERT
	else if (biome == 12) { ALBEDO = vec3(0.700, 0.650, 0.260); } // SAVANNA
	else if (biome == 13) { ALBEDO = vec3(0.190, 0.380, 0.150); } // TROPICAL_SEASONAL_FOREST
	else if (biome == 14) { ALBEDO = vec3(0.080, 0.310, 0.130); } // TROPICAL_RAINFOREST
	else if (biome == 15) { ALBEDO = vec3(1.000, 1.000, 1.000); } // ALPINE
	else                 { ALBEDO = vec3(1.000, 0.000, 1.000); } // ERROR — magenta
}
