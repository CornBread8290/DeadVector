#version 430 core

in vec3 v_direction;
out vec4 FragColor;

const vec3  G_POLE    = vec3(0.0, 1.0, 0.0);   // band around XZ equator
const vec3  G_CENTER  = vec3(1.0, 0.0, 0.0);   // bright center toward +X

const float BAND_SIGMA     = 0.24;
const float CENTER_SIGMA   = 0.30;   // tighter bright core
const float GLOW_STRENGTH  = 1.45;   // overall Milky Way glow
const float STAR_STRENGTH  = 5.00;   // small stars intensity
const float SCATTER_STRENGTH = 0.22; // sparse polar sparkle
const float TWINKLE        = 0.25;   // small random flicker
const float TIME           = 0.0;    // no app-driven time; keep static

const float NOISE_SCALE_LON = 4.0;

// Improved stars (bigger/softer without loops)
const float STAR_FREQ0   = 200.0;    // was 320.0
const float STAR_FREQ1   = 140.0;    // was 220.0
const float THRESH_A0    = 0.970;    // was 0.985
const float THRESH_B0    = 0.985;    // was 0.992
const float THRESH_C0    = 0.992;    // was 0.997

float hash11(float n){ return fract(sin(n)*43758.5453123); }
float hash21(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7)))*43758.5453123); }
float hash31(vec3 p){ return fract(sin(dot(p, vec3(17.13, 37.11, 91.7)))*43758.5453123); }

float vnoise(vec2 p){
    vec2 i = floor(p), f = fract(p);
    float a = hash21(i);
    float b = hash21(i+vec2(1,0));
    float c = hash21(i+vec2(0,1));
    float d = hash21(i+vec2(1,1));
    vec2 u = f*f*(3.0-2.0*f);
    return mix(mix(a,b,u.x), mix(c,d,u.x), u.y);
}

float fbm(vec2 p){
    float a=0.5, s=0.0;
    for(int i=0;i<5;i++){ s += a * vnoise(p); p *= 2.12; a *= 0.5; }
    return s;
}

vec3 starColor(float t){
    return mix(vec3(1.0,0.92,0.85), vec3(0.62,0.78,1.0), t);
}

void main(){
    vec3 dir = normalize(v_direction);

    vec3 Z = normalize(G_POLE);
    vec3 C = normalize(G_CENTER);
    vec3 X = C - dot(C, Z)*Z;
    float xlen = length(X);
    if (xlen < 1e-4) {
        X = normalize( abs(Z.y) < 0.99 ? cross(Z, vec3(0,1,0)) : cross(Z, vec3(1,0,0)) );
    } else {
        X /= xlen;
    }
    vec3 Y = normalize(cross(Z, X));

    // Galactic coords
    float lat = asin(clamp(dot(dir, Z), -1.0, 1.0)); // distance from plane
    float x = dot(dir, X), y = dot(dir, Y);
    float lon = atan(y, x);                          // [-pi, pi], 0 faces center

    // Milky Way band (Gaussian in latitude)
    float s2   = max(BAND_SIGMA*BAND_SIGMA, 1e-5);
    float band = exp(-0.5 * (lat*lat) / s2);

    float c2    = max(CENTER_SIGMA*CENTER_SIGMA, 1e-5);
    float bulge = exp(-0.5 * (lon*lon) / c2);

    vec2 galUV = vec2(lon * NOISE_SCALE_LON, lat / max(BAND_SIGMA, 1e-3));
    float dust = fbm(galUV*1.7 + vec2(0.0, 0.3*TIME))
               * 0.6 + 0.4*fbm(galUV*3.3 - vec2(0.2*TIME, 0.0));
    float dustMask = smoothstep(0.25, 0.95, dust);

    // Band glow
    float glow = GLOW_STRENGTH * band * mix(0.55, 1.35, bulge) * dustMask
                / (0.9 + 1.0 - max(0.0, dot(dir, X)))*1.0;

    // Base sky
    vec3 col = vec3(0.005, 0.008, 0.012);

    float hueT = smoothstep(0.0, 1.0, 0.35*bulge + 0.65*band);
    vec3 bandColor = mix(vec3(0.6,0.75,1.0), vec3(1.0,0.93,0.85), hueT);
    col += bandColor * glow;

    float densityBoost = 0.35 + 0.65 * mix(0.3, 1.0, bulge) * mix(0.4, 1.0, band);
    vec2 s0 = vec2(lon * cos(lat), lat) * STAR_FREQ0;
    vec2 s1 = vec2((lon * cos(lat))*1.7, lat) * STAR_FREQ1; // keep variety
    float starField =
        smoothstep(THRESH_A0, 1.0, vnoise(s0 + 7.1)) +
        0.7 * smoothstep(THRESH_B0, 1.0, vnoise(s1 + 3.7)) +
        0.4 * smoothstep(THRESH_C0, 1.0, vnoise(s0*1.7 - 4.2));

    float tw = 1.0 + TWINKLE * (hash31(dir*137.0 + TIME)*2.0 - 1.0);
    float starI = STAR_STRENGTH * starField * densityBoost * tw;

    float cSeed = hash11(floor((lon+3.14159)*57.0) + 31.0*floor((lat+1.5708)*19.0));
    vec3  starCol = starColor(cSeed);
    col += starCol * starI;

    // Sparse polar scatter (unchanged)
    float scatter = smoothstep(0.9975, 1.0, vnoise(vec2(dir.y, dir.x)*180.0 + 9.0));
    col += vec3(0.9,0.95,1.0) * SCATTER_STRENGTH * scatter * (1.0 - smoothstep(0.0, 1.0, band));

    // Tonemap
    col = col / (1.0 + col);
    FragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
