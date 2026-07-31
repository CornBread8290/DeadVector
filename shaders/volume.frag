#version 430 core

in vec3 v_world;
out vec4 FragColor;

layout(location = 0)  uniform mat4 u_view;
layout(location = 4)  uniform mat4 u_proj;
layout(location = 8)  uniform vec4 u_centre;
layout(location = 9)  uniform vec3 u_cam;
layout(location = 10) uniform vec3 u_light;   // direction the sunlight travels
layout(location = 11) uniform vec4 u_origin;
layout(location = 12) uniform vec4 u_axis;
layout(location = 13) uniform vec4 u_tint;
layout(location = 14) uniform vec4 u_par;

#define T_PLUME 0.0
#define T_RCS   1.0
#define T_FIRE  2.0

const int STEPS = 24;

float h3(vec3 p) { return fract(sin(dot(p, vec3(12.9898, 78.233, 37.719))) * 43758.5453); }

float n3(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(h3(i),                h3(i + vec3(1, 0, 0)), f.x),
                   mix(h3(i + vec3(0, 1, 0)), h3(i + vec3(1, 1, 0)), f.x), f.y),
               mix(mix(h3(i + vec3(0, 0, 1)), h3(i + vec3(1, 0, 1)), f.x),
                   mix(h3(i + vec3(0, 1, 1)), h3(i + vec3(1, 1, 1)), f.x), f.y), f.z);
}

float fbm3(vec3 p) {
    float s = 0.0, a = 0.5;
    for (int i = 0; i < 4; ++i) { s += n3(p) * a; p = p * 2.03 + 11.7; a *= 0.5; }
    return s;
}

vec3 blackbody(float K) {
    float t = clamp(K, 800.0, 12000.0) * 0.0001;
    vec3 c = vec3(1.0,
                  clamp(1.35 * t + 0.12, 0.0, 1.0),
                  clamp(2.10 * t - 0.32, 0.0, 1.0));
    return c * c;
}

float density(vec3 p, out float temp) {
    vec3  q = p - u_origin.xyz;
    float L = u_origin.w;
    float s = dot(q, u_axis.xyz);          // distance downstream of the mouth

    temp = 0.0;
    if (s < 0.0 || s > L) return 0.0;

    float r = length(q - u_axis.xyz * s);  // distance off the axis
    float a = s / L;

    float flare = (u_par.w == T_RCS) ? 2.6 : 1.0;
    float w = u_axis.w * (0.55 + flare * 0.90 * pow(a, 0.75));

    float mach = 1.0;
    if (u_par.w == T_PLUME) {
        float k = cos(a * 34.0) * exp(-a * 5.0);
        w    *= 1.0 - 0.28 * k;
        mach  = 1.0 + 0.90 * max(k, 0.0);
    }

    float radial = 1.0 - smoothstep(0.35 * w, w, r);
    if (radial <= 0.0) return 0.0;

    float scale  = (u_par.w == T_FIRE) ? 2.2 : 3.4;
    float drift  = (u_par.w == T_FIRE) ? 1.1 : 26.0;
    float turb   = fbm3(p * scale - u_axis.xyz * (u_par.y * drift) + u_par.z);
    float d      = radial * mix(1.0, turb * 1.7, smoothstep(0.05, 0.90, a));

    d *= 1.0 - smoothstep(0.55, 1.0, a);

    temp = clamp(mach * (1.0 - a * 0.85) * (0.4 + 0.6 * radial), 0.0, 1.0);
    return max(d, 0.0) * u_tint.a;
}

void main() {
    vec3 ro = u_cam;
    vec3 rd = normalize(v_world - u_cam);

    vec3  oc = ro - u_centre.xyz;
    float b  = dot(oc, rd);
    float c  = dot(oc, oc) - u_centre.w * u_centre.w;
    float disc = b * b - c;
    if (disc < 0.0) discard;
    disc = sqrt(disc);

    float t0 = max(-b - disc, 0.0);
    float t1 = -b + disc;
    if (t1 <= t0) discard;

    float dt = (t1 - t0) / float(STEPS);
    float jit = fract(sin(dot(gl_FragCoord.xy, vec2(12.9898, 78.233))) * 43758.545 + u_par.y);
    float t = t0 + dt * jit;

    vec3  L  = normalize(-u_light);
    float mu = dot(rd, L);
    float g  = 0.35;                        // Henyey-Greenstein, forward biased
    float phase = (1.0 - g * g) / (12.566 * pow(1.0 + g * g - 2.0 * g * mu, 1.5));

    vec3  acc = vec3(0.0);
    float trans = 1.0;

    for (int i = 0; i < STEPS; ++i) {
        vec3 p = ro + rd * t;
        float temp, ignored;
        float d = density(p, temp);

        if (d > 0.001) {
            vec3 emit = blackbody(900.0 + 2600.0 * temp) * (temp * temp * temp) * u_par.x;

            float sh = exp(-density(p + L * (u_axis.w * 1.2), ignored) * 4.0);
            vec3 scat = u_tint.rgb * sh * phase * 3.0;

            float alpha = 1.0 - exp(-d * 3.0 * dt);
            acc   += trans * (emit * u_tint.rgb + scat) * alpha;
            trans *= 1.0 - alpha;
            if (trans < 0.01) break;
        }
        t += dt;
    }

    float alpha = 1.0 - trans;
    if (alpha < 0.003) discard;
    FragColor = vec4(acc, alpha);   // premultiplied
}
