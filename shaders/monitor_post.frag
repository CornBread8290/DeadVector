#version 330 core

in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_scene_tex;
uniform vec2 u_screen_size;
uniform float u_time;

float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

void main() {
    vec2 centered = v_uv * 2.0 - 1.0;
    vec2 warped = v_uv + centered * dot(centered, centered) * 0.015;
    vec3 color = texture(u_scene_tex, clamp(warped, vec2(0.001), vec2(0.999))).rgb;

    float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(vec3(luma), color, 0.25);
    color *= vec3(0.70, 0.95, 0.82);

    float scan = 0.92 + 0.08 * sin((v_uv.y + u_time * 0.06) * u_screen_size.y * 1.6);
    vec2 cell = floor(v_uv * u_screen_size * 0.5);
    float noise = hash12(cell + fract(vec2(u_time * 13.7, u_time * 7.3)) * 100.0) * 0.06;
    float vignette = 1.0 - dot(centered, centered) * 0.26;
    color = (color + noise) * scan * clamp(vignette, 0.68, 1.0);

    FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
