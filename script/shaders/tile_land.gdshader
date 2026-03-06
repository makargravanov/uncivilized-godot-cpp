shader_type spatial;

// Land shader — applied to RELIEF_LAND and RELIEF_HILL MultiMesh groups.
// INSTANCE_CUSTOM.x = biome_id, .y = river_edges, .z = features, .w = overlay_value.
// Procedural noise-based colour variation for natural terrain look.

uniform int view_mode : hint_range(0, 4) = 0;

varying flat float biome_id;
varying flat float river_edges;
varying flat float features;
varying flat float overlay_value;
varying vec3 world_pos;

void vertex() {
	biome_id      = INSTANCE_CUSTOM.x;
	river_edges   = INSTANCE_CUSTOM.y;
	features      = INSTANCE_CUSTOM.z;
	overlay_value = INSTANCE_CUSTOM.w;
	world_pos     = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

// ── Overlay heatmap ramp ─────────────────────────────────────────
// Shared colour ramp: value 0→1 maps to a gradient.
// mode 1 (temperature): blue → cyan → green → yellow → red
// mode 2 (moisture):    brown → yellow → green → cyan → blue
// mode 3 (elevation):   dark green → green → yellow → white
// mode 4 (biome):       distinct hue per biome_id

vec3 heatmap_ramp(float t, int mode) {
	t = clamp(t, 0.0, 1.0);
	if (mode == 1) {
		// Temperature: blue → red
		vec3 c0 = vec3(0.1, 0.2, 0.7);   // cold
		vec3 c1 = vec3(0.0, 0.8, 0.8);   // cool
		vec3 c2 = vec3(0.2, 0.8, 0.2);   // mild
		vec3 c3 = vec3(0.9, 0.9, 0.1);   // warm
		vec3 c4 = vec3(0.8, 0.1, 0.1);   // hot
		if (t < 0.25) return mix(c0, c1, t / 0.25);
		if (t < 0.50) return mix(c1, c2, (t - 0.25) / 0.25);
		if (t < 0.75) return mix(c2, c3, (t - 0.50) / 0.25);
		return mix(c3, c4, (t - 0.75) / 0.25);
	}
	if (mode == 2) {
		// Moisture: brown → blue
		vec3 c0 = vec3(0.6, 0.4, 0.2);   // arid
		vec3 c1 = vec3(0.8, 0.8, 0.2);   // semi-arid
		vec3 c2 = vec3(0.2, 0.7, 0.3);   // moderate
		vec3 c3 = vec3(0.1, 0.5, 0.8);   // humid
		vec3 c4 = vec3(0.1, 0.2, 0.7);   // wet
		if (t < 0.25) return mix(c0, c1, t / 0.25);
		if (t < 0.50) return mix(c1, c2, (t - 0.25) / 0.25);
		if (t < 0.75) return mix(c2, c3, (t - 0.50) / 0.25);
		return mix(c3, c4, (t - 0.75) / 0.25);
	}
	if (mode == 3) {
		// Elevation: dark green → white
		vec3 c0 = vec3(0.1, 0.3, 0.1);
		vec3 c1 = vec3(0.3, 0.6, 0.2);
		vec3 c2 = vec3(0.8, 0.8, 0.3);
		vec3 c3 = vec3(1.0, 1.0, 1.0);
		if (t < 0.33) return mix(c0, c1, t / 0.33);
		if (t < 0.66) return mix(c1, c2, (t - 0.33) / 0.33);
		return mix(c2, c3, (t - 0.66) / 0.34);
	}
	// mode 4 — biome: distinct hue from biome_id
	float hue = fract(t * 0.618 + 0.1);
	vec3 c = clamp(abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
	return mix(vec3(0.3), c, 0.7);
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

// Fractal Brownian Motion — manually unrolled for GPU performance.
// fbm3: 3 octaves (large-scale patches)
float fbm3(vec2 p) {
	return value_noise(p) * 0.5
	     + value_noise(p * 2.0) * 0.25
	     + value_noise(p * 4.0) * 0.125;
}

// fbm2: 2 octaves (medium detail)
float fbm2(vec2 p) {
	return value_noise(p) * 0.5
	     + value_noise(p * 2.0) * 0.25;
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
	// ── Overlay mode: early exit with heatmap colour ──
	if (view_mode > 0) {
		ALBEDO = heatmap_ramp(overlay_value, view_mode);
		ROUGHNESS = 0.9;
		METALLIC = 0.0;
	} else {
		int biome = clamp(int(biome_id + 0.5), 0, 7);

		vec3 base_col = get_biome_base_color(biome);
		vec3 alt_col  = get_biome_alt_color(biome);

		// World-space noise coordinates — seamless across tiles.
		vec2 noise_uv = world_pos.xz;

		// Large-scale colour variation (patches of darker/lighter terrain).
		float large_noise = fbm3(noise_uv * 0.15);

		// Medium-scale detail.
		float med_noise = fbm2(noise_uv * 0.6);

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
}
