shader_type spatial;

uniform int view_mode : hint_range(0, 6) = 0;

varying flat float biome_id;
varying flat float cryosphere_value;
varying flat float overlay_value;
varying vec3 world_pos;

void vertex() {
	biome_id = INSTANCE_CUSTOM.x;
	cryosphere_value = INSTANCE_CUSTOM.y;
	overlay_value = INSTANCE_CUSTOM.w;
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

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

vec3 ripple_normal(vec2 uv, float time) {
	float eps = 0.05;
	float dx1 = value_noise((uv + vec2(eps, 0.0)) * 2.5 + vec2(time * 0.08, time * 0.06))
	          - value_noise((uv - vec2(eps, 0.0)) * 2.5 + vec2(time * 0.08, time * 0.06));
	float dy1 = value_noise((uv + vec2(0.0, eps)) * 2.5 + vec2(time * 0.08, time * 0.06))
	          - value_noise((uv - vec2(0.0, eps)) * 2.5 + vec2(time * 0.08, time * 0.06));

	float dx2 = value_noise((uv + vec2(eps, 0.0)) * 4.0 + vec2(-time * 0.05, time * 0.09))
	          - value_noise((uv - vec2(eps, 0.0)) * 4.0 + vec2(-time * 0.05, time * 0.09));
	float dy2 = value_noise((uv + vec2(0.0, eps)) * 4.0 + vec2(-time * 0.05, time * 0.09))
	          - value_noise((uv - vec2(0.0, eps)) * 4.0 + vec2(-time * 0.05, time * 0.09));

	float dx = (dx1 + dx2 * 0.5) * 0.7;
	float dy = (dy1 + dy2 * 0.5) * 0.7;

	return normalize(vec3(-dx, 1.0, -dy));
}

vec3 parallax_seabed(vec2 uv, vec3 view_dir_world, float depth_scale, float time) {
	vec2 parallax_offset = view_dir_world.xz / max(view_dir_world.y, 0.15) * depth_scale;
	vec2 deep_uv = uv - parallax_offset;

	float caustics1 = value_noise(deep_uv * 1.5 + vec2(time * 0.02, -time * 0.015));
	float caustics2 = value_noise(deep_uv * 3.0 + vec2(-time * 0.01, time * 0.025));
	float caustics = caustics1 * 0.6 + caustics2 * 0.4;

	vec3 seabed_color = vec3(0.04, 0.06, 0.12);
	seabed_color += vec3(0.03, 0.06, 0.08) * smoothstep(0.45, 0.75, caustics);
	return seabed_color;
}

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

void get_ocean_params(int biome, out vec3 surface_col, out float depth, out float specular_val) {
	if (biome == 1) {
		surface_col = vec3(0.12, 0.42, 0.50);
		depth = 0.06;
		specular_val = 0.5;
	} else if (biome == 3) {
		surface_col = vec3(0.11, 0.38, 0.44);
		depth = 0.05;
		specular_val = 0.45;
	} else if (biome == 2) {
		surface_col = vec3(0.08, 0.12, 0.24);
		depth = 0.12;
		specular_val = 0.4;
	} else {
		surface_col = vec3(0.07, 0.11, 0.22);
		depth = 0.15;
		specular_val = 0.5;
	}
}

void fragment() {
	if (view_mode > 0) {
		ALBEDO = heatmap_ramp(overlay_value, view_mode);
		ROUGHNESS = 0.9;
		METALLIC = 0.0;
	} else {
		int biome = clamp(int(biome_id + 0.5), 0, 12);
		vec2 uv = world_pos.xz;

		vec3 surface_col;
		float depth_scale;
		float specular_val;
		get_ocean_params(biome, surface_col, depth_scale, specular_val);

		float ice_cover = clamp(cryosphere_value, 0.0, 1.0);

		vec3 ripple = ripple_normal(uv, TIME);
		NORMAL_MAP = ripple * 0.5 + 0.5;
		NORMAL_MAP_DEPTH = mix(0.6, 0.08, ice_cover);

		vec3 view_world = normalize(CAMERA_POSITION_WORLD - world_pos);
		vec3 seabed = parallax_seabed(uv, view_world, depth_scale * (1.0 - ice_cover), TIME);

		float fresnel = pow(1.0 - clamp(dot(view_world, vec3(0.0, 1.0, 0.0)), 0.0, 1.0), 3.0);
		vec3 color = mix(seabed, surface_col, 0.35 + fresnel * 0.55);

		float shimmer = value_noise(uv * 5.0 + vec2(TIME * 0.12, TIME * 0.08));
		color += vec3(0.02, 0.03, 0.04) * smoothstep(0.55, 0.85, shimmer) * (1.0 - ice_cover);

		vec3 ice_color = vec3(0.83, 0.90, 0.96);
		float ice_noise = value_noise(uv * 1.8 + vec2(TIME * 0.01, -TIME * 0.008));
		vec3 ice_surface = mix(ice_color * 0.92, ice_color, smoothstep(0.25, 0.75, ice_noise));
		color = mix(color, ice_surface, ice_cover);

		ALBEDO = color;
		METALLIC = mix(0.2, 0.02, ice_cover);
		ROUGHNESS = mix(0.15 + (1.0 - fresnel) * 0.2, 0.72, ice_cover);
		SPECULAR = mix(specular_val, 0.18, ice_cover);
	}
}
