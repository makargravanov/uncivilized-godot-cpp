shader_type spatial;

// Land shader — applied to RELIEF_LAND and RELIEF_HILL MultiMesh groups.
// INSTANCE_CUSTOM.x = biome_id, .y = river_edges, .z = features.
// Procedural noise-based colour variation for natural terrain look.

varying flat float biome_id;
varying flat float river_edges;
varying flat float features;
varying vec3 world_pos;

void vertex() {
	biome_id    = INSTANCE_CUSTOM.x;
	river_edges = INSTANCE_CUSTOM.y;
	features    = INSTANCE_CUSTOM.z;
	world_pos   = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

// ── Noise utilities ──────────────────────────────────────────────
// Simple hash-based value noise — no textures needed.

vec2 hash22(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}

float value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);  // smoothstep

	float a = dot(hash22(i + vec2(0.0, 0.0)), vec2(1.0));
	float b = dot(hash22(i + vec2(1.0, 0.0)), vec2(1.0));
	float c = dot(hash22(i + vec2(0.0, 1.0)), vec2(1.0));
	float d = dot(hash22(i + vec2(1.0, 1.0)), vec2(1.0));

	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// Fractal Brownian Motion — multiple octaves of value noise.
float fbm(vec2 p, int octaves) {
	float value = 0.0;
	float amplitude = 0.5;
	float frequency = 1.0;
	for (int i = 0; i < octaves; i++) {
		value += amplitude * value_noise(p * frequency);
		frequency *= 2.0;
		amplitude *= 0.5;
	}
	return value;
}

// ── Biome colour palette ─────────────────────────────────────────

vec3 get_biome_base_color(int biome) {
	if (biome == 4) { return vec3(0.537, 0.628, 0.210); } // GRASSLAND
	if (biome == 5) { return vec3(0.250, 0.336, 0.153); } // HIGHLAND
	return vec3(0.537, 0.628, 0.210);                      // fallback
}

// Secondary colour for noise blending — slightly shifted hue.
vec3 get_biome_alt_color(int biome) {
	if (biome == 4) { return vec3(0.420, 0.550, 0.180); } // GRASSLAND — darker olive
	if (biome == 5) { return vec3(0.200, 0.280, 0.120); } // HIGHLAND — darker forest
	return vec3(0.420, 0.550, 0.180);
}

// ── Fragment ─────────────────────────────────────────────────────

void fragment() {
	int biome = clamp(int(biome_id + 0.5), 0, 7);

	vec3 base_col = get_biome_base_color(biome);
	vec3 alt_col  = get_biome_alt_color(biome);

	// World-space noise coordinates — seamless across tiles.
	vec2 noise_uv = world_pos.xz;

	// Large-scale colour variation (patches of darker/lighter terrain).
	float large_noise = fbm(noise_uv * 0.15, 3);

	// Medium-scale detail.
	float med_noise = fbm(noise_uv * 0.6, 2);

	// Fine-grain detail (grass-like texture).
	float fine_noise = value_noise(noise_uv * 3.0);

	// Blend: large patches drive base↔alt mix, medium+fine add brightness variation.
	vec3 color = mix(base_col, alt_col, smoothstep(0.3, 0.7, large_noise));
	color += (med_noise - 0.5) * 0.06;   // subtle medium detail
	color += (fine_noise - 0.5) * 0.03;   // fine grain

	// Slight roughness variation from noise.
	float roughness = 0.85 + (med_noise - 0.5) * 0.1;

	ALBEDO    = clamp(color, vec3(0.0), vec3(1.0));
	ROUGHNESS = roughness;
	METALLIC  = 0.0;
}
