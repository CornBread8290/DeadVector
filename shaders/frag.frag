#version 330 core
out vec4 FragColor;

in vec3 v_normal;
in vec3 v_frag_pos;
in vec4 v_color;

uniform vec3 u_light_dir;  // direction *to* light
uniform vec3 u_view_pos;

float rand(vec3 co) {
    return fract(sin(dot(co, vec3(12.9898,78.233,54.53))) * 43758.5453);
}

void main() {
    // Normalize vectors
    vec3 normal = normalize(v_normal);
    vec3 lightDir = normalize(-u_light_dir); // because incoming light
    vec3 viewDir = normalize(u_view_pos - v_frag_pos);
    vec3 halfway = normalize(lightDir + viewDir);

    // Base lighting
    float diff = max(dot(normal, lightDir), 0.0);
    float spec = pow(max(dot(normal, halfway), 0.0), 32.0);

    // Hemisphere ambient: subtle top/bottom coloring
    vec3 up = vec3(0.0, 1.0, 0.0);
    float hemiFactor = dot(normal, up) * 0.5 + 0.5;
    vec3 skyAmbient = vec3(0.05);   // soft starlight
    vec3 groundAmbient = vec3(0.01); // dark space bounce
    vec3 ambient = mix(groundAmbient, skyAmbient, hemiFactor);
    ambient = vec3(0.05);

    // Rim lighting for edges
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = pow(rim, 3.0);
    vec3 rimLight = vec3(0.2) * rim;

    // Procedural dust
    float noise = rand(v_frag_pos * 3.5); // scale = dust density
    float dust = mix(0.9, 1.05, noise); // subtle bright flecks

    // Final base color
    vec3 albedo = v_color.rgb * dust;

    // Lighting components
    vec3 diffuse = albedo * diff;
    vec3 specular = vec3(0.3) * spec;

    vec3 final_color = ambient + diffuse + specular + rimLight;

    FragColor = vec4(final_color, v_color.a);
}
