shader_type spatial;

// Ocean shader — applied to RELIEF_OCEAN MultiMesh group.
// INSTANCE_CUSTOM.x = biome_id (0–3 for ocean biomes).
// Future: translucency, depth-based colour, animated waves via vertex offset.

varying flat float biome_id;

void vertex() {
	biome_id = INSTANCE_CUSTOM.x;
}

void fragment() {
	int biome = clamp(int(biome_id + 0.5), 0, 7);

	vec3 deep    = vec3(0.110, 0.157, 0.259);  // DEEP_OCEAN / INLAND_SEA
	vec3 shallow = vec3(0.117, 0.439, 0.497);  // SHALLOW_OCEAN / SHALLOW_INLAND_SEA

	if (biome == 1 || biome == 3) {
		ALBEDO = shallow;
	} else {
		ALBEDO = deep;
	}

	METALLIC  = 0.15;
	ROUGHNESS = 0.35;
	SPECULAR  = 0.5;
}
