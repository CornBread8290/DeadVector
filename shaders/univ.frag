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

// must match defs.h
const int MATERIAL_IS_FLAT = (1 << 5);

vec3 faceNormalFromPos() {
    // Recompute per-triangle normal in world space
    vec3 dx = dFdx(v_frag_pos);
    vec3 dy = dFdy(v_frag_pos);
    return normalize(cross(dx, dy));
}

void main(){
    bool isFlat = (u_flags & (1<<5)) != 0;
    //isFlat = false;
    vec3 N = normalize(isFlat ? faceNormalFromPos() : v_normal);
    vec3 V = normalize(u_view_pos - v_frag_pos);

    vec3 L = normalize(-u_light_dir);

    vec3 H = normalize(L + V);

    vec3 base = clamp(u_material_albedo.rgb * v_color.rgb, 0.0, 1.0);
    float a = clamp(u_material_roughness, 0.04, 1.0);
    float m = clamp(u_material_metallic, 0.0, 1.0);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    vec3 F0 = mix(vec3(0.04), base, m);
    vec3  F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    float a2 = a*a;
    float dDen = (NdotH*NdotH)*(a2-1.0)+1.0;
    float D = a2 / (3.14159265 * dDen * dDen);

    float k = (a + 1.0); k = (k*k) / 8.0;
    float Gv = NdotV / (NdotV*(1.0 - k) + k);
    float Gl = NdotL / (NdotL*(1.0 - k) + k);
    float G = Gv * Gl;

    vec3 spec = (D * G * F) / max(4.0 * NdotV * NdotL, 0.001);
    vec3 kd   = (1.0 - F) * (1.0 - m);
    vec3 diff = kd * base * (1.0 / 3.14159265);

    vec3 ambient = 0.01 * base;

    // subtle rim so dark sides aren't dead
    float rim = pow(clamp(1.0 - dot(N, V), 0.0, 1.0), 3.0);
    vec3 rimCol = 0.08 * base * rim;

    vec3 lit = ambient + (diff + spec) * NdotL + u_material_emissive * base + rimCol;

    // optional neutral tone-map
    lit = lit / (lit + vec3(1.0));

    lit = pow(max(lit, 0.0), vec3(1.0/2.2));

    float alpha = clamp(u_material_albedo.a, 0.0, 1.0); // straight alpha
    FragColor = vec4(lit, alpha);
}
