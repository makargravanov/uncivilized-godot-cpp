shader_type spatial;

// Mountain shader — applied to RELIEF_MOUNTAIN and RELIEF_HIGH_MOUNTAIN groups.
// INSTANCE_CUSTOM.x = biome_id.
// Future: snow by normal steepness, rock/grass by slope angle.

varying flat float biome_id;

void vertex() {
	biome_id = INSTANCE_CUSTOM.x;
}

void fragment() {
	int biome = clamp(int(biome_id + 0.5), 0, 7);

	vec3 color;

	if (biome == 6) {
		// ALPINE — snow-capped
		color = vec3(1.000, 1.000, 1.000);
	} else if (biome == 5) {
		// HIGHLAND — darker green/grey
		color = vec3(0.250, 0.336, 0.153);
	} else {
		// Generic mountain rock
		color = vec3(0.45, 0.42, 0.38);
	}

	ALBEDO    = color;
	ROUGHNESS = 0.9;
	METALLIC  = 0.0;
}
