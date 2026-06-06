#version 330 core

in vec2 v_uv;
out vec4 FragColor;

uniform sampler2D u_scene_tex;
uniform vec2 u_screen_size;

vec3 tonemap(vec3 x) {
    return x / (x + vec3(1.0));
}

vec3 bloom_sample(vec2 uv) {
    vec2 texel = 1.0 / max(u_screen_size, vec2(1.0));
    vec2 offsets[5] = vec2[](
        vec2(0.0, 0.0),
        vec2(1.5, 0.0),
        vec2(-1.5, 0.0),
        vec2(0.0, 1.5),
        vec2(0.0, -1.5)
    );

    vec3 accum = vec3(0.0);
    for (int i = 0; i < 5; ++i) {
        vec3 sample_color = texture(u_scene_tex, uv + offsets[i] * texel).rgb;
        float bright = max(max(sample_color.r, sample_color.g), sample_color.b);
        accum += sample_color * clamp(bright - 1.0, 0.0, 6.0);
    }
    return accum * 0.06;
}

void main() {
    vec3 color = texture(u_scene_tex, v_uv).rgb;
    color += bloom_sample(v_uv);
    color *= 0.72;

    vec2 centered = v_uv * 2.0 - 1.0;
    float vignette = 1.0 - dot(centered, centered) * 0.18;
    color *= clamp(vignette, 0.72, 1.0);

    color = tonemap(color);
    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
