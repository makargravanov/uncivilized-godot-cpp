shader_type spatial;

// Land shader — applied to RELIEF_LAND and RELIEF_HILL MultiMesh groups.
// INSTANCE_CUSTOM.x = biome_id, .y = river_edges, .z = features.
// Future: noise-based colour variation, feature overlays (forest, swamp).

varying flat float biome_id;
varying flat float river_edges;
varying flat float features;

void vertex() {
	biome_id    = INSTANCE_CUSTOM.x;
	river_edges = INSTANCE_CUSTOM.y;
	features    = INSTANCE_CUSTOM.z;
}

void fragment() {
	int biome = clamp(int(biome_id + 0.5), 0, 7);

	vec3 color;

	if (biome == 4)      { color = vec3(0.537, 0.628, 0.210); } // GRASSLAND
	else if (biome == 5) { color = vec3(0.250, 0.336, 0.153); } // HIGHLAND
	else                 { color = vec3(0.537, 0.628, 0.210); } // fallback — grassland

	ALBEDO    = color;
	ROUGHNESS = 0.85;
	METALLIC  = 0.0;
}
