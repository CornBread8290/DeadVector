#version 330 core
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;

uniform mat4 u_model, u_view, u_projection;

out vec3 v_position;
out vec3 v_normal;

void main() {
    vec4 worldPos = u_model * vec4(a_position, 1.0);
    v_position = worldPos.xyz;
    v_normal = mat3(transpose(inverse(u_model))) * a_normal;
    gl_Position = u_projection * u_view * worldPos;
}
