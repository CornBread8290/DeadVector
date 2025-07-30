#ifndef DEFS_H
#define DEFS_H

#include <math.h>
#include <windows.h>
#include <gl/gl.h>

extern float aspectRatio;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef short int int16_t;

typedef struct {
    float x, y;
} Vec2;
typedef union {
    struct { float x, y, z; };
    float data[3];
} Vec3;
typedef Vec3 Normal;
typedef Vec3 Vertex;
typedef struct { float x, y, z, w; } Quat;
typedef union {
    struct { float r, g, b; };
    float data[3];
} Color3;
typedef union {
    struct { float r, g, b, a; };
    float data[4];
} Color4;
typedef struct { float u, v; } UV;
typedef struct {
    Vertex* vertices;
    Normal* normals;
    Color4* colors;
    UV* uvs;

    int vertex_count;
    int vertex_capacity;

    unsigned int flags;
} Mesh;

typedef struct {
    Vec3 position;
    Quat rotation;
    Vec3 scale;
    Mesh mesh;
    unsigned int flags;
    void** components;
} Object;

#define pi 3.14159265358979323846f
#define pi2 (pi * 2.0f)
#define DEG2RAD (3.14159265f / 180.0f)

#define NUM_STARS 80000


enum SoundType {
    SND_BEEP = 0,
    SND_ENGINE,
    SND_GUN,
    SND_HARSH
};


uint32_t xorshift32(void);
float xorshift32f(void);
float lerp(float a, float b, float t);
float smoothstep(float edge0, float edge1, float x);
float hashf3(Vec3 p);
float noise3f(Vec3 p);
void playSoundEffect(int type, float pitch, float volume, float pan, int channel);
void stopSound(int channel);
float fbm(Vec3 p, int octaves, float persistence, float lacunarity);
Quat quat_mul(Quat a, Quat b);
Quat quat_axis_angle(float x, float y, float z, float angle_rad);
Quat quat_normalize(Quat q);
void quat_to_matrix(const Quat* q, float* m);
Vec3 quat_rotate_vec3(Quat q, Vec3 v);
Quat quat_conjugate(Quat q);
void SetProjectionMatrix(float zNear, float zFar);

Vec3 direction_between(Vec3 a, Vec3 b);
void* normalize(void* v);
void* multiply3f(void* v, float scalar);

void draw_mesh(const Mesh* mesh, GLenum primitive_type);
void init_mesh(Mesh* mesh, int initial_capacity);
void add_vertex(Mesh* mesh, Vec3 pos, Normal normal, Color4 color, UV uv);
void add_triangle(Mesh* mesh, Vec3 a, Vec3 b, Vec3 c, Color4 color);
void add_cube(Mesh* mesh, Vec3 origin, float size, Color4 color);
void extrude_last_triangle(Mesh* mesh, float distance, Color4 color);
void set_mesh_color(Mesh* mesh, Color4 color);
void scale_mesh(Mesh* mesh, float scale);
void perturb_vertices(Mesh* mesh, float strength, float frequency);

void debug_mesh(const Mesh* mesh);
void draw_object(const Object* obj, GLenum primitive_type);
void debug_object(const Object* obj);

typedef struct {
    Vec3 normal_sum;
    int count;
} NormalAccumulator;
#endif // DEFS_H
