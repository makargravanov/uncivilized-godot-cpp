shader_type spatial;

// Mountain shader — applied to RELIEF_MOUNTAIN and RELIEF_HIGH_MOUNTAIN groups.
// INSTANCE_CUSTOM.x = biome_id, .w = overlay_value.
// Procedural: snow by world-space normal steepness, rock/grass by slope, noise variation.

uniform int view_mode : hint_range(0, 6) = 0;

varying flat float biome_id;
varying flat float overlay_value;
varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	biome_id      = INSTANCE_CUSTOM.x;
	overlay_value = INSTANCE_CUSTOM.w;
	world_pos     = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal  = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
}

// ── Noise utilities ──────────────────────────────────────────────

vec2 hash22(vec2 p) {
	vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.xx + p3.yz) * p3.zy);
}

float value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);

	float a = dot(hash22(i + vec2(0.0, 0.0)), vec2(1.0));
	float b = dot(hash22(i + vec2(1.0, 0.0)), vec2(1.0));
	float c = dot(hash22(i + vec2(0.0, 1.0)), vec2(1.0));
	float d = dot(hash22(i + vec2(1.0, 1.0)), vec2(1.0));

	return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// fbm3: 3 octaves, manually unrolled.
float fbm3(vec2 p) {
	return value_noise(p) * 0.5
	     + value_noise(p * 2.0) * 0.25
	     + value_noise(p * 4.0) * 0.125;
}

// fbm2: 2 octaves.
float fbm2(vec2 p) {
	return value_noise(p) * 0.5
	     + value_noise(p * 2.0) * 0.25;
}

// ── Overlay heatmap ramp ─────────────────────────────────────────

vec3 heatmap_ramp(float t, int mode) {
	t = clamp(t, 0.0, 1.0);
	if (mode == 1) {
		vec3 c0 = vec3(0.1, 0.2, 0.7);
		vec3 c1 = vec3(0.0, 0.8, 0.8);
		vec3 c2 = vec3(0.2, 0.8, 0.2);
		vec3 c3 = vec3(0.9, 0.9, 0.1);
		vec3 c4 = vec3(0.8, 0.1, 0.1);
		if (t < 0.25) return mix(c0, c1, t / 0.25);
		if (t < 0.50) return mix(c1, c2, (t - 0.25) / 0.25);
		if (t < 0.75) return mix(c2, c3, (t - 0.50) / 0.25);
		return mix(c3, c4, (t - 0.75) / 0.25);
	}
	if (mode == 2) {
		vec3 c0 = vec3(0.6, 0.4, 0.2);
		vec3 c1 = vec3(0.8, 0.8, 0.2);
		vec3 c2 = vec3(0.2, 0.7, 0.3);
		vec3 c3 = vec3(0.1, 0.5, 0.8);
		vec3 c4 = vec3(0.1, 0.2, 0.7);
		if (t < 0.25) return mix(c0, c1, t / 0.25);
		if (t < 0.50) return mix(c1, c2, (t - 0.25) / 0.25);
		if (t < 0.75) return mix(c2, c3, (t - 0.50) / 0.25);
		return mix(c3, c4, (t - 0.75) / 0.25);
	}
	if (mode == 3) {
		vec3 c0 = vec3(0.1, 0.3, 0.1);
		vec3 c1 = vec3(0.3, 0.6, 0.2);
		vec3 c2 = vec3(0.8, 0.8, 0.3);
		vec3 c3 = vec3(1.0, 1.0, 1.0);
		if (t < 0.33) return mix(c0, c1, t / 0.33);
		if (t < 0.66) return mix(c1, c2, (t - 0.33) / 0.33);
		return mix(c2, c3, (t - 0.66) / 0.34);
	}
	if (mode == 6) {
		vec3 c0 = vec3(0.10, 0.05, 0.18);
		vec3 c1 = vec3(0.20, 0.14, 0.52);
		vec3 c2 = vec3(0.10, 0.56, 0.84);
		vec3 c3 = vec3(0.34, 0.82, 0.46);
		vec3 c4 = vec3(0.96, 0.92, 0.54);
		if (t < 0.25) return mix(c0, c1, t / 0.25);
		if (t < 0.50) return mix(c1, c2, (t - 0.25) / 0.25);
		if (t < 0.75) return mix(c2, c3, (t - 0.50) / 0.25);
		return mix(c3, c4, (t - 0.75) / 0.25);
	}
	float hue = fract(t * 0.618 + 0.1);
	vec3 c = clamp(abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
	return mix(vec3(0.3), c, 0.7);
}

// ── Material colours ─────────────────────────────────────────────

const vec3 ROCK_COLOR     = vec3(0.45, 0.42, 0.38);
const vec3 ROCK_ALT_COLOR = vec3(0.38, 0.35, 0.30);
const vec3 SNOW_COLOR     = vec3(0.95, 0.96, 0.98);
const vec3 GRASS_COLOR    = vec3(0.30, 0.38, 0.18);

vec3 get_mountain_vegetation_color(int biome) {
	if (biome == 4) { return vec3(0.64, 0.62, 0.58); } // POLAR_DESERT
	if (biome == 5) { return vec3(0.52, 0.58, 0.48); } // TUNDRA
	if (biome == 6) { return vec3(0.14, 0.22, 0.12); } // BOREAL_FOREST
	if (biome == 7) { return vec3(0.56, 0.52, 0.33); } // COLD_STEPPE
	if (biome == 8) { return vec3(0.22, 0.38, 0.16); } // TEMPERATE_FOREST
	if (biome == 9) { return vec3(0.60, 0.56, 0.27); } // TEMPERATE_STEPPE
	if (biome == 10) { return vec3(0.50, 0.44, 0.26); } // XERIC_SHRUBLAND
	if (biome == 11) { return vec3(0.69, 0.58, 0.35); } // HOT_DESERT
	if (biome == 12) { return vec3(0.57, 0.53, 0.22); } // SAVANNA
	if (biome == 13) { return vec3(0.16, 0.31, 0.13); } // TROPICAL_SEASONAL_FOREST
	if (biome == 14) { return vec3(0.08, 0.24, 0.10); } // TROPICAL_RAINFOREST
	return GRASS_COLOR;
}

float get_mountain_vegetation_strength(int biome) {
	if (biome == 4 || biome == 11) { return 0.08; }
	if (biome == 10) { return 0.15; }
	if (biome == 7 || biome == 9) { return 0.24; }
	if (biome == 5) { return 0.20; }
	if (biome == 12) { return 0.30; }
	if (biome == 6 || biome == 8 || biome == 13 || biome == 14) { return 0.42; }
	return 0.18;
}

// ── Fragment ─────────────────────────────────────────────────────

void fragment() {
	// ── Overlay mode: early exit with heatmap colour ──
	if (view_mode > 0) {
		ALBEDO = heatmap_ramp(overlay_value, view_mode);
		ROUGHNESS = 0.9;
		METALLIC = 0.0;
	} else {
		int biome = clamp(int(biome_id + 0.5), 0, 15);

		vec2 noise_uv = world_pos.xz;

		// Slope factor: 1.0 = flat top, 0.0 = vertical cliff.
		float slope = dot(world_normal, vec3(0.0, 1.0, 0.0));
		slope = clamp(slope, 0.0, 1.0);

		// Noise layers for natural variation.
		float large_noise = fbm3(noise_uv * 0.15);
		float med_noise   = fbm2(noise_uv * 0.5);
		float fine_noise  = value_noise(noise_uv * 2.5);

		// ── Base rock colour with noise variation ──
		vec3 rock = mix(ROCK_COLOR, ROCK_ALT_COLOR, smoothstep(0.35, 0.65, large_noise));
		rock += (med_noise - 0.5) * 0.05;
		rock += (fine_noise - 0.5) * 0.025;

		vec3 color;
		float roughness;

		if (biome == 15) {
			// ALPINE — snow on flat surfaces, exposed rock on steep slopes.
			float snow_noise = value_noise(noise_uv * 1.2) * 0.15;
			float snow_factor = smoothstep(0.55, 0.75, slope + snow_noise);
			color = mix(rock, SNOW_COLOR, snow_factor);
			roughness = mix(0.9, 0.6, snow_factor);  // snow is smoother
		} else {
			// Non-alpine mountains inherit a weak vegetation tint from the biome,
			// while steep slopes stay mostly bare rock.
			vec3 vegetation = get_mountain_vegetation_color(biome) + (large_noise - 0.5) * 0.04;
			float vegetation_factor =
				smoothstep(0.55, 0.85, slope) * get_mountain_vegetation_strength(biome);
			color = mix(rock, vegetation, vegetation_factor);
			roughness = mix(0.9, 0.84, vegetation_factor);
		}

		ALBEDO    = clamp(color, vec3(0.0), vec3(1.0));
		ROUGHNESS = roughness;
		METALLIC  = 0.0;
	}
}
