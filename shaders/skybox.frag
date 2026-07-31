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
const float TIME           = 0.0;    // no app-driven time; keep static

const float NOISE_SCALE_LON = 4.0;

// Simple star frequencies
const float STAR_FREQ0   = 200.0;
const float STAR_FREQ1   = 140.0;

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

    // Safe basis
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

    // Core bulge (Gaussian in longitude)
    float c2    = max(CENTER_SIGMA*CENTER_SIGMA, 1e-5);
    float bulge = exp(-0.5 * (lon*lon) / c2);

    // Dust/texture along the band (unchanged)
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

    float density = 0.35 + 0.65 * (0.4*band + 0.6*bulge);

    vec2 starUV = vec2(lon * cos(lat), lat);
    float n0 = vnoise(starUV * STAR_FREQ0 + 7.1);
    float n1 = vnoise(starUV * STAR_FREQ1 - 3.7);
    float n  = max(n0, n1);

    // soft mask gives slightly larger, softer stars
    float starMask = smoothstep(0.96, 0.999, n);
    float starI = STAR_STRENGTH * starMask * density;

    float cSeed = hash11(floor(starUV.x*97.0) + 37.0*floor(starUV.y*59.0));
    vec3  starCol = starColor(cSeed);
    col += starCol * starI;

    // Sparse polar scatter (unchanged)
    float scatter = smoothstep(0.9975, 1.0, vnoise(vec2(dir.y, dir.x)*180.0 + 9.0));
    col += vec3(0.9,0.95,1.0) * SCATTER_STRENGTH * scatter * (1.0 - smoothstep(0.0, 1.0, band));

    FragColor = vec4(max(col, vec3(0.0)), 1.0);
}
