shader_type spatial;

// Ocean shader — applied to RELIEF_OCEAN MultiMesh group.
// INSTANCE_CUSTOM.x = biome_id (0–3 for ocean biomes), .w = overlay_value.
// Features:
//   - Animated ripple via procedural normal map (no vertex displacement)
//   - Parallax fake-depth: view-dependent UV shift reveals "seabed" layer
//   - Deep vs shallow biome controls depth amount and colour

uniform int view_mode : hint_range(0, 6) = 0;

varying flat float biome_id;
varying flat float features;
varying flat float overlay_value;
varying vec3 world_pos;

void vertex() {
	biome_id      = INSTANCE_CUSTOM.x;
	features      = INSTANCE_CUSTOM.z;
	overlay_value = INSTANCE_CUSTOM.w;
	world_pos     = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

bool has_ice_cover(float feature_flags) {
	return mod(floor(feature_flags / 16.0), 2.0) >= 0.5;
}

// ── Hash / noise (same as land shader, duplicated to keep shaders self-contained) ──

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

// ── Animated ripple normal ──────────────────────────────────────

vec3 ripple_normal(vec2 uv, float time) {
	// Two scrolling noise layers at different scales and directions.
	float n1 = value_noise(uv * 2.5 + vec2(time * 0.08, time * 0.06));
	float n2 = value_noise(uv * 4.0 + vec2(-time * 0.05, time * 0.09));

	// Derive normal from noise gradients (central-difference approximation).
	float eps = 0.05;
	float dx1 = value_noise((uv + vec2(eps, 0.0)) * 2.5 + vec2(time * 0.08, time * 0.06))
	          - value_noise((uv - vec2(eps, 0.0)) * 2.5 + vec2(time * 0.08, time * 0.06));
	float dy1 = value_noise((uv + vec2(0.0, eps)) * 2.5 + vec2(time * 0.08, time * 0.06))
	          - value_noise((uv - vec2(0.0, eps)) * 2.5 + vec2(time * 0.08, time * 0.06));

	float dx2 = value_noise((uv + vec2(eps, 0.0)) * 4.0 + vec2(-time * 0.05, time * 0.09))
	          - value_noise((uv - vec2(eps, 0.0)) * 4.0 + vec2(-time * 0.05, time * 0.09));
	float dy2 = value_noise((uv + vec2(0.0, eps)) * 4.0 + vec2(-time * 0.05, time * 0.09))
	          - value_noise((uv - vec2(0.0, eps)) * 4.0 + vec2(-time * 0.05, time * 0.09));

	// Combine both layers.
	float dx = (dx1 + dx2 * 0.5) * 0.7;
	float dy = (dy1 + dy2 * 0.5) * 0.7;

	return normalize(vec3(-dx, 1.0, -dy));
}

// ── Parallax depth (fake seabed) ────────────────────────────────

vec3 parallax_seabed(vec2 uv, vec3 view_dir_tangent, float depth_scale, float time) {
	// Shift UV by view direction to simulate looking "into" the water.
	vec2 parallax_offset = view_dir_tangent.xz / max(view_dir_tangent.y, 0.15) * depth_scale;
	vec2 deep_uv = uv - parallax_offset;

	// Slow-moving seabed caustics pattern.
	float caustics1 = value_noise(deep_uv * 1.5 + vec2(time * 0.02, -time * 0.015));
	float caustics2 = value_noise(deep_uv * 3.0 + vec2(-time * 0.01, time * 0.025));
	float caustics = caustics1 * 0.6 + caustics2 * 0.4;

	// Dark seabed base with bright caustic highlights.
	vec3 seabed_color = vec3(0.04, 0.06, 0.12);
	seabed_color += vec3(0.03, 0.06, 0.08) * smoothstep(0.45, 0.75, caustics);

	return seabed_color;
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

// ── Colour palettes ─────────────────────────────────────────────

void get_ocean_params(int biome, out vec3 surface_col, out float depth, out float specular_val) {
	// Biome 0 = DEEP_OCEAN, 1 = SHALLOW_OCEAN, 2 = INLAND_SEA, 3 = SHALLOW_INLAND_SEA.
	if (biome == 1) {
		surface_col  = vec3(0.12, 0.42, 0.50);   // SHALLOW_OCEAN — teal
		depth        = 0.06;                       // less parallax depth
		specular_val = 0.5;
	} else if (biome == 3) {
		surface_col  = vec3(0.11, 0.38, 0.44);   // SHALLOW_INLAND_SEA
		depth        = 0.05;
		specular_val = 0.45;
	} else if (biome == 2) {
		surface_col  = vec3(0.08, 0.12, 0.24);   // INLAND_SEA — dark
		depth        = 0.12;
		specular_val = 0.4;
	} else {
		surface_col  = vec3(0.07, 0.11, 0.22);   // DEEP_OCEAN — darkest
		depth        = 0.15;                       // maximum parallax depth
		specular_val = 0.5;
	}
}

// ── Fragment ─────────────────────────────────────────────────────

void fragment() {
	// ── Overlay mode: early exit with heatmap colour ──
	if (view_mode > 0) {
		ALBEDO = heatmap_ramp(overlay_value, view_mode);
		ROUGHNESS = 0.9;
		METALLIC = 0.0;
	} else {
		int biome = clamp(int(biome_id + 0.5), 0, 12);
		vec2 uv = world_pos.xz;

		// Biome-dependent parameters.
		vec3 surface_col;
		float depth_scale;
		float specular_val;
		get_ocean_params(biome, surface_col, depth_scale, specular_val);

		// ── Animated ripple normal ──
		vec3 ripple = ripple_normal(uv, TIME);
		NORMAL_MAP = ripple * 0.5 + 0.5;
		NORMAL_MAP_DEPTH = 0.6;

		// ── Parallax seabed ──
		// Approximate tangent-space view direction from camera.
		vec3 view_world = normalize(CAMERA_POSITION_WORLD - world_pos);
		vec3 seabed = parallax_seabed(uv, view_world, depth_scale, TIME);

		// Fresnel: more reflection at grazing angles, more depth when looking straight down.
		float fresnel = pow(1.0 - clamp(dot(view_world, vec3(0.0, 1.0, 0.0)), 0.0, 1.0), 3.0);

		// Blend surface colour (reflective) with seabed (depth).
		vec3 color = mix(seabed, surface_col, 0.35 + fresnel * 0.55);

		// Subtle caustic shimmer on the surface.
		float shimmer = value_noise(uv * 5.0 + vec2(TIME * 0.12, TIME * 0.08));
		color += vec3(0.02, 0.03, 0.04) * smoothstep(0.55, 0.85, shimmer);

		if (has_ice_cover(features)) {
			vec3 ice_color = vec3(0.83, 0.90, 0.96);
			float ice_noise = value_noise(uv * 1.8 + vec2(TIME * 0.01, -TIME * 0.008));
			color = mix(ice_color * 0.92, ice_color, smoothstep(0.25, 0.75, ice_noise));
			METALLIC  = 0.02;
			ROUGHNESS = 0.72;
			SPECULAR  = 0.18;
			ALBEDO    = color;
		} else {
			ALBEDO    = color;
			METALLIC  = 0.2;
			ROUGHNESS = 0.15 + (1.0 - fresnel) * 0.2;  // smoother at glancing angles
			SPECULAR  = specular_val;
		}
	}
}
