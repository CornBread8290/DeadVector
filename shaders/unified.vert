#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec4 a_color;
layout(location = 3) in vec2 a_uv;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_light_space_matrix;
uniform int  u_is_skybox;

out vec3 v_normal;
out vec3 v_frag_pos;   // mesh: world-space position;        skybox: 0
out vec4 v_color;      // mesh: vertex color;                skybox: 1
out vec2 v_uv;         // mesh: texture coordinates;         skybox: 0
out vec4 v_light_space_pos;
out vec3 v_position;
out vec3 v_direction;  // skybox: direction;                 mesh: 0

void main() {
    if (u_is_skybox == 1) {
        vec4 p = u_projection * mat4(mat3(u_view)) * vec4(a_position, 1.0);
        gl_Position = vec4(p.xy, p.w, p.w);
        v_direction = a_position;

        v_normal   = vec3(0.0);
        v_frag_pos = vec3(0.0);
        v_position = vec3(0.0);
        v_color    = vec4(1.0);
        v_uv       = vec2(0.0);
        v_light_space_pos = vec4(0.0);
        return;
    }

    vec4 world = u_model * vec4(a_position, 1.0);
    gl_Position = u_projection * u_view * world;

    // world-space outputs for lighting
    v_frag_pos = world.xyz;
    v_position = world.xyz;
    v_light_space_pos = u_light_space_matrix * world;

    mat3 N = mat3(transpose(inverse(u_model)));
    v_normal = normalize(N * a_normal);

    v_color = a_color;
    v_uv = a_uv;
    v_direction = vec3(0.0);
}
