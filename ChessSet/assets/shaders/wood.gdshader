shader_type spatial;
render_mode diffuse_burley, specular_schlick_ggx;

uniform vec3 light_wood : source_color = vec3(0.42, 0.25, 0.11);
uniform vec3 dark_wood : source_color = vec3(0.16, 0.08, 0.03);
uniform float grain_strength : hint_range(0.0, 1.0) = 0.32;
uniform float grain_scale = 1500.0;
uniform float roughness_value : hint_range(0.0, 1.0) = 0.52;

varying vec3 world_pos;

float hash21(vec2 p) {
	p = fract(p * vec2(123.34, 456.21));
	p += dot(p, p + 45.32);
	return fract(p.x * p.y);
}

float value_noise(vec2 p) {
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(
		mix(hash21(i), hash21(i + vec2(1.0, 0.0)), u.x),
		mix(hash21(i + vec2(0.0, 1.0)), hash21(i + vec2(1.0, 1.0)), u.x),
		u.y
	);
}

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float slow = value_noise(vec2(world_pos.x * 12.0, world_pos.z * 55.0));
	float fine = value_noise(vec2(world_pos.x * 38.0, world_pos.z * 360.0));
	float lines = sin(world_pos.z * grain_scale + world_pos.x * 6.0 + slow * 8.0) * 0.5 + 0.5;
	float grain = mix(fine, lines, 0.55);
	vec3 col = mix(light_wood, dark_wood, grain * grain_strength);
	col *= 0.94 + slow * 0.12;
	ALBEDO = col;
	ROUGHNESS = roughness_value;
	SPECULAR = 0.28;
}
