#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 3) in vec4 a_color;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec3 v_normal;
out vec3 v_frag_pos;
out vec4 v_color;

void main() {
    vec4 world_pos = u_model * vec4(a_position, 1.0);
    v_frag_pos = world_pos.xyz;

    v_normal = mat3(transpose(inverse(u_model))) * a_normal;

    v_color = a_color;

    gl_Position = u_projection * u_view * world_pos;
}
