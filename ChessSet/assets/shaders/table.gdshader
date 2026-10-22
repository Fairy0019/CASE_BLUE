shader_type spatial;
render_mode diffuse_burley, specular_schlick_ggx;

uniform vec3 center_color : source_color = vec3(0.085, 0.070, 0.055);
uniform vec3 edge_color : source_color = vec3(0.010, 0.010, 0.011);
uniform float pool_radius = 1.15;

varying vec3 world_pos;

void vertex() {
	world_pos = (MODEL_MATRIX * vec4(VERTEX, 1.0)).xyz;
}

void fragment() {
	float d = length(world_pos.xz);
	float t = smoothstep(0.08, pool_radius, d);
	ALBEDO = mix(center_color, edge_color, t);
	ROUGHNESS = 0.78;
	SPECULAR = 0.18;
}
