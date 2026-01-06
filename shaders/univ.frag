#version 330 core
out vec4 FragColor;

in vec3 v_frag_pos;
in vec3 v_normal;
in vec4 v_color;

uniform vec3  u_light_dir;        // light -> scene
uniform vec3  u_view_pos;

uniform vec4  u_material_albedo;  // rgba; a is transparency
uniform float u_material_roughness;
uniform float u_material_metallic;
uniform float u_material_emissive;

uniform int   u_flags;            // bitfield
const int MATERIAL_FLAG_ALBEDO            = (1 << 0);
const int MATERIAL_FLAG_NORMAL            = (1 << 1);
const int MATERIAL_FLAG_EMISSIVEF0SWITCH  = (1 << 2);
const int MATERIAL_IS_FLAT                = (1 << 5);

const float PI = 3.14159265;

vec3 faceNormalFromPos() {
    vec3 dx = dFdx(v_frag_pos);
    vec3 dy = dFdy(v_frag_pos);
    return normalize(cross(dx, dy));
}

float saturate(float x) { return clamp(x, 0.0, 1.0); }

float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float d  = (NoH * NoH) * (a2 - 1.0) + 1.0;
    return a2 / max(PI * d * d, 1e-6);
}

float G_Smith_SchlickGGX(float NoV, float NoL, float a) {
    float k = (a + 1.0);
    k = (k * k) / 8.0; // UE4-style
    float gv = NoV / (NoV * (1.0 - k) + k);
    float gl = NoL / (NoL * (1.0 - k) + k);
    return gv * gl;
}

vec3 F_Schlick(float VoH, vec3 F0) {
    float f = pow(1.0 - VoH, 5.0);
    return F0 + (1.0 - F0) * f;
}

// Specular anti-aliasing (helps glittering on rough/noisy normals)
float aaRoughness(float a, vec3 N) {
    float nvar = max(dot(dFdx(N), dFdx(N)), dot(dFdy(N), dFdy(N)));
    // Tunable constant; small value keeps it subtle
    return clamp(sqrt(a*a + nvar), 0.04, 1.0);
}

void main() {
    bool isFlat = (u_flags & MATERIAL_IS_FLAT) != 0;
    bool isEmissive = (u_flags & MATERIAL_FLAG_EMISSIVEF0SWITCH) == 0;
    vec3 N = normalize(isFlat ? faceNormalFromPos() : v_normal);
    vec3 V = normalize(u_view_pos - v_frag_pos);
    vec3 L = normalize(-u_light_dir);
    vec3 H = normalize(L + V);

    float NoL = saturate(dot(N, L));
    float NoV = saturate(dot(N, V));
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));

    vec3 base = clamp(u_material_albedo.rgb * v_color.rgb, 0.0, 1.0);

    float rough = clamp(u_material_roughness, 0.04, 1.0);
    rough = aaRoughness(rough, N);             // reduce “sparkle”
    float metal = clamp(u_material_metallic, 0.0, 1.0);

    float f0 = isEmissive ? 0.04 : clamp(u_material_emissive, 0.0, 1.0);
    vec3 F0 = mix(vec3(f0), base, metal);

    vec3  F = F_Schlick(VoH, F0);
    float D = D_GGX(NoH, rough);
    float G = G_Smith_SchlickGGX(NoV, NoL, rough);

    vec3 spec = (D * G) * F / max(4.0 * NoV * NoL, 1e-4);

    // Energy conserving diffuse term
    vec3 kd = (1.0 - F) * (1.0 - metal);
    vec3 diff = kd * base * (1.0 / PI);

    vec3 ambient = base * 0.1; //0.005 for space

    // Single directional light
    vec3 lit = ambient + (diff + spec) * NoL;

    // Emissive is in linear space
    if (isEmissive) lit += u_material_emissive * base;

    float alpha = clamp(u_material_albedo.a, 0.0, 1.0);
    FragColor = vec4(lit, alpha);
}
