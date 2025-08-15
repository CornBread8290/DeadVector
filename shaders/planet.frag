#version 330 core
out vec4 FragColor;

in vec3 v_position;
in vec3 v_normal;

uniform vec3 u_view_pos;
uniform vec3 u_light_dir;

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1, 0));
    float c = hash(i + vec2(0, 1));
    float d = hash(i + vec2(1, 1));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

float fbm(vec2 p) {
    float total = 0.0;
    float amp = 0.5;
    for (int i = 0; i < 5; i++) {
        total += noise(p) * amp;
        p *= 2.0;
        amp *= 0.5;
    }
    return total;
}

void main() {
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_view_pos - v_position);
    vec3 L = normalize(-u_light_dir);
    vec3 H = normalize(L + V);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 64.0);

    vec3 sphere_p = normalize(v_position);
    float lat = sphere_p.y;

    float bands = sin(lat * 50.0 + fbm(sphere_p.xz * 4.0)) * 0.5 + 0.5;
    float storms = fbm(sphere_p.xz * 8.0 + vec2(0.0, v_position.y * 0.1));

    vec3 base_color = mix(vec3(1.0, 0.6, 0.1), vec3(0.4, 0.2, 0.9), bands);
    base_color += storms * 0.1;

    vec3 color = base_color * diff + vec3(spec);
    FragColor = vec4(color, 1.0);
    //FragColor = vec4(1.0, 0.0, 1.0, 1.0); // bright pink
}
