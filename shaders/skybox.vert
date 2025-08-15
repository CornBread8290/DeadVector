#version 430 core

layout (location = 0) in vec3 a_position;

out vec3 v_direction;

uniform mat4 u_view;
uniform mat4 u_projection;

void main() {
    vec4 pos = u_projection * mat4(mat3(u_view)) * vec4(a_position, 1.0);
    gl_Position = pos.xyww;
    v_direction = a_position;
}
