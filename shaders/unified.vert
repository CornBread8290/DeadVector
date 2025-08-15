#version 330 core
layout(location=0) in vec3 a_position;
layout(location=1) in vec3 a_normal;
layout(location=3) in vec4 a_color;

uniform mat4 u_model,u_view,u_projection;
uniform int u_is_skybox; // 0=mesh (planet/asteroid), 1=skybox

out vec3 v_normal;     // used by planet/asteroid FS
out vec3 v_frag_pos;   // used by asteroid FS
out vec4 v_color;      // used by asteroid FS
out vec3 v_position;   // used by planet FS
out vec3 v_direction;  // used by skybox FS

void main(){
    if(u_is_skybox==1){
        vec4 p=u_projection*mat4(mat3(u_view))*vec4(a_position,1);
        gl_Position=vec4(p.xy,p.w,p.w);
        v_direction=a_position;
        v_normal=vec3(0); v_frag_pos=vec3(0); v_position=vec3(0); v_color=vec4(1);
    }else{
        vec4 w=u_model*vec4(a_position,1);
        gl_Position=u_projection*u_view*w;
        v_frag_pos=w.xyz;
        v_position=w.xyz;
        v_normal=mat3(transpose(inverse(u_model)))*a_normal;
        v_color=a_color;
        v_direction=vec3(0);
    }
}
