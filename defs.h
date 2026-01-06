#ifndef DEFS_H
#define DEFS_H

#include <math.h>
#include <windows.h>
#include <glad/gl.h>
#include <glad/wgl.h>
#include <stdint.h>

extern float aspectRatio;

#define pi 3.14159265358979323846f
#define pi2 (pi * 2.0f)
#define DEG2RAD (3.14159265f / 180.0f)

#define NUM_STARS 80000


typedef struct { float x, y; } Vec2;
typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Vec4;
typedef Vec4 Quat; // for rotations
typedef struct { float u, v; } UV;
typedef struct { float r, g, b, a; } Color4;
typedef struct {
    Vec3 position;
    Vec3 normal;
    Color4 color;
    UV uv;
} VertexFormat;
typedef struct {
    Color4 albedo;
    float roughness, metallic, emissiveF0;

    int tex_id;

    uint32_t flags;

    //TO BE DEPRECATED, DO NOT USE:
    int has_texture_albedo;
    int has_texture_normal;
    int has_texture_metallic_roughness;
    int has_texture_emissive;
} Material;
#define MATERIAL_FLAG_ALBEDO     (1 << 0)
#define MATERIAL_FLAG_NORMAL      (1 << 1)
#define MATERIAL_FLAG_EMISSIVEF0SWITCH (1 << 2) //Dialectric Specular
#define MATERIAL_IS_FLAT (1 << 5)

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
#define MESH_HAS_UVS         (1 << 1)
#define MESH_HAS_COLORS      (1 << 2)
#define MESH_FINISHED_BAKING (1 << 3)

typedef struct {
    Vec3 position;
    Quat rotation;
    Vec3 scale;

    int mesh_id;
    unsigned int flags; 
} Object;
#define OBJ_FLAG_VISIBLE        (1 << 0)
#define OBJ_FLAG_STATIC         (1 << 1)
#define OBJ_FLAG_CAST_SHADOWS   (1 << 2)
#define OBJ_FLAG_RECEIVE_SHADOWS (1 << 3)
typedef float Mat4[16];
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

    GLint u_flags_loc;

} ShaderProgram;
#define MAX_MATERIALS 256

// selections
typedef unsigned char* Mask;

typedef struct {
    Vec3 normal_sum;
    int count;
} NormalAccumulator;


extern const Material* material_pool[MAX_MATERIALS];

#endif // DEFS_H
