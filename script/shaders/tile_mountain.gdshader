shader_type spatial;

// Mountain shader — applied to RELIEF_MOUNTAIN and RELIEF_HIGH_MOUNTAIN groups.
// INSTANCE_CUSTOM.x = biome_id, .w = overlay_value.
// Procedural: snow by world-space normal steepness, rock/grass by slope, noise variation.

uniform int view_mode : hint_range(0, 4) = 0;

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
	float hue = fract(t * 0.618 + 0.1);
	vec3 c = clamp(abs(mod(hue * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
	return mix(vec3(0.3), c, 0.7);
}

// ── Material colours ─────────────────────────────────────────────

const vec3 ROCK_COLOR     = vec3(0.45, 0.42, 0.38);
const vec3 ROCK_ALT_COLOR = vec3(0.38, 0.35, 0.30);
const vec3 SNOW_COLOR     = vec3(0.95, 0.96, 0.98);
const vec3 GRASS_COLOR    = vec3(0.30, 0.38, 0.18);

// ── Fragment ─────────────────────────────────────────────────────

void fragment() {
	// ── Overlay mode: early exit with heatmap colour ──
	if (view_mode > 0) {
		ALBEDO = heatmap_ramp(overlay_value, view_mode);
		ROUGHNESS = 0.9;
		METALLIC = 0.0;
	} else {
		int biome = clamp(int(biome_id + 0.5), 0, 7);

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

		if (biome == 6) {
			// ALPINE — snow on flat surfaces, exposed rock on steep slopes.
			float snow_noise = value_noise(noise_uv * 1.2) * 0.15;
			float snow_factor = smoothstep(0.55, 0.75, slope + snow_noise);
			color = mix(rock, SNOW_COLOR, snow_factor);
			roughness = mix(0.9, 0.6, snow_factor);  // snow is smoother
		} else if (biome == 5) {
			// HIGHLAND — grass on flat tops, rock on steep sides.
			vec3 grass = GRASS_COLOR + (large_noise - 0.5) * 0.04;
			float grass_factor = smoothstep(0.5, 0.8, slope);
			color = mix(rock, grass, grass_factor);
			roughness = mix(0.9, 0.85, grass_factor);
		} else {
			// Generic mountain — rock with subtle moss on flat surfaces.
			vec3 moss = vec3(0.28, 0.32, 0.22);
			float moss_factor = smoothstep(0.6, 0.85, slope) * large_noise * 0.4;
			color = mix(rock, moss, moss_factor);
			roughness = 0.9;
		}

		ALBEDO    = clamp(color, vec3(0.0), vec3(1.0));
		ROUGHNESS = roughness;
		METALLIC  = 0.0;
	}
}
