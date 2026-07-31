#version 430 core

layout(location = 0) uniform mat4 u_view;
layout(location = 4) uniform mat4 u_proj;
layout(location = 8) uniform vec4 u_centre;

out vec3 v_world;

void main() {
    vec2 c = vec2(gl_VertexID & 1, (gl_VertexID >> 1) & 1) * 2.0 - 1.0;

    vec3 right = vec3(u_view[0][0], u_view[1][0], u_view[2][0]);
    vec3 up    = vec3(u_view[0][1], u_view[1][1], u_view[2][1]);

    v_world = u_centre.xyz + (right * c.x + up * c.y) * u_centre.w;
    gl_Position = u_proj * u_view * vec4(v_world, 1.0);
}
