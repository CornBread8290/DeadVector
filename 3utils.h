#ifndef UTILS_H
#define UTILS_H

#include "defs.h"

void ensure_v(Mesh* m, int add);
void ensure_i(Mesh* m, int add);

Mask mask_all(const Mesh* m);
Mask mask_box(const Mesh* m, Vec3 mn, Vec3 mx);
Mask mask_sphere(const Mesh* m, Vec3 c, float r);
int  mask_count(const Mesh* m, const Mask s);

// transforms
void sel_translate(Mesh* m, Mask s, Vec3 d);
void sel_scale(Mesh* m, Mask s, Vec3 pivot, Vec3 k);
void sel_rotate(Mesh* m, const Mask s, Vec3 pivot, Vec3 axis, float ang);

// mirrors & extrusion
Mask sel_mirror(Mesh* m, const Mask s, Vec3 n, float d, char duplicate);
Mask sel_extrude_tris(Mesh* m, Mask s, Vec3 dir, float dist, int keep_base);

// submesh
int  assign_submesh_from_mask(Mesh* m, Mask s, unsigned material_id);

Mask mask_grow(const Mesh* m, const Mask in);
Mask mask_shrink_fulltri(const Mesh* m, const Mask in);

// mask tiny helpers
Mask mask_not(const Mesh* m, const Mask a);
Mask mask_and(const Mesh* m, const Mask a, const Mask b);
Mask mask_andnot(const Mesh* m, const Mask a, const Mask b);

// cockpit builder
void build_cockpit_interior(Mesh* m);

// simple whole-mesh transforms
void mesh_scale_all(Mesh* m, Vec3 k);
void mesh_translate_all(Mesh* m, Vec3 d);

// primitives
void mesh_clear(Mesh* m);
void make_box(Mesh* m, Vec3 mn, Vec3 mx);
void make_prism(Mesh* m, int sides, float r, float h);        // regular N-gon prism
void make_cubesphere(Mesh* m, Vec3 c, float r, int subdivs);
void make_icosahedron_sphere(Mesh* m, Vec3 c, float r);

uint32_t xorshift32(void);
float xorshift32f(void);
uint32_t wangHash(uint32_t seed);
float lerp(float a, float b, float t);
float smoothstep(float edge0, float edge1, float x);
float hashf3(Vec3 p);
float noise3f(Vec3 p);
float fbm(Vec3 p, int octaves, float persistence, float lacunarity);

Vec3 vec3_normalize(Vec3 v);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_cross(Vec3 a, Vec3 b);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 v, float s);
float vec3_dot(Vec3 a, Vec3 b);
Vec3 vec3_invert(Vec3 v);

Vec3 quat_rotate_vec3(Quat q, Vec3 v);
Quat quat_mul(Quat a, Quat b);
Quat quat_axis_angle(float x, float y, float z, float angle_rad);
Quat quat_normalize(Quat q);
Quat quat_conjugate(Quat q);
Quat quat_from_euler(float pitch, float yaw);

void mat4_perspective(float* out, float fovY_deg, float aspect, float zNear, float zFar);
void mat4_identity(Mat4 m);
void mat4_translate(Mat4 m, float x, float y, float z);
void mat4_scale(Mat4 m, float sx, float sy, float sz);
void mat4_multiply(Mat4 out, const Mat4 a, const Mat4 b);
void mat4_ortho(Mat4 out, float left, float right, float bottom, float top, float nearf, float farf);
void mat4_lookat(Mat4 m, Vec3 eye, Vec3 center, Vec3 up);
void quat_to_matrix(const Quat* q, Mat4 m);

void init_mesh(Mesh* mesh, int vertex_capacity, int index_capacity);
void draw_mesh(Mesh* mesh, GLenum primitive_type);
int find_or_add_vertex(Mesh* mesh, VertexFormat v);
int add_vertex(Mesh* mesh, VertexFormat v);
void add_triangle(Mesh* mesh, unsigned int i0, unsigned int i1, unsigned int i2);
void upload_mesh(Mesh* mesh);
void draw_object(const Object* obj, const ShaderProgram* shader, Mesh* mesh_pool[], GLenum primitive_type);

#endif // UTILS_H
