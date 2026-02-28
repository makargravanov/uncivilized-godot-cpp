shader_type spatial;

// Mountain shader — applied to RELIEF_MOUNTAIN and RELIEF_HIGH_MOUNTAIN groups.
// INSTANCE_CUSTOM.x = biome_id.
// Procedural: snow by world-space normal steepness, rock/grass by slope, noise variation.

varying flat float biome_id;
varying vec3 world_pos;
varying vec3 world_normal;

void vertex() {
	biome_id     = INSTANCE_CUSTOM.x;
	world_pos    = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
	world_normal = normalize((MODEL_MATRIX * vec4(NORMAL, 0.0)).xyz);
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

// ── Material colours ─────────────────────────────────────────────

const vec3 ROCK_COLOR     = vec3(0.45, 0.42, 0.38);
const vec3 ROCK_ALT_COLOR = vec3(0.38, 0.35, 0.30);
const vec3 SNOW_COLOR     = vec3(0.95, 0.96, 0.98);
const vec3 GRASS_COLOR    = vec3(0.30, 0.38, 0.18);

// ── Fragment ─────────────────────────────────────────────────────

void fragment() {
	int biome = clamp(int(biome_id + 0.5), 0, 7);

	vec2 noise_uv = world_pos.xz;

	// Slope factor: 1.0 = flat top, 0.0 = vertical cliff.
	float slope = dot(world_normal, vec3(0.0, 1.0, 0.0));
	slope = clamp(slope, 0.0, 1.0);

	// Noise layers for natural variation.
	float large_noise = fbm(noise_uv * 0.15, 3);
	float med_noise   = fbm(noise_uv * 0.5, 2);
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
