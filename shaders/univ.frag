#version 330 core
out vec4 FragColor;

in vec3 v_frag_pos;
in vec3 v_normal;
in vec4 v_color;
in vec2 v_uv;
in vec4 v_light_space_pos;

uniform vec3  u_view_pos;

uniform vec4  u_material_albedo;  // rgba; a is transparency
uniform float u_material_roughness;
uniform float u_material_metallic;
uniform float u_material_emissive;
uniform int   u_material_flags;
uniform int   u_mesh_flags;
uniform sampler2D u_albedo_tex;
uniform sampler2D u_shadow_map;
uniform sampler2D u_reflection_tex;
uniform sampler2D u_refraction_tex;
uniform int   u_light_count;
uniform vec4  u_light_pos_type[4];
uniform vec4  u_light_dir_inner[4];
uniform vec4  u_light_color_outer[4];
uniform vec4  u_light_params[4];
uniform mat4  u_reflection_view_proj;
uniform vec2  u_screen_size;
uniform int   u_render_features;
const int MATERIAL_FLAG_ALBEDO            = (1 << 0);
const int MATERIAL_FLAG_NORMAL            = (1 << 1);
const int MATERIAL_FLAG_EMISSIVEF0SWITCH  = (1 << 2);
const int MATERIAL_FLAG_REFLECTION        = (1 << 3);
const int MATERIAL_FLAG_REFRACTION        = (1 << 4);
const int MATERIAL_IS_FLAT                = (1 << 5);
const int MESH_HAS_UVS                    = (1 << 1);
const int LIGHT_DIRECTIONAL               = 0;
const int LIGHT_POINT                     = 1;
const int LIGHT_SPOT                      = 2;
const int RENDER_FEATURE_REFLECTION       = (1 << 0);
const int RENDER_FEATURE_REFRACTION       = (1 << 1);

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

float sampleShadow(float NoL) {
    vec3 proj = v_light_space_pos.xyz / max(v_light_space_pos.w, 1e-5);
    proj = proj * 0.5 + 0.5;

    if (proj.z <= 0.0 || proj.z >= 1.0) return 1.0;
    if (proj.x <= 0.0 || proj.x >= 1.0 || proj.y <= 0.0 || proj.y >= 1.0) return 1.0;

    float bias = max(0.00035 * (1.0 - NoL), 0.00005);
    vec2 texel = 1.0 / vec2(textureSize(u_shadow_map, 0));
    float vis = 0.0;

    for (int y = -1; y <= 1; ++y) {
        for (int x = -1; x <= 1; ++x) {
            float depth = texture(u_shadow_map, proj.xy + vec2(x, y) * texel).r;
            vis += (proj.z - bias <= depth) ? 1.0 : 0.0;
        }
    }

    return vis / 9.0;
}

float hash21(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453123);
}

float vnoise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    vec2 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm2(vec2 p) {
    float a = 0.5;
    float s = 0.0;
    for (int i = 0; i < 4; ++i) {
        s += a * vnoise(p);
        p *= 2.12;
        a *= 0.5;
    }
    return s;
}

vec3 sampleReflectionSky(vec3 dir) {
    dir = normalize(dir);
    const vec3 pole = vec3(0.0, 1.0, 0.0);
    const vec3 center = vec3(1.0, 0.0, 0.0);

    vec3 X = normalize(center - dot(center, pole) * pole);
    vec3 Y = normalize(cross(pole, X));
    float lat = asin(clamp(dot(dir, pole), -1.0, 1.0));
    float lon = atan(dot(dir, Y), dot(dir, X));

    float band = exp(-0.5 * (lat * lat) / (0.24 * 0.24));
    float bulge = exp(-0.5 * (lon * lon) / (0.30 * 0.30));
    vec2 galUV = vec2(lon * 4.0, lat / 0.24);
    float dust = fbm2(galUV * 1.7) * 0.6 + 0.4 * fbm2(galUV * 3.3);
    float glow = 1.2 * band * mix(0.55, 1.35, bulge) * smoothstep(0.25, 0.95, dust);
    vec3 bandColor = mix(vec3(0.6, 0.75, 1.0), vec3(1.0, 0.93, 0.85), clamp(0.35 * bulge + 0.65 * band, 0.0, 1.0));

    vec2 starUV = vec2(lon * cos(lat), lat);
    float starNoise = max(vnoise(starUV * 200.0 + 7.1), vnoise(starUV * 140.0 - 3.7));
    float starMask = smoothstep(0.985, 0.9995, starNoise);
    vec3 starColor = mix(vec3(1.0, 0.92, 0.85), vec3(0.62, 0.78, 1.0), hash21(floor(starUV * 64.0)));

    return vec3(0.003, 0.005, 0.009) + bandColor * glow + starColor * starMask * (0.35 + 0.65 * (0.4 * band + 0.6 * bulge)) * 2.0;
}

// rgb = reflected scene, a = validity (fades at the map edge so the
// fallback sky takes over without a seam)
vec4 samplePlanarReflection(float roughness) {
    vec4 clip = u_reflection_view_proj * vec4(v_frag_pos, 1.0);
    if (clip.w <= 1e-5) return vec4(0.0);

    vec3 proj = clip.xyz / clip.w;
    vec2 uv = proj.xy * 0.5 + 0.5;
    if (proj.z <= 0.0 || proj.z >= 1.0) return vec4(0.0);

    float edge = smoothstep(0.0, 0.04, uv.x) * smoothstep(1.0, 0.96, uv.x)
               * smoothstep(0.0, 0.04, uv.y) * smoothstep(1.0, 0.96, uv.y);
    if (edge <= 0.0) return vec4(0.0);

    return vec4(textureLod(u_reflection_tex, uv, roughness * 6.0).rgb, edge);
}

vec3 sampleScreenRefraction(vec3 N, float strength) {
    vec2 uv = gl_FragCoord.xy / max(u_screen_size, vec2(1.0));
    uv += N.xy * strength;
    uv = clamp(uv, vec2(0.001), vec2(0.999));
    return texture(u_refraction_tex, uv).rgb;
}

void main() {
    bool isFlat = (u_material_flags & MATERIAL_IS_FLAT) != 0;
    bool isEmissive = (u_material_flags & MATERIAL_FLAG_EMISSIVEF0SWITCH) == 0;
    bool hasAlbedoTex = (u_material_flags & MATERIAL_FLAG_ALBEDO) != 0 && (u_mesh_flags & MESH_HAS_UVS) != 0;
    bool hasReflection = (u_material_flags & MATERIAL_FLAG_REFLECTION) != 0 && (u_render_features & RENDER_FEATURE_REFLECTION) != 0;
    bool hasRefraction = (u_material_flags & MATERIAL_FLAG_REFRACTION) != 0 && (u_render_features & RENDER_FEATURE_REFRACTION) != 0;
    vec3 N = normalize(isFlat ? faceNormalFromPos() : v_normal);
    vec3 V = normalize(u_view_pos - v_frag_pos);
    float NoV = saturate(dot(N, V));

    vec4 texel = hasAlbedoTex ? texture(u_albedo_tex, v_uv) : vec4(1.0);
    vec4 baseColor = clamp(u_material_albedo * v_color * texel, 0.0, 1.0);
    vec3 base = baseColor.rgb;

    float rough = clamp(u_material_roughness, 0.04, 1.0);
    rough = aaRoughness(rough, N);             // reduce “sparkle”
    float metal = clamp(u_material_metallic, 0.0, 1.0);

    float f0 = isEmissive ? 0.04 : clamp(u_material_emissive, 0.0, 1.0);
    vec3 F0 = mix(vec3(f0), base, metal);

    // Space ambient should stay very low.
    vec3 ambient = base * 0.015;

    vec3 lit = ambient;

    for (int i = 0; i < 4; ++i) {
        if (i >= u_light_count) break;

        int type = int(u_light_pos_type[i].w + 0.5);
        vec3 lightPos = u_light_pos_type[i].xyz;
        vec3 lightDir = normalize(u_light_dir_inner[i].xyz);
        vec3 lightColor = u_light_color_outer[i].rgb;
        float innerCos = u_light_dir_inner[i].w;
        float outerCos = u_light_color_outer[i].w;
        float range = u_light_params[i].x;
        float intensity = u_light_params[i].y;
        float castsShadow = u_light_params[i].z;

        vec3 L = vec3(0.0);
        float attenuation = 1.0;

        if (type == LIGHT_DIRECTIONAL) {
            L = normalize(-lightDir);
        } else {
            vec3 toLight = lightPos - v_frag_pos;
            float dist = length(toLight);
            if (dist <= 1e-4 || dist >= range) continue;
            L = toLight / dist;
            attenuation = 1.0 - clamp(dist / range, 0.0, 1.0);
            attenuation *= attenuation;

            if (type == LIGHT_SPOT) {
                float cone = dot(lightDir, normalize(-toLight));
                attenuation *= smoothstep(outerCos, innerCos, cone);
                if (attenuation <= 0.0) continue;
            }
        }

        float NoL = saturate(dot(N, L));
        if (NoL <= 0.0) continue;

        vec3 H = normalize(L + V);
        float NoH = saturate(dot(N, H));
        float VoH = saturate(dot(V, H));
        vec3  F = F_Schlick(VoH, F0);
        float D = D_GGX(NoH, rough);
        float G = G_Smith_SchlickGGX(NoV, NoL, rough);
        vec3 spec = (D * G) * F / max(4.0 * NoV * NoL, 1e-4);
        vec3 kd = (1.0 - F) * (1.0 - metal);
        vec3 diff = kd * base * (1.0 / PI);
        float shadow = (type == LIGHT_DIRECTIONAL && castsShadow > 0.5) ? sampleShadow(NoL) : 1.0;
        vec3 radiance = lightColor * intensity * attenuation * shadow;

        lit += (diff + spec) * NoL * radiance;
    }

    // Emissive is in linear space
    if (isEmissive) lit += u_material_emissive * base;

    if (hasReflection) {
        vec4 planar = samplePlanarReflection(rough);
        vec3 sky_reflection = sampleReflectionSky(reflect(-V, N));
        vec3 reflection_sample = mix(sky_reflection, planar.rgb, planar.a);
        float fresnel = pow(1.0 - NoV, 5.0);
        float reflectivity = clamp(mix(0.18, 1.0, metal) * (1.0 - rough * 0.55), 0.0, 1.0);
        vec3 reflection_tint = mix(vec3(1.0), base, metal);
        lit += reflection_sample * reflection_tint * reflectivity * (0.22 + 0.78 * fresnel);
    }

    float alpha = clamp(baseColor.a, 0.0, 1.0);
    vec3 out_color = lit;

    if (hasRefraction) {
        vec3 refracted = sampleScreenRefraction(N, 0.014 + (1.0 - alpha) * 0.05);
        vec3 transmissive = mix(refracted, refracted * base, 0.45);
        float fresnel_mix = 0.10 + 0.65 * pow(1.0 - NoV, 5.0);
        out_color = mix(transmissive, lit, fresnel_mix);
    }

    FragColor = vec4(out_color, alpha);
}
