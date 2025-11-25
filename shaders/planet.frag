#version 430 core
out vec4 FragColor;

in vec3 v_position;
in vec3 v_normal;

uniform vec3 u_view_pos;
uniform vec3 u_light_dir;
uniform sampler2D u_flowTex; // rgba: RG=flow, B/A optional

const float PI = 3.14159265359;

float h2(vec2 p){ return fract(sin(dot(p, vec2(127.1,311.7))) * 43758.5453); }
float n2(vec2 p){
    vec2 i=floor(p), f=fract(p);
    float a=h2(i), b=h2(i+vec2(1,0)), c=h2(i+vec2(0,1)), d=h2(i+vec2(1,1));
    vec2 u=f*f*(3.0-2.0*f);
    return mix(a,b,u.x) + (c-a)*u.y*(1.0-u.x) + (d-b)*u.x*u.y;
}
float fbm(vec2 p){
    float t=0.0, a=0.5;
    for(int i=0;i<5;i++){ t+=n2(p)*a; p*=2.0; a*=0.5; }
    return t;
}
float ridged1D(float x){
    float t=0.0, a=0.6, f=1.5;
    for(int i=0;i<5;i++){
        float s = n2(vec2(x*f, 13.7+11.3*i));
        s = 1.0 - abs(2.0*s - 1.0);
        t += s*a; f*=1.9; a*=0.55;
    }
    return clamp(t*0.9,0.0,1.0);
}

vec2 toEquirect(vec3 n){
    float lon = atan(n.z, n.x);
    float lat = asin(clamp(n.y, -1.0, 1.0));
    return vec2(lon/(2.0*PI)+0.5, lat/PI+0.5);
}

float beltBaseAt(vec2 uv){
    float lat = (uv.y - 0.5);
    float jets  = ridged1D(lat*26.0); // was 18.0
    float jwarp = fbm(vec2(lat*24.0,7.3))*0.38 + fbm(vec2(lat*7.0,11.1))*0.28;
    float micro = fbm(vec2(lat*90.0,21.7))*0.12; // new: fine latitude wrinkles
    return clamp(jets*0.70 + jwarp*0.22 + micro*0.08, 0.0, 1.0);
}

float blurredBeltBase(vec2 uv){
    vec4 F = texture(u_flowTex, uv);
    vec2 flow = F.xy*2.0 - 1.0;
    vec2 dir = normalize(flow + vec2(1e-4, 0.0));
    vec2 orth = vec2(-dir.y, dir.x);

    vec2 texel = 1.0 / vec2(textureSize(u_flowTex, 0));

    float turb = (F.w > 0.0) ? F.w : fbm(uv*50.0);
    float sigma_s = mix(1.1, 2.2, turb); // spatial sigma in "tap units"
    float sigma_r = 0.08;                // range sigma (keeps edges crisp)

    float center = beltBaseAt(uv);
    float acc = 0.0, wsum = 0.0;

    for (int k = -3; k <= 3; ++k){
        float ks = float(k);
        vec2 o = dir * (ks * 2.0 * texel.x) + orth * (ks * 0.35 * texel.y);
        vec2 q = fract(uv + o);

        float s   = beltBaseAt(q);
        float g_s = exp(-0.5 * (ks*ks) / (sigma_s*sigma_s));
        float g_r = exp(-0.5 * pow((s - center)/sigma_r, 2.0));
        float w   = g_s * g_r;

        acc += s * w;
        wsum += w;
    }
    return acc / max(wsum, 1e-6);
}

vec3 ramp(float t){
    t = clamp(t,0.0,1.0);
    vec3 c0=vec3(0.94,0.92,0.88), c1=vec3(0.86,0.77,0.60),
         c2=vec3(0.72,0.54,0.33), c3=vec3(0.54,0.38,0.22), c4=vec3(0.80,0.68,0.54);
    if(t<0.25) return mix(c0,c1,smoothstep(0.00,0.25,t));
    if(t<0.55) return mix(c1,c2,smoothstep(0.25,0.55,t));
    if(t<0.85) return mix(c2,c3,smoothstep(0.55,0.85,t));
    return mix(c3,c4,smoothstep(0.85,1.00,t));
}

void main(){
    vec3 N = normalize(v_normal);
    vec3 V = normalize(u_view_pos - v_position);
    vec3 L = normalize(-u_light_dir);

    vec2 uv = toEquirect(normalize(v_position));
    vec2 uvw = uv;
    for(int i=0;i<3;i++){
        vec2 flow = texture(u_flowTex, uvw).xy*2.0 - 1.0;
        float step = mix(0.010, 0.022, float(i)/2.0);
        uvw = fract(uvw + flow * step);
    }

    float base_blur = blurredBeltBase(uvw);

    float belts = smoothstep(0.40, 0.60, base_blur);
    float filigree = smoothstep(0.48,0.52, fract(base_blur*9.0 + fbm(uvw*40.0)*1.5));
    belts = mix(belts, belts*0.88 + filigree*0.12, 0.50);

    vec3 albedo = ramp(belts);

    vec2 c = vec2(0.32, 0.43), d = uvw - c;
    if(d.x>0.5) d.x-=1.0; if(d.x<-0.5) d.x+=1.0;
    mat2 R = mat2(0.96,-0.28, 0.28,0.96); d = R*d;
    vec2 s = d*vec2(1.4,0.6);
    float r2 = dot(s,s);
    float ring = clamp(exp(-r2*18.0) - exp(-r2*130.0), 0.0, 1.0);
    float core = exp(-r2*280.0);
    vec3 ringColor = vec3(0.86,0.56,0.28), eyeColor = vec3(0.95,0.91,0.86);
    albedo = mix(albedo, ringColor, ring*0.90);
    albedo = mix(albedo, eyeColor,  core*0.70);
    albedo += (atan(d.y,d.x)/(2.0*PI)) * 0.03 * ring;

    float k = dot(N,L);
    float ndl = max(k, 0.0);
    float nv  = clamp(dot(N,V), 0.0, 1.0);

    float haze = exp(-3.0*(1.0-ndl));
    float day  = ndl * (0.7 + 0.3*haze); // no wrap diffuse

    float g=0.6, mu=dot(V,-L);
    float phase = (1.0-g*g) / pow(1.0+g*g-2.0*g*mu, 1.5);
    float rimV = pow(1.0 - nv, 3.0);
    float dayRim   = smoothstep(0.0, 0.12, ndl) * rimV * 0.35;
    float nightCrs = smoothstep(-0.18, -0.02, k) * pow(1.0 - nv, 4.0) * 0.18;

    vec3 col = albedo * day + albedo * phase * (dayRim + nightCrs);
    col += albedo * 0.005; // tiny ambient only

    FragColor = vec4(pow(col, vec3(1.0/2.2)), 1.0);
}
