#ifndef DEFS_H
#define DEFS_H

#include <math.h>
#include <windows.h>
#include <glad/gl.h>
#include <glad/wgl.h>

extern float aspectRatio;


typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int int16_t;

typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Vec4;
typedef Vec4 Quat; // for rotations
typedef struct { float u, v; } UV;
typedef struct { float r, g, b, a; } Color4;
typedef struct {
    Vec3 position;
    Vec3 normal;
    Vec4 tangent;       // Optional: for normal mapping
    Color4 color;
    UV uv;

    float roughness;
    float metallic;
    float emissive;

} VertexFormat;
typedef struct {
    Color4 albedo;
    float roughness;
    float metallic;
    float emissive;

    int has_texture_albedo;
    int has_texture_normal;
    int has_texture_metallic_roughness;
    int has_texture_emissive;
} Material;
typedef struct {
    unsigned int albedo;
    unsigned int normal;
    unsigned int metallic_roughness;
    unsigned int emissive;
} MaterialTextures;
typedef struct {
    int index_offset;
    int index_count;
    unsigned int material_id;
} SubMesh;
typedef struct {
    VertexFormat* vertices;
    unsigned int* indices;

    int vertex_count;
    int vertex_capacity;

    int index_count;
    int index_capacity;

    int material_id;

    unsigned int vao;
    unsigned int vbo;
    unsigned int ibo;

    SubMesh* submeshes;
    int submesh_count;

    unsigned int flags;
} Mesh;
typedef struct {
    Vec3 position;
    Quat rotation;
    Vec3 scale;

    int mesh_id;
    unsigned int flags; 
} Object;
typedef struct {
    Vec3 position;
    Vec3 target;
    Vec3 up;
    float fov;
    float zNear, zFar;
} Camera;
typedef float Mat4[16];
typedef struct {
    Vec3 position;
    Vec3 direction;
    float intensity;
    Color4 color;
} Light;
typedef struct {
    GLuint id; // GL shader program ID

    GLint u_model_loc;
    GLint u_view_loc;
    GLint u_projection_loc;

    GLint u_material_albedo_loc;
    GLint u_material_roughness_loc;
    GLint u_material_metallic_loc;
    GLint u_material_emissive_loc;
    GLint u_light_space_matrix_loc;

} ShaderProgram;
#define MAX_MATERIALS 256



#define MESH_HAS_NORMALS     (1 << 0)
#define MESH_HAS_UVS         (1 << 1)
#define MESH_HAS_COLORS      (1 << 2)
#define MESH_HAS_TANGENTS    (1 << 3)
#define MESH_HAS_BONE_WEIGHTS (1 << 4)


#define OBJ_FLAG_VISIBLE        (1 << 0)
#define OBJ_FLAG_STATIC         (1 << 1)
#define OBJ_FLAG_CAST_SHADOWS   (1 << 2)
#define OBJ_FLAG_RECEIVE_SHADOWS (1 << 3)

#define pi 3.14159265358979323846f
#define pi2 (pi * 2.0f)
#define DEG2RAD (3.14159265f / 180.0f)

#define NUM_STARS 80000


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
void upload_mesh(Mesh* mesh);
void draw_object(const Object* obj, ShaderProgram* shader, Mesh* mesh_pool[], GLenum primitive_type);

enum SoundType {
    SND_BEEP = 0,
    SND_ENGINE,
    SND_GUN,
    SND_HARSH
};


typedef struct {
    Vec3 normal_sum;
    int count;
} NormalAccumulator;


extern const Material* material_pool[MAX_MATERIALS];

void hud_setup_triangles(void);
void hud_setup_font(const char* ttf_path, float px);
void hud_draw(const char* text);

#endif // DEFS_H

