#version 430 core

in vec3 v_world;
out vec4 FragColor;

layout(location = 0)  uniform mat4 u_view;
layout(location = 4)  uniform mat4 u_proj;
layout(location = 8)  uniform vec4 u_centre;
layout(location = 9)  uniform vec3 u_cam;
layout(location = 10) uniform vec3 u_light;   // direction the sunlight travels
layout(location = 11) uniform vec4 u_planet;  // xyz centre, w radius
layout(location = 12) uniform vec4 u_axis;    // xyz ring plane normal
layout(location = 13) uniform vec4 u_radii;

float rh(float x) { return fract(sin(x * 127.1) * 43758.5453); }
float rn(float x) { float i = floor(x), f = fract(x); f = f * f * (3.0 - 2.0 * f); return mix(rh(i), rh(i + 1.0), f); }

float ring_bands(float a) {
    float d = 0.55 * rn(a * 38.0) + 0.28 * rn(a * 97.0) + 0.17 * rn(a * 260.0);
    d = smoothstep(0.18, 0.85, d);
    d *= smoothstep(0.004, 0.030, abs(a - 0.42));   // Cassini-style divisions
    d *= smoothstep(0.004, 0.020, abs(a - 0.71));
    d *= smoothstep(0.0, 0.06, a) * (1.0 - smoothstep(0.90, 1.0, a));
    return d;
}

void main() {
    vec3 rd = normalize(v_world - u_cam);
    vec3 N  = normalize(u_axis.xyz);

    float dn = dot(rd, N);
    if (abs(dn) < 1e-5) discard;

    float t = dot(u_planet.xyz - u_cam, N) / dn;
    if (t <= 0.0) discard;

    vec3  p = u_cam + rd * t;
    float r = length(p - u_planet.xyz);
    float a = (r - u_radii.x) / (u_radii.y - u_radii.x);
    if (a < 0.0 || a > 1.0) discard;

    float dens = ring_bands(a);
    if (dens < 0.002) discard;

    float alpha = 1.0 - exp(-u_radii.z * dens / max(abs(dn), 0.02));

    // Planet shadow, exact ray/sphere toward the sun.
    vec3  L  = normalize(-u_light);
    vec3  oc = p - u_planet.xyz;
    float b  = dot(oc, L);
    float shadow = (b < 0.0 && b * b - (dot(oc, oc) - u_planet.w * u_planet.w) > 0.0) ? 0.06 : 1.0;

    float mu = dot(rd, L);
    float phase = 0.35 + 2.6 * pow(max(mu, 0.0), 6.0);

    vec3 tint = mix(vec3(0.86, 0.83, 0.78), vec3(0.74, 0.62, 0.47), dens);
    vec3 col  = tint * phase * shadow * 1.15;

    vec4 clip = u_proj * u_view * vec4(p, 1.0);
    gl_FragDepth = clip.z / clip.w * 0.5 + 0.5;

    FragColor = vec4(col * alpha, alpha);   // premultiplied
}
