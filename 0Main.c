#define WIN32_LEAN_AND_MEAN
#include "defs.h"
#include "3utils.h"
#include "2Audio.h"
#include "4Font.h"
#include "5Speech.h"
#include "6Volume.h"
#include "7Physics.h"
#include "8Mesh.h"
#include "9Meshes.h"
#include "DEBUG_Freecam.h"

#include <stdbool.h>
#include <stdio.h>
#include <glad/gl.h>
#include <glad/wgl.h>
#include <stdint.h>

HGLRC hRC;
HDC hDC;   // Device Context
HWND hWnd; // Window Handle
static int win_w = 800, win_h = 600;

#ifdef EMBED_SHADERS
#include <string.h>
#include "shaders_embed.h"
#endif

static void shader_log(const char* what, const char* who, const char* detail) {
    FILE* f = fopen("shader_error.txt", "a");
    if (!f) return;
    fprintf(f, "%s %s\n%s\n", what, who, detail);
    fclose(f);
}

GLuint compile_shader_from_file(const char* filepath, GLenum shader_type) {
#ifdef EMBED_SHADERS
    const char* source = embedded_shader_source(filepath);
    if (!source) return 0;
#else
    FILE* file = fopen(filepath, "rb");

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char* source = (char*)malloc((size_t)length + 1);

    fread(source, 1, (size_t)length, file);
    source[length] = '\0';
    fclose(file);
#endif

    const GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, (const char* const*)&source, NULL);
    glCompileShader(shader);
#ifndef EMBED_SHADERS
    free(source);
#endif

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 1) { char* log = (char*)malloc(logLen); glGetShaderInfoLog(shader, logLen, NULL, log); fprintf(stderr, "SHADER COMPILE FAIL %s:\n%s\n", filepath, log); shader_log("SHADER COMPILE FAIL", filepath, log); free(log); }
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

ShaderProgram create_shader_program_from_files(const char* vert_path, const char* frag_path) {
    static const char* light_pos_name = "u_light_pos_type[0]";
    static const char* light_dir_name = "u_light_dir_inner[0]";
    static const char* light_color_name = "u_light_color_outer[0]";
    static const char* light_param_name = "u_light_params[0]";
    GLuint vs = compile_shader_from_file(vert_path, GL_VERTEX_SHADER);
    GLuint fs = compile_shader_from_file(frag_path, GL_FRAGMENT_SHADER);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint logLen = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 1) { char* log = (char*)malloc(logLen); glGetProgramInfoLog(prog, logLen, NULL, log); fprintf(stderr, "PROGRAM LINK FAIL %s + %s:\n%s\n", vert_path, frag_path, log); shader_log("PROGRAM LINK FAIL", frag_path, log); free(log); }
    }

    glDeleteShader(vs);
    glDeleteShader(fs);

    ShaderProgram sp = {0};
    sp.id = prog;
    sp.u_model_loc = glGetUniformLocation(prog, "u_model");
    sp.u_view_loc = glGetUniformLocation(prog, "u_view");
    sp.u_projection_loc = glGetUniformLocation(prog, "u_projection");
    sp.u_light_space_matrix_loc = glGetUniformLocation(prog, "u_light_space_matrix");
    sp.u_material_albedo_loc = glGetUniformLocation(prog, "u_material_albedo");
    sp.u_material_roughness_loc = glGetUniformLocation(prog, "u_material_roughness");
    sp.u_material_metallic_loc = glGetUniformLocation(prog, "u_material_metallic");
    sp.u_material_emissive_loc = glGetUniformLocation(prog, "u_material_emissive");
    sp.u_material_flags_loc = glGetUniformLocation(prog, "u_material_flags");
    sp.u_mesh_flags_loc = glGetUniformLocation(prog, "u_mesh_flags");
    sp.u_albedo_tex_loc = glGetUniformLocation(prog, "u_albedo_tex");
    sp.u_view_pos_loc = glGetUniformLocation(prog, "u_view_pos");
    sp.u_light_dir_loc = glGetUniformLocation(prog, "u_light_dir");
    sp.u_is_skybox_loc = glGetUniformLocation(prog, "u_is_skybox");
    sp.u_light_count_loc = glGetUniformLocation(prog, "u_light_count");
    sp.u_shadow_map_loc = glGetUniformLocation(prog, "u_shadow_map");
    sp.u_reflection_tex_loc = glGetUniformLocation(prog, "u_reflection_tex");
    sp.u_refraction_tex_loc = glGetUniformLocation(prog, "u_refraction_tex");
    sp.u_reflection_view_proj_loc = glGetUniformLocation(prog, "u_reflection_view_proj");
    sp.u_screen_size_loc = glGetUniformLocation(prog, "u_screen_size");
    sp.u_render_features_loc = glGetUniformLocation(prog, "u_render_features");
    sp.u_scene_tex_loc = glGetUniformLocation(prog, "u_scene_tex");
    sp.u_flow_tex_loc = glGetUniformLocation(prog, "u_flowTex");
    sp.u_light_pos_type_loc = glGetUniformLocation(prog, light_pos_name);
    sp.u_light_dir_inner_loc = glGetUniformLocation(prog, light_dir_name);
    sp.u_light_color_outer_loc = glGetUniformLocation(prog, light_color_name);
    sp.u_light_params_loc = glGetUniformLocation(prog, light_param_name);
    sp.u_time_loc = glGetUniformLocation(prog, "u_time");

    return sp;
}


// OpenGL state
GLuint skybox_vao, skybox_vbo, skybox_ibo, fullscreen_vao;
Mat4 light_space_matrix;
GLuint shadow_map_tex;

enum { SHADOW_MAP_SIZE = 2048 };
enum { LIGHT_DIRECTIONAL = 0, LIGHT_POINT = 1, LIGHT_SPOT = 2 };
enum { RENDER_FEATURE_REFLECTION = 1 << 0, RENDER_FEATURE_REFRACTION = 1 << 1 };
enum {
    RENDER_WORLD_CLEAR       = 1 << 0,
    RENDER_WORLD_SKYBOX      = 1 << 1,
    RENDER_WORLD_OPAQUE      = 1 << 2,
    RENDER_WORLD_PLANET      = 1 << 3,
    RENDER_WORLD_TRANSPARENT = 1 << 4,
    RENDER_WORLD_VOLUME      = 1 << 5
};

typedef struct {
    int type;
    Vec3 position;
    Vec3 direction;
    Vec3 color;
    float intensity;
    float range;
    float inner_cos;
    float outer_cos;
    int casts_shadow;
} SceneLight;

typedef struct {
    GLuint fbo;
    GLuint color_tex;
    GLuint depth_rb;
    int width;
    int height;
    GLenum color_format;
    unsigned char has_depth;
    unsigned char mipmapped;
} RenderTarget;

typedef struct {
    const ShaderProgram* forward_shader;
    const ShaderProgram* skybox_shader;
    const ShaderProgram* planet_shader;
    const Object* const* objects;
    int object_count;
    Mesh** mesh_pool;
    const Object* gas_planet;
    GLuint planet_flow_tex;
    GLenum primitive_type;
    const Volume* volumes;
    int volume_count;
    const Ring* ring;
} RenderScene;

typedef struct {
    const float* view;
    const float* projection;
    Vec3 camera_pos;
    Vec3 light_dir;
    const SceneLight* lights;
    int light_count;
    GLuint target_fbo;
    int viewport_w;
    int viewport_h;
    const float* reflection_matrix;
    GLuint reflection_tex;
    GLuint refraction_tex;
    int render_features;
    unsigned int object_skip_flags;
    int pass_flags;
    float time;
} RenderView;
    Mesh spaceship1;

float aspectRatio = 8.0f/6.0f;

const Material* material_pool[MAX_MATERIALS];
GLuint texture_pool[MAX_TEXTURES];
Mesh* mesh_pool[256];
Vec3 pos = {0};      // Ship world position
Vec3 vel = {0};      // Ship linear velocity
Vec3 angVel = {0};   // Ship angular velocity (radians/sec)
Quat rot = {0, 0, 0, 1}; // Ship orientation
Vec2 view = {0, 0};  // Head pitch/yaw in radians
static int u32_to_str(char *out, unsigned v) {
    char tmp[10];
    int n = 0;
    do { tmp[n++] = (char)('0' + (v % 10)); v /= 10; } while (v);
    for (int i = 0; i < n; i++) out[i] = tmp[n - 1 - i];
    out[n] = 0;
    return n;
}

void calculate_smooth_normals(Mesh* mesh) {
    if (!mesh || mesh->index_count % 3 != 0) return;

    // Accumulate face normals
    for (int i = 0; i < mesh->index_count; i += 3) {
        int ia = mesh->indices[i];
        int ib = mesh->indices[i+1];
        int ic = mesh->indices[i+2];

        Vec3 a = mesh->vertices[ia].position;
        Vec3 b = mesh->vertices[ib].position;
        Vec3 c = mesh->vertices[ic].position;

        Vec3 u = vec3_sub(b, a);
        Vec3 v = vec3_sub(c, a);
        Vec3 n = vec3_cross(u, v);
        n = vec3_normalize(n);

        mesh->vertices[ia].normal = vec3_add(mesh->vertices[ia].normal, n);
        mesh->vertices[ib].normal = vec3_add(mesh->vertices[ib].normal, n);
        mesh->vertices[ic].normal = vec3_add(mesh->vertices[ic].normal, n);
    }
    // Normalize all vertex normals
    for (int i = 0; i < mesh->vertex_count; i++) {
        mesh->vertices[i].normal = vec3_normalize(mesh->vertices[i].normal);
    }
}

static void cubesphere_face_basis(int f, Vec3* normal, Vec3* axis_a, Vec3* axis_b, int* flip)
{
    switch (f) {
        default:
        case 0: *normal = (Vec3){ 1, 0, 0 }; break;
        case 1: *normal = (Vec3){-1, 0, 0 }; break;
        case 2: *normal = (Vec3){ 0, 1, 0 }; break;
        case 3: *normal = (Vec3){ 0,-1, 0 }; break;
        case 4: *normal = (Vec3){ 0, 0, 1 }; break;
        case 5: *normal = (Vec3){ 0, 0,-1 }; break;
    }

    *axis_a = (Vec3){0,0,0};
    *axis_b = (Vec3){0,0,0};

    if (fabsf(normal->x) > 0.5f) { axis_a->z = 1; axis_b->y = 1; }
    else if (fabsf(normal->y) > 0.5f) { axis_a->x = 1; axis_b->z = 1; }
    else { axis_a->x = 1; axis_b->y = 1; }

    *flip = (f == 0 || f == 2 || f == 5);
}

static void generate_cubesphere(Mesh* mesh, Vec3 center, float radius, int subdivisions, int rocky)
{
    mesh->material_id = 1;
    if (subdivisions < 1) subdivisions = 1;

    int verts_per_face = (subdivisions + 1) * (subdivisions + 1);
    init_mesh(mesh, verts_per_face * 6, subdivisions * subdivisions * 36);

    float step = 2.0f / subdivisions;

    for (int f = 0; f < 6; f++) {
        Vec3 normal, axis_a, axis_b;
        int flip;
        cubesphere_face_basis(f, &normal, &axis_a, &axis_b, &flip);

        int vert_idx_grid[subdivisions + 1][subdivisions + 1];

        for (int i = 0; i <= subdivisions; i++) {
            for (int j = 0; j <= subdivisions; j++) {
                float x = -1.0f + step * i;
                float y = -1.0f + step * j;

                Vec3 p = {
                    normal.x + axis_a.x * x + axis_b.x * y,
                    normal.y + axis_a.y * x + axis_b.y * y,
                    normal.z + axis_a.z * x + axis_b.z * y
                };
                Vec3 dir = vec3_normalize(p);

                float deform = 1.0f;
                Color4 color = {1,1,1,1};
                if (rocky) {
                    float base_shape, deform2;
                    deform = asteroid_shape(dir, &base_shape, &deform2);

                    float base_gray  = 0.33f + base_shape * 0.08f;
                    float brown_tint = (fbm(vec3_scale(dir, 1.3f), 2, 0.5f, 2.0f) - 0.5f) * 0.12f;
                    float blue_tint  = (1.0f - deform2) * fbm(vec3_scale(dir, 1.8f), 2, 0.5f, 2.0f) * 0.12f;
                    color = (Color4){
                        .r = fminf(fmaxf(base_gray + brown_tint * 0.6f - blue_tint * 0.2f, 0.0f), 1.0f),
                        .g = fminf(fmaxf(base_gray + brown_tint * 0.4f - blue_tint * 0.1f, 0.0f), 1.0f),
                        .b = fminf(fmaxf(base_gray + blue_tint, 0.0f), 1.0f),
                        .a = 1.0f
                    };
                }

                VertexFormat vert = {
                    .position = vec3_add(center, vec3_scale(dir, radius * deform)),
                    .normal   = rocky ? (Vec3){0,0,0} : dir,
                    .color    = color,
                    .uv       = (UV){0,0}
                };
                vert_idx_grid[i][j] = find_or_add_vertex(mesh, vert);
            }
        }

        for (int i = 0; i < subdivisions; i++) {
            for (int j = 0; j < subdivisions; j++) {
                int a = vert_idx_grid[i][j];
                int b = vert_idx_grid[i+1][j];
                int c = vert_idx_grid[i][j+1];
                int d = vert_idx_grid[i+1][j+1];
                if (flip) { add_triangle(mesh, a, c, b); add_triangle(mesh, c, d, b); }
                else      { add_triangle(mesh, a, b, c); add_triangle(mesh, c, b, d); }
            }
        }
    }
}


void set_common_uniforms(const ShaderProgram* s,
                         const float* view,
                         const float* proj,
                         Vec3 cam_pos,
                         Vec3 light_dir,
                         const SceneLight* lights,
                         int light_count,
                         const float* reflection_matrix,
                         GLuint reflection_tex,
                         GLuint refraction_tex,
                         Vec2 render_size,
                         int render_features)
{
    float light_pos_type[MAX_FORWARD_LIGHTS * 4] = {0};
    float light_dir_inner[MAX_FORWARD_LIGHTS * 4] = {0};
    float light_color_outer[MAX_FORWARD_LIGHTS * 4] = {0};
    float light_params[MAX_FORWARD_LIGHTS * 4] = {0};
    int count = light_count > MAX_FORWARD_LIGHTS ? MAX_FORWARD_LIGHTS : light_count;
    glUseProgram(s->id);

    if (s->u_view_loc        != -1) glUniformMatrix4fv(s->u_view_loc,        1, GL_FALSE, view);
    if (s->u_projection_loc  != -1) glUniformMatrix4fv(s->u_projection_loc,  1, GL_FALSE, proj);
    if (s->u_light_space_matrix_loc != -1) glUniformMatrix4fv(s->u_light_space_matrix_loc, 1, GL_FALSE, light_space_matrix);

    if (s->u_view_pos_loc != -1) glUniform3f(s->u_view_pos_loc, cam_pos.x, cam_pos.y, cam_pos.z);
    if (s->u_light_dir_loc != -1) glUniform3f(s->u_light_dir_loc, light_dir.x, light_dir.y, light_dir.z);
    if (s->u_is_skybox_loc != -1) glUniform1i(s->u_is_skybox_loc, 0);
    if (s->u_light_count_loc != -1) glUniform1i(s->u_light_count_loc, count);
    if (s->u_render_features_loc != -1) glUniform1i(s->u_render_features_loc, render_features);
    if (s->u_screen_size_loc != -1) glUniform2f(s->u_screen_size_loc, render_size.x, render_size.y);
    if (s->u_reflection_view_proj_loc != -1 && reflection_matrix) {
        glUniformMatrix4fv(s->u_reflection_view_proj_loc, 1, GL_FALSE, reflection_matrix);
    }
    if (s->u_shadow_map_loc != -1) {
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadow_map_tex);
        glUniform1i(s->u_shadow_map_loc, 1);
    }
    if (s->u_reflection_tex_loc != -1) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, (render_features & RENDER_FEATURE_REFLECTION) ? reflection_tex : 0);
        glUniform1i(s->u_reflection_tex_loc, 2);
    }
    if (s->u_refraction_tex_loc != -1) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, (render_features & RENDER_FEATURE_REFRACTION) ? refraction_tex : 0);
        glUniform1i(s->u_refraction_tex_loc, 3);
    }
    glActiveTexture(GL_TEXTURE0);

    for (int i = 0; i < MAX_FORWARD_LIGHTS; ++i) {
        const SceneLight zero = {0};
        const SceneLight* l = (lights && i < light_count) ? &lights[i] : &zero;

        light_pos_type[i * 4 + 0] = l->position.x;
        light_pos_type[i * 4 + 1] = l->position.y;
        light_pos_type[i * 4 + 2] = l->position.z;
        light_pos_type[i * 4 + 3] = (float)l->type;

        light_dir_inner[i * 4 + 0] = l->direction.x;
        light_dir_inner[i * 4 + 1] = l->direction.y;
        light_dir_inner[i * 4 + 2] = l->direction.z;
        light_dir_inner[i * 4 + 3] = l->inner_cos;

        light_color_outer[i * 4 + 0] = l->color.x;
        light_color_outer[i * 4 + 1] = l->color.y;
        light_color_outer[i * 4 + 2] = l->color.z;
        light_color_outer[i * 4 + 3] = l->outer_cos;

        light_params[i * 4 + 0] = l->range;
        light_params[i * 4 + 1] = l->intensity;
        light_params[i * 4 + 2] = (float)l->casts_shadow;
    }

    if (s->u_light_pos_type_loc != -1) glUniform4fv(s->u_light_pos_type_loc, MAX_FORWARD_LIGHTS, light_pos_type);
    if (s->u_light_dir_inner_loc != -1) glUniform4fv(s->u_light_dir_inner_loc, MAX_FORWARD_LIGHTS, light_dir_inner);
    if (s->u_light_color_outer_loc != -1) glUniform4fv(s->u_light_color_outer_loc, MAX_FORWARD_LIGHTS, light_color_outer);
    if (s->u_light_params_loc != -1) glUniform4fv(s->u_light_params_loc, MAX_FORWARD_LIGHTS, light_params);
}

typedef struct {
    const Object* obj;
    float dist2;
} RenderItem;

static float object_distance_sq(const Object* obj, Vec3 camera_pos){
    const float dx = obj->position.x - camera_pos.x;
    const float dy = obj->position.y - camera_pos.y;
    const float dz = obj->position.z - camera_pos.z;
    return dx*dx + dy*dy + dz*dz;
}

static void sort_render_items(RenderItem* items, int count){
    for(int i = 1; i < count; ++i){
        RenderItem key = items[i];
        int j = i - 1;
        while(j >= 0 && items[j].dist2 < key.dist2){
            items[j + 1] = items[j];
            --j;
        }
        items[j + 1] = key;
    }
}

static void render_forward_opaque_objects(const Object* const* objects,
                                          int count,
                                          const ShaderProgram* shader,
                                          Mesh* pool[],
                                          GLenum prim,
                                          unsigned int skip_flags)
{
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    for(int i = 0; i < count; ++i){
        const Object* obj = objects[i];
        if(obj->flags & skip_flags) continue;
        if(object_has_opaque_parts(obj, pool)){
            draw_object_pass(obj, shader, pool, prim, 0);
        }
    }
}

static void render_forward_transparent_objects(const Object* const* objects,
                                               int count,
                                               const ShaderProgram* shader,
                                               Mesh* pool[],
                                               GLenum prim,
                                               Vec3 camera_pos,
                                               unsigned int skip_flags)
{
    RenderItem transparent[64];
    int transparent_count = 0;

    for(int i = 0; i < count; ++i){
        const Object* obj = objects[i];
        if(obj->flags & skip_flags) continue;
        if(object_has_transparent_parts(obj, pool) && transparent_count < (int)(sizeof transparent / sizeof transparent[0])){
            transparent[transparent_count++] = (RenderItem){
                .obj = obj,
                .dist2 = object_distance_sq(obj, camera_pos)
            };
        }
    }

    if(!transparent_count) return;

    sort_render_items(transparent, transparent_count);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    for(int i = 0; i < transparent_count; ++i){
        draw_object_pass(transparent[i].obj, shader, pool, prim, 1);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

static void render_shadow_casters(const Object* const* objects,
                                  int count,
                                  const ShaderProgram* shader,
                                  Mesh* pool[],
                                  GLenum prim)
{
    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    for(int i = 0; i < count; ++i){
        const Object* obj = objects[i];
        if(!(obj->flags & OBJ_FLAG_CAST_SHADOWS)) continue;
        if(!object_has_opaque_parts(obj, pool)) continue;
        draw_object_pass(obj, shader, pool, prim, 0);
    }
}

static void build_directional_light_matrix(Mat4 out, Vec3 light_dir, Vec3 focus){
    Mat4 light_view, light_proj;
    Vec3 eye = {
        focus.x - light_dir.x * 42.0f,
        focus.y - light_dir.y * 42.0f,
        focus.z - light_dir.z * 42.0f
    };
    Vec3 up = fabsf(light_dir.y) > 0.92f ? (Vec3){0.0f, 0.0f, 1.0f} : (Vec3){0.0f, 1.0f, 0.0f};

    mat4_lookat(light_view, eye, focus, up);
    mat4_ortho(light_proj, -34.0f, 34.0f, -34.0f, 34.0f, 1.0f, 96.0f);
    mat4_multiply(out, light_proj, light_view);
}

static GLenum render_target_data_type(GLenum color_format){
    return color_format == GL_RGBA8 ? GL_UNSIGNED_BYTE : GL_FLOAT;
}

static void ensure_render_target(RenderTarget* rt,
                                 int width,
                                 int height,
                                 GLenum color_format,
                                 unsigned char has_depth,
                                 unsigned char mipmapped)
{
    if(width < 1) width = 1;
    if(height < 1) height = 1;

    if(rt->fbo &&
       rt->width == width &&
       rt->height == height &&
       rt->color_format == color_format &&
       rt->has_depth == has_depth &&
       rt->mipmapped == mipmapped){
        return;
    }

    if(!rt->fbo) glGenFramebuffers(1, &rt->fbo);
    if(!rt->color_tex) glGenTextures(1, &rt->color_tex);
    if(has_depth && !rt->depth_rb) glGenRenderbuffers(1, &rt->depth_rb);

    rt->width = width;
    rt->height = height;
    rt->color_format = color_format;
    rt->has_depth = has_depth;
    rt->mipmapped = mipmapped;

    glBindTexture(GL_TEXTURE_2D, rt->color_tex);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 color_format,
                 width,
                 height,
                 0,
                 GL_RGBA,
                 render_target_data_type(color_format),
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmapped ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    if(mipmapped) glGenerateMipmap(GL_TEXTURE_2D);

    glBindFramebuffer(GL_FRAMEBUFFER, rt->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt->color_tex, 0);

    if(has_depth){
        glBindRenderbuffer(GL_RENDERBUFFER, rt->depth_rb);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->depth_rb);
    } else {
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    }

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
        fprintf(stderr, "FRAMEBUFFER INCOMPLETE %dx%d\n", width, height);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

static void render_skybox_pass(const ShaderProgram* skybox_shader, const Mat4 view, const Mat4 projection){
    Mat4 view_no_translate;
    memcpy(view_no_translate, view, sizeof(Mat4));
    view_no_translate[12] = view_no_translate[13] = view_no_translate[14] = 0.0f;

    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glUseProgram(skybox_shader->id);
    if (skybox_shader->u_is_skybox_loc != -1) glUniform1i(skybox_shader->u_is_skybox_loc, 1);
    glUniformMatrix4fv(skybox_shader->u_view_loc, 1, GL_FALSE, view_no_translate);
    glUniformMatrix4fv(skybox_shader->u_projection_loc, 1, GL_FALSE, projection);
    glBindVertexArray(skybox_vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}

static void render_planet_pass(const ShaderProgram* planet_shader,
                               const Object* gas_planet,
                               Mesh* pool[],
                               GLuint planet_flow_tex,
                               const Mat4 view,
                               const Mat4 projection,
                               Vec3 cam_pos,
                               Vec3 light_dir,
                               const Ring* ring)
{
    glUseProgram(planet_shader->id);
    if (ring) {
        glUniform4f(40, ring->centre.x, ring->centre.y, ring->centre.z, ring->planet_r);
        glUniform4f(41, ring->axis.x, ring->axis.y, ring->axis.z, 0.0f);
        glUniform4f(42, ring->inner_r, ring->outer_r, ring->tau, 0.0f);
    } else {
        glUniform4f(42, 0.0f, 0.0f, 0.0f, 0.0f);
    }
    if (planet_shader->u_is_skybox_loc != -1) glUniform1i(planet_shader->u_is_skybox_loc, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planet_flow_tex);
    if (planet_shader->u_flow_tex_loc != -1) glUniform1i(planet_shader->u_flow_tex_loc, 0);
    glUniformMatrix4fv(planet_shader->u_view_loc, 1, GL_FALSE, view);
    glUniformMatrix4fv(planet_shader->u_projection_loc, 1, GL_FALSE, projection);
    glUniformMatrix4fv(planet_shader->u_light_space_matrix_loc, 1, GL_FALSE, light_space_matrix);
    if (planet_shader->u_light_dir_loc != -1) glUniform3f(planet_shader->u_light_dir_loc, light_dir.x, light_dir.y, light_dir.z);
    if (planet_shader->u_view_pos_loc != -1) glUniform3f(planet_shader->u_view_pos_loc, cam_pos.x, cam_pos.y, cam_pos.z);
    draw_object(gas_planet, planet_shader, pool, GL_TRIANGLES);
}

static void render_post_process(const ShaderProgram* post_shader, GLuint scene_tex){
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, win_w, win_h);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(post_shader->id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_tex);
    if (post_shader->u_scene_tex_loc != -1) glUniform1i(post_shader->u_scene_tex_loc, 0);
    if (post_shader->u_screen_size_loc != -1) glUniform2f(post_shader->u_screen_size_loc, (float)win_w, (float)win_h);
    glBindVertexArray(fullscreen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

static void render_monitor_post_process(const ShaderProgram* monitor_post_shader,
                                        GLuint scene_tex,
                                        const RenderTarget* dst,
                                        float time)
{
    glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo);
    glViewport(0, 0, dst->width, dst->height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glUseProgram(monitor_post_shader->id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, scene_tex);
    if (monitor_post_shader->u_scene_tex_loc != -1) glUniform1i(monitor_post_shader->u_scene_tex_loc, 0);
    if (monitor_post_shader->u_screen_size_loc != -1) glUniform2f(monitor_post_shader->u_screen_size_loc, (float)dst->width, (float)dst->height);
    if (monitor_post_shader->u_time_loc != -1) glUniform1f(monitor_post_shader->u_time_loc, time);
    glBindVertexArray(fullscreen_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
}

static void render_world_view(const RenderScene* scene, const RenderView* view)
{
    if(!scene || !view) return;

    glBindFramebuffer(GL_FRAMEBUFFER, view->target_fbo);
    glViewport(0, 0, view->viewport_w, view->viewport_h);
    glEnable(GL_DEPTH_TEST);

    if(view->pass_flags & RENDER_WORLD_CLEAR){
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    if((view->pass_flags & RENDER_WORLD_SKYBOX) && scene->skybox_shader){
        render_skybox_pass(scene->skybox_shader, view->view, view->projection);
    }

    if((view->pass_flags & (RENDER_WORLD_OPAQUE | RENDER_WORLD_TRANSPARENT)) && scene->forward_shader){
        set_common_uniforms(scene->forward_shader,
                            view->view,
                            view->projection,
                            view->camera_pos,
                            view->light_dir,
                            view->lights,
                            view->light_count,
                            view->reflection_matrix,
                            view->reflection_tex,
                            view->refraction_tex,
                            (Vec2){(float)view->viewport_w, (float)view->viewport_h},
                            view->render_features);
        glPointSize(1.0f);
    }

    if((view->pass_flags & RENDER_WORLD_OPAQUE) && scene->forward_shader){
        render_forward_opaque_objects(scene->objects,
                                      scene->object_count,
                                      scene->forward_shader,
                                      scene->mesh_pool,
                                      scene->primitive_type,
                                      view->object_skip_flags);
    }

    if((view->pass_flags & RENDER_WORLD_PLANET) && scene->planet_shader && scene->gas_planet){
        render_planet_pass(scene->planet_shader,
                           scene->gas_planet,
                           scene->mesh_pool,
                           scene->planet_flow_tex,
                           view->view,
                           view->projection,
                           view->camera_pos,
                           view->light_dir,
                           scene->ring);
    }

    if(view->pass_flags & RENDER_WORLD_VOLUME){
        if(scene->ring){
            vol_ring(view->view, view->projection, view->camera_pos, view->light_dir, scene->ring);
        }
        if(scene->volume_count > 0){
            vol_begin(view->view, view->projection, view->camera_pos, view->light_dir, view->time);
            for(int i = 0; i < scene->volume_count; ++i) vol_draw(&scene->volumes[i]);
        }
        vol_end();
    }

    if((view->pass_flags & RENDER_WORLD_TRANSPARENT) && scene->forward_shader){
        render_forward_transparent_objects(scene->objects,
                                           scene->object_count,
                                           scene->forward_shader,
                                           scene->mesh_pool,
                                           scene->primitive_type,
                                           view->camera_pos,
                                           view->object_skip_flags);
    }
}

static GLuint create_rgba8_texture(int width, int height, const unsigned char* pixels){
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_2D);
    return tex;
}

static GLuint create_monitor_texture(void){
    const int width = 256;
    const int height = 144;
    unsigned char* pixels = (unsigned char*)malloc((size_t)width * (size_t)height * 4u);
    if(!pixels) return 0;

    for(int y = 0; y < height; ++y){
        for(int x = 0; x < width; ++x){
            const float u = (float)x / (float)(width - 1);
            const float v = (float)y / (float)(height - 1);
            const float scan = (y & 3) == 0 ? 0.72f : 1.0f;
            unsigned char r = (unsigned char)(7.0f * scan);
            unsigned char g = (unsigned char)(18.0f * scan);
            unsigned char b = (unsigned char)(28.0f * scan);

            if(x > 12 && x < width - 12 && y > 12 && y < height - 12){
                r = (unsigned char)(10.0f * scan);
                g = (unsigned char)(30.0f * scan);
                b = (unsigned char)(45.0f * scan);
            }
            if(y > 18 && y < 32){
                if((x > 22 && x < 78) || (x > 90 && x < 150) || (x > 170 && x < 234)){
                    r = (unsigned char)(40.0f * scan);
                    g = (unsigned char)(180.0f * scan);
                    b = (unsigned char)(225.0f * scan);
                }
            }

            if(x > 22 && x < width - 22 && y > 54 && y < 112){
                const int wave_y = (int)(78.0f + sinf(u * 16.0f + sinf(u * 42.0f) * 0.6f) * 16.0f);
                const int wave_delta = y > wave_y ? y - wave_y : wave_y - y;
                if(wave_delta <= 1){
                    r = (unsigned char)(255.0f * scan);
                    g = (unsigned char)(170.0f * scan);
                    b = (unsigned char)(72.0f * scan);
                }
                if(y > 92 && y < 108 && ((x / 18) & 1) == 0){
                    r = (unsigned char)(40.0f * scan);
                    g = (unsigned char)(210.0f * scan);
                    b = (unsigned char)(130.0f * scan);
                }
            }

            if(x > 176 && x < 236 && y > 40 && y < 88){
                const float du = (u - 0.81f) * 18.0f;
                const float dv = (v - 0.44f) * 18.0f;
                const float glow = 1.0f / (1.0f + du*du + dv*dv);
                const float boost = glow * 185.0f;
                r = (unsigned char)fminf(255.0f, r + boost * 0.35f);
                g = (unsigned char)fminf(255.0f, g + boost * 0.75f);
                b = (unsigned char)fminf(255.0f, b + boost);
            }

            unsigned char* px = pixels + (((size_t)y * (size_t)width + (size_t)x) * 4u);
            px[0] = r;
            px[1] = g;
            px[2] = b;
            px[3] = 255;
        }
    }

    GLuint tex = create_rgba8_texture(width, height, pixels);
    free(pixels);
    return tex;
}

static void make_panel_mesh(Mesh* mesh, float half_w, float half_h, int material_id){
    init_mesh(mesh, 8, 12);
    mesh->material_id = material_id;
    mesh->flags = MESH_HAS_UVS;

    const Color4 white = {1,1,1,1};
    mesh->vertices[0] = (VertexFormat){ {-half_w, -half_h, 0.0f}, { 0.0f, 0.0f, 1.0f}, white, {0.0f, 0.0f} };
    mesh->vertices[1] = (VertexFormat){ { half_w, -half_h, 0.0f}, { 0.0f, 0.0f, 1.0f}, white, {1.0f, 0.0f} };
    mesh->vertices[2] = (VertexFormat){ { half_w,  half_h, 0.0f}, { 0.0f, 0.0f, 1.0f}, white, {1.0f, 1.0f} };
    mesh->vertices[3] = (VertexFormat){ {-half_w,  half_h, 0.0f}, { 0.0f, 0.0f, 1.0f}, white, {0.0f, 1.0f} };
    mesh->vertices[4] = (VertexFormat){ {-half_w, -half_h, 0.0f}, { 0.0f, 0.0f,-1.0f}, white, {0.0f, 0.0f} };
    mesh->vertices[5] = (VertexFormat){ { half_w, -half_h, 0.0f}, { 0.0f, 0.0f,-1.0f}, white, {1.0f, 0.0f} };
    mesh->vertices[6] = (VertexFormat){ { half_w,  half_h, 0.0f}, { 0.0f, 0.0f,-1.0f}, white, {1.0f, 1.0f} };
    mesh->vertices[7] = (VertexFormat){ {-half_w,  half_h, 0.0f}, { 0.0f, 0.0f,-1.0f}, white, {0.0f, 1.0f} };
    mesh->vertex_count = 8;

    const unsigned indices[12] = { 0,1,2, 0,2,3, 4,6,5, 4,7,6 };
    memcpy(mesh->indices, indices, sizeof indices);
    mesh->index_count = 12;
}


typedef struct { void (*fn)(void *payload); } Call;

static DWORD WINAPI tramp(void *blk){
    Call *c = (Call*)blk;
    c->fn(c + 1);
    HeapFree(GetProcessHeap(), 0, blk);
    return 0;
}

static inline HANDLE launch_job(void (*fn)(void*), const void *payload, size_t bytes){
    size_t n = sizeof(Call) + bytes;
    Call *blk = (Call*)HeapAlloc(GetProcessHeap(), 0, n);
    blk->fn = fn;
    unsigned char *d = (unsigned char*)(blk + 1);
    const unsigned char *s = (const unsigned char*)payload;
    for (size_t i = 0; i < bytes; ++i) d[i] = s[i];
    return CreateThread(0, 0, tramp, blk, 0, 0);
}

static inline int handle_done(HANDLE h){
    return h && WaitForSingleObject(h, 0) == WAIT_OBJECT_0;
}

static inline void handle_close(HANDLE *h){
    if (*h) { CloseHandle(*h); *h = 0; }
}

typedef struct {
    void *p0, *p1, *p2, *p3;
    int   i0,  i1,  i2,  i3;
    float f0,  f1,  f2,  f3;
    Vec3  v0,  v1;
} Argv;
static void asteroid_job(void *vp){
    Argv *a = (Argv*)vp;
    Mesh *m  = (Mesh*)a->p0;
    generate_cubesphere(m, a->v0, a->f0, a->i0, 1);
    calculate_smooth_normals(m);
}

static void gas_job(void *vp){
    Argv *g = (Argv*)vp;
    Mesh *m  = (Mesh*)g->p0;
    generate_cubesphere(m, g->v0, g->f0, g->i0, 0);
}



static GLADapiproc APIENTRY glad_wgl_loader(const char* name) {
    return (GLADapiproc)wglGetProcAddress(name);
}
GLADapiproc APIENTRY glad_opengl_loader(const char *name) {
    GLADapiproc p = (GLADapiproc)wglGetProcAddress(name);
    if (!p || (uintptr_t)p <= 3 || p == (GLADapiproc)-1) {
        p = (GLADapiproc)GetProcAddress(GetModuleHandleA("opengl32.dll"), name);
    }
    return p;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// ReSharper disable once CppDFAConstantFunctionResult
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {

    // Register and create window
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "D";
    RegisterClass(&wc);
    hWnd = CreateWindow(wc.lpszClassName, "Dead Vector 2", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                             NULL, NULL, hInstance, NULL);
    hDC = GetDC(hWnd);

    // Dummy context
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24 };
    int pf = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pf, &pfd);
    HGLRC dummyRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, dummyRC);


    // Load WGL extensions
    gladLoadWGL(hDC, glad_wgl_loader);

    int pixelAttribs[] = {
    WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
    WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
    WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
    WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
    WGL_COLOR_BITS_ARB, 32,
    WGL_DEPTH_BITS_ARB, 24,
    WGL_STENCIL_BITS_ARB, 8,
    WGL_SAMPLE_BUFFERS_ARB, 1,
    WGL_SAMPLES_ARB, 4,
    0
    };

    int format;
    UINT numFormats;
    BOOL status = wglChoosePixelFormatARB(hDC, pixelAttribs, NULL, 1, &format, &numFormats);

    PIXELFORMATDESCRIPTOR realPFD;
    DescribePixelFormat(hDC, format, sizeof(realPFD), &realPFD);
    SetPixelFormat(hDC, format, &realPFD);
    int attribs[] = {
        0x2091, 4,
        0x2092, 3,
        0x9126, 0x00000001,
        0
    };
        typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    hRC = wglCreateContextAttribsARB(hDC, 0, attribs);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(dummyRC);

    wglMakeCurrent(hDC, hRC);

    gladLoadGL(glad_opengl_loader);

    // Load OpenGL functions

    glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);


    typedef BOOL (WINAPI *wglSwapIntervalEXT_t)(int);
    wglSwapIntervalEXT_t wglSwapIntervalEXT =
        (wglSwapIntervalEXT_t)wglGetProcAddress("wglSwapIntervalEXT");

    if (wglSwapIntervalEXT) wglSwapIntervalEXT(1);



    Mat4 projection;
    Mat4 cview;
    Mat4 tmp;


    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);

    ShowWindow(hWnd, nShowCmd);
    UpdateWindow(hWnd);
    ShowCursor(FALSE);

    LARGE_INTEGER freq, prev, curr;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);
    int flashlight_on = 0;
    int flashlight_prev = 0;

    snd_init();
    int engine_voice = -1;
    int fire_prev = 0;

    static const char PH_HELLO[]  = "ACp-fiAhTn6AeQNGgIOKCKyDd6oUqHd0qiJ4dnYCBDxwbIAC3mxUqgNGbGyKDRhsbGM";
    static const char PH_LIGHTS[] = "A0aOjooLro6Gqgmohn2qDhh9dWAaOHVtQAdibW-QEkZvcn0GHnJ0agehdGyqCcZsVKoNGGxsYx04bGxKDRhsbGM";
    static const char PH_LIGHTS_OUT[] = "A0aOjooLro6Dqgmog3-qDhh_e2AaOHt3QAuud2yqFMZsVKoOGGxsYA";
    const Vec3 speaker_off = {0.5f, 0.1f, -0.9f}; // speaker lives by the console
    const char* monitor_text = "CAM 02 - NO SIGNAL";
    int speech_voice = -1;
    int said_hello = 0;
    float game_time = 0.0f;


    Mesh asteroid_mesh = {0};
    Mesh gas_mesh      = {0};

    Argv A = {0};
    A.p0 = &asteroid_mesh;
    A.v0 = (Vec3){0, 0, 0};
    A.f0 = 5.0f; // radius
    A.i0 = 100; // subdivisions
    HANDLE hAst = launch_job(asteroid_job, &A, sizeof A);

    Argv G = {0};
    G.p0 = &gas_mesh;
    G.v0 = (Vec3){0,0,0};
    G.f0 = 1000.0f;
    G.i0 = 100;
    HANDLE hGas = launch_job(gas_job, &G, sizeof G);

    Object asteroid = {
        .position = {0.0f, 0.0f, -40.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale    = {1.0f, 1.0f, 1.0f},
        .mesh_id  = 1,
        .flags    = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS
    };

    Object gas_planet = {
        .position = {0, 0, 5000},
        .rotation = {0, 0, 0, 1},
        .scale    = {1, 1, 1},
        .mesh_id  = 2,
        .flags    = OBJ_FLAG_VISIBLE
    };

    // Build shaders
    ShaderProgram test_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/asteroid.frag");
    ShaderProgram skybox_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/skybox.frag");
    skybox_shader.u_view_loc = glGetUniformLocation(skybox_shader.id, "u_view");
    skybox_shader.u_projection_loc = glGetUniformLocation(skybox_shader.id, "u_projection");

    ShaderProgram univ_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/univ.frag");

    ShaderProgram planet_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/planet.frag");
    set_common_matrices(&test_shader, cview, projection);
    set_common_matrices(&skybox_shader, cview, projection);
    set_common_matrices(&planet_shader, cview, projection);
    set_common_matrices(&univ_shader, cview, projection);

    const static Material test_material = {
        .albedo = {1.0f, 0.15f, 0.45f, 1.0f},
        .roughness=0.12f,
        .metallic=0.0f,
        .emissiveF0=0.095f,
        .flags= MATERIAL_FLAG_EMISSIVEF0SWITCH,
        };
    material_pool[0] = &test_material;

    static Material asteroid_material = {
        .albedo = {1.0f, 1.00f, 1.0f, 1.0f},
        .roughness = 0.5f,
        .metallic = 0.5f,
        .emissiveF0 = 0.0f
    };
    material_pool[1] = &asteroid_material;
    Material metal_material = {
        .albedo = {1.0f, 1.0f, 1.0f, 1.0f},
        .roughness = 0.1f,
        .metallic = 1.0f,
        .emissiveF0 = 0.0f,
    };
    material_pool[2] = &metal_material;
    Material monitor_bezel_material = {
        .albedo = {0.06f, 0.08f, 0.10f, 1.0f},
        .roughness = 0.55f,
        .metallic = 0.15f,
        .emissiveF0 = 0.0f,
    };
    material_pool[4] = &monitor_bezel_material;
    Material monitor_screen_material = {
        .albedo = {1.0f, 1.0f, 1.0f, 1.0f},
        .roughness = 0.08f,
        .metallic = 0.0f,
        .emissiveF0 = 1.8f,
        .tex_id = 1,
        .flags = MATERIAL_FLAG_ALBEDO
    };
    material_pool[5] = &monitor_screen_material;
    Material monitor_glass_material = { // faint tint only, no refraction
        .albedo = {0.65f, 0.83f, 1.0f, 0.10f},
        .roughness = 0.02f,
        .metallic = 0.0f,
        .emissiveF0 = 0.0f,
    };
    material_pool[6] = &monitor_glass_material;
    Material shadow_pad_material = {
        .albedo = {0.68f, 0.72f, 0.78f, 1.0f},
        .roughness = 0.85f,
        .metallic = 0.0f,
        .emissiveF0 = 0.0f,
    };
    material_pool[7] = &shadow_pad_material;
    Material shadow_block_material = {
        .albedo = {0.92f, 0.52f, 0.24f, 1.0f},
        .roughness = 0.35f,
        .metallic = 0.05f,
        .emissiveF0 = 0.0f,
    };
    material_pool[8] = &shadow_block_material;
    Material demo_hull_material = {
        .albedo = {0.28f, 0.31f, 0.37f, 1.0f},
        .roughness = 0.58f,
        .metallic = 0.18f,
        .emissiveF0 = 0.0f,
    };
    material_pool[9] = &demo_hull_material;
    static Material console_body_material = {
        .albedo = {0.17f, 0.18f, 0.20f, 1.0f},
        .roughness = 0.62f,
        .metallic = 0.25f,
        .emissiveF0 = 0.0f,
    };
    material_pool[10] = &console_body_material;
    static Material console_lit_material = {
        .albedo = {1.0f, 0.62f, 0.18f, 1.0f},
        .roughness = 0.40f,
        .metallic = 0.0f,
        .emissiveF0 = 2.2f,
        .flags = MATERIAL_FLAG_EMISSIVEF0SWITCH,
    };
    material_pool[11] = &console_lit_material;
    texture_pool[0] = create_monitor_texture();
    texture_pool[1] = texture_pool[0];

    GLuint shadow_fbo = 0;
    glGenFramebuffers(1, &shadow_fbo);
    glGenTextures(1, &shadow_map_tex);
    glBindTexture(GL_TEXTURE_2D, shadow_map_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    {
        const float border[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_map_tex, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    RenderTarget scene_rt = {0};
    RenderTarget monitor_scene_rt = {0};
    RenderTarget monitor_display_rt = {0};
    ensure_render_target(&scene_rt, win_w, win_h, GL_RGBA16F, 1, 0);
    ensure_render_target(&monitor_scene_rt, 512, 288, GL_RGBA16F, 1, 0);
    ensure_render_target(&monitor_display_rt, 512, 288, GL_RGBA8, 0, 0);
    texture_pool[1] = monitor_display_rt.color_tex;

    //Skybox generation
    float skyboxVertices[] = {
    -1, -1, -1,  1, -1, -1,  1,  1, -1, -1,  1, -1,
    -1, -1,  1,  1, -1,  1,  1,  1,  1, -1,  1,  1,
    };
    unsigned int skyboxIndices[] = {
        // -Z
        0, 2, 3,   2, 0, 1,

        // +Z
        4, 6, 5,   6, 4, 7,

        // -X
        0, 7, 4,   7, 0, 3,

        // +X
        1, 6, 2,   6, 1, 5,

        // +Y
        3, 6, 7,   6, 3, 2,

        // -Y
        0, 5, 1,   5, 0, 4
    };
    // Generate skybox buffers
    glGenVertexArrays(1, &skybox_vao);
    glGenBuffers(1, &skybox_vbo);
    glGenBuffers(1, &skybox_ibo);

    // Bind and fill
    glBindVertexArray(skybox_vao);

    glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skybox_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), skyboxIndices, GL_STATIC_DRAW);

    // Position attribute only
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);

    glGenVertexArrays(1, &fullscreen_vao);


    //Ship
    Mesh spaceship;
    spaceship.material_id = 2;
    //init_mesh(&spaceship, 10000, 30000);
    //create_spaceship(&spaceship);


    init_mesh(&spaceship, 984 * 3, 1452 * 3); // room for mirrored copy

    memcpy(spaceship.vertices,
           spaceship1.vertices,
           (size_t)spaceship1.vertex_count * sizeof(VertexFormat));

    memcpy(spaceship.indices,
           spaceship1.indices,
           (size_t)spaceship1.index_count * sizeof(unsigned int));

    spaceship.submeshes     = NULL;
    spaceship.submesh_count = 0;
    spaceship.flags         = 0;

    free(sel_mirror(&spaceship, NULL, (Vec3){1,0,0}, 0, 1));

    upload_mesh(&spaceship);
    mesh_pool[3] = &spaceship;

    Object ship = {
        .position = {0.0f, 0.0f, 0.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh_id = 3,
        .flags = OBJ_FLAG_VISIBLE,
    };

    Mesh monitor_bezel_mesh = {0};
    make_panel_mesh(&monitor_bezel_mesh, 1.32f, 0.86f, 4);
    upload_mesh(&monitor_bezel_mesh);
    mesh_pool[5] = &monitor_bezel_mesh;

    Mesh monitor_screen_mesh = {0};
    make_panel_mesh(&monitor_screen_mesh, 1.12f, 0.64f, 5);
    upload_mesh(&monitor_screen_mesh);
    mesh_pool[6] = &monitor_screen_mesh;

    Mesh monitor_glass_mesh = {0};
    make_panel_mesh(&monitor_glass_mesh, 1.20f, 0.72f, 6);
    upload_mesh(&monitor_glass_mesh);
    mesh_pool[7] = &monitor_glass_mesh;

    Object monitor_bezel = {
        .scale = {0.5f, 0.5f, 0.5f},
        .mesh_id = 5,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS,
    };
    Object monitor_screen = {
        .scale = {0.5f, 0.5f, 0.5f},
        .mesh_id = 6,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS,
    };
    Object monitor_glass = {
        .scale = {0.5f, 0.5f, 0.5f},
        .mesh_id = 7,
        .flags = OBJ_FLAG_VISIBLE,
    };

    Mesh shadow_pad_mesh = {0};
    make_panel_mesh(&shadow_pad_mesh, 1.9f, 1.9f, 7);
    upload_mesh(&shadow_pad_mesh);
    mesh_pool[8] = &shadow_pad_mesh;

    Mesh shadow_block_mesh = {0};
    init_mesh(&shadow_block_mesh, 8, 36);
    make_box(&shadow_block_mesh, (Vec3){-0.35f, -0.35f, -0.35f}, (Vec3){0.35f, 0.35f, 0.35f});
    shadow_block_mesh.material_id = 8;
    upload_mesh(&shadow_block_mesh);
    mesh_pool[9] = &shadow_block_mesh;

    Object shadow_pad = {
        .position = {-2.8f, -2.05f, -8.5f},
        .rotation = quat_axis_angle(1.0f, 0.0f, 0.0f, -90.0f * DEG2RAD),
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh_id = 8,
        .flags = OBJ_FLAG_VISIBLE,
    };
    Object shadow_block = {
        .position = {-2.3f, -1.1f, -8.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {0.9f, 1.5f, 0.9f},
        .mesh_id = 9,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS,
    };

    Mesh demo_hull_mesh = {0};
    init_mesh(&demo_hull_mesh, 24, 36);
    make_box(&demo_hull_mesh, (Vec3){-1.0f, -0.35f, -1.4f}, (Vec3){1.0f, 0.35f, 1.4f});
    demo_hull_mesh.material_id = 9;
    upload_mesh(&demo_hull_mesh);
    mesh_pool[10] = &demo_hull_mesh;

    Mesh demo_nose_mesh = {0};
    init_mesh(&demo_nose_mesh, 24, 36);
    make_box(&demo_nose_mesh, (Vec3){-0.55f, -0.18f, -0.55f}, (Vec3){0.55f, 0.18f, 0.55f});
    demo_nose_mesh.material_id = 9;
    upload_mesh(&demo_nose_mesh);
    mesh_pool[11] = &demo_nose_mesh;

    Mesh demo_panel_mesh = {0};
    make_panel_mesh(&demo_panel_mesh, 0.86f, 0.62f, 4); // matte console, not a mirror
    upload_mesh(&demo_panel_mesh);
    mesh_pool[12] = &demo_panel_mesh;

    Object demo_hull = {
        .scale = {1.1f, 1.0f, 1.0f},
        .mesh_id = 10,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS,
    };
    Object demo_spine = {
        .scale = {0.55f, 0.42f, 0.72f},
        .mesh_id = 11,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS,
    };
    Object demo_left_panel = {
        .scale = {0.7f, 0.7f, 1.0f},
        .mesh_id = 12,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS | OBJ_FLAG_HIDE_IN_REFLECTION,
    };
    Object demo_right_panel = {
        .scale = {0.7f, 0.7f, 1.0f},
        .mesh_id = 12,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS | OBJ_FLAG_HIDE_IN_REFLECTION,
    };
    Mesh console_mesh = {0};
    mesh_build(&console_mesh, MESH_CONSOLE, MESH_LIB);
    upload_mesh(&console_mesh);
    mesh_pool[13] = &console_mesh;

    Object console = {
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh_id = 13,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS | OBJ_FLAG_HIDE_IN_REFLECTION,
    };

    Object demo_center_panel = {
        .scale = {0.58f, 0.48f, 1.0f},
        .mesh_id = 12,
        .flags = OBJ_FLAG_VISIBLE | OBJ_FLAG_CAST_SHADOWS | OBJ_FLAG_HIDE_IN_REFLECTION,
    };
    typedef struct { Object* obj; Vec3 off; Quat local; } CockpitPart;
    const Quat q_ident = {0, 0, 0, 1};
    const Quat q_console = quat_axis_angle(1, 0, 0, -35.0f * DEG2RAD);
    const Quat q_screen  = quat_axis_angle(1, 0, 0, -15.0f * DEG2RAD);
    const Quat q_lpanel  = quat_mul(quat_axis_angle(0, 1, 0,  42.0f * DEG2RAD), q_screen);
    const Quat q_rpanel  = quat_mul(quat_axis_angle(0, 1, 0, -42.0f * DEG2RAD), q_screen);
    const CockpitPart cockpit[] = {
        { &demo_hull,         {  0.00f, -1.15f, -0.50f }, q_ident },
        { &demo_spine,        {  0.00f,  0.80f,  0.20f }, q_ident },
        { &demo_left_panel,   { -0.95f, -0.35f, -0.85f }, q_lpanel },
        { &demo_right_panel,  {  0.95f, -0.35f, -0.85f }, q_rpanel },
        { &console,           {  0.00f, -0.66f, -1.02f }, q_console },
        { &monitor_bezel,     {  0.00f, -0.28f, -1.15f }, q_screen },
        { &monitor_screen,    {  0.00f, -0.27f, -1.14f }, q_screen },
        { &monitor_glass,     {  0.00f, -0.26f, -1.12f }, q_screen },
    };
    const int cockpit_count = (int)(sizeof cockpit / sizeof cockpit[0]);

    Body ship_body = {
        .rot          = {0, 0, 0, 1},
        .inv_mass     = 1.0f / SHIP_MASS,
        .inv_inertia  = ship_inv_inertia,
        .restitution  = 0.25f,      // metal into rock barely bounces
        .friction     = 0.45f,
        .shape        = SHAPE_SPHERES,
        .spheres      = ship_hull,
        .sphere_count = (int)(sizeof ship_hull / sizeof ship_hull[0]),
    };

    Body asteroid_body = {
        .pos          = asteroid.position,
        .rot          = {0, 0, 0, 1},
        .inv_mass     = 0.0f,       // immovable
        .restitution  = 0.25f,
        .friction     = 0.60f,
        .shape        = SHAPE_FIELD,
        .field_radius = 5.0f,       // matches the asteroid_job radius
    };

    static const Sphere block_hull[] = {{{0, 0, 0}, 0.62f}};
    Body block_body = {
        .pos          = shadow_block.position,
        .rot          = {0, 0, 0, 1},
        .inv_mass     = 1.0f / 0.8f,
        .inv_inertia  = {8.0f, 8.0f, 8.0f},
        .restitution  = 0.40f,
        .friction     = 0.35f,
        .shape        = SHAPE_SPHERES,
        .spheres      = block_hull,
        .sphere_count = 1,
    };

    Body* phys_bodies[] = { &ship_body, &asteroid_body, &block_body };
    const int phys_body_count = (int)(sizeof phys_bodies / sizeof phys_bodies[0]);

    //HUD
    hud_init_minimal();

    GLuint planetFlowTex = 0;
    glGenTextures(1, &planetFlowTex);
    glBindTexture(GL_TEXTURE_2D, planetFlowTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1024, 512, 0, GL_RGBA, GL_HALF_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    GLuint cs = compile_shader_from_file("shaders/swirl.comp", GL_COMPUTE_SHADER);
    GLuint bakeProg = glCreateProgram(); glAttachShader(bakeProg, cs); glLinkProgram(bakeProg); glDeleteShader(cs);
    glUseProgram(bakeProg);
    glBindImageTexture(0, planetFlowTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
    glDispatchCompute(1024/8, 512/8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
    glBindTexture(GL_TEXTURE_2D, planetFlowTex);
    glGenerateMipmap(GL_TEXTURE_2D);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, planetFlowTex);
    if (planet_shader.u_flow_tex_loc != -1) glUniform1i(planet_shader.u_flow_tex_loc, 0);

    const Object* forward_objects[] = {
        &asteroid,
        &monitor_bezel,
        &monitor_screen,
        &monitor_glass,
        &shadow_pad,
        &shadow_block,
        &demo_hull,
        &demo_spine,
        &demo_left_panel,
        &demo_right_panel,
        &demo_center_panel,
        &console
    };
    const int forward_object_count = (int)(sizeof forward_objects / sizeof forward_objects[0]);
    const Object* monitor_camera_objects[] = {
        &asteroid,
        &shadow_pad,
        &shadow_block
    };
    const int monitor_camera_object_count = (int)(sizeof monitor_camera_objects / sizeof monitor_camera_objects[0]);
    RenderScene main_scene = {
        .forward_shader = &univ_shader,
        .skybox_shader = &skybox_shader,
        .planet_shader = &planet_shader,
        .objects = forward_objects,
        .object_count = forward_object_count,
        .mesh_pool = mesh_pool,
        .gas_planet = &gas_planet,
        .planet_flow_tex = planetFlowTex,
        .primitive_type = GL_TRIANGLES,
        .ring = &gas_ring
    };
    RenderScene monitor_scene = {
        .forward_shader = &univ_shader,
        .skybox_shader = &skybox_shader,
        .planet_shader = &planet_shader,
        .objects = monitor_camera_objects,
        .object_count = monitor_camera_object_count,
        .mesh_pool = mesh_pool,
        .gas_planet = &gas_planet,
        .planet_flow_tex = planetFlowTex,
        .primitive_type = GL_TRIANGLES,
        .ring = &gas_ring
    };



    // 8. Main loop
    MSG msg;
    bool running = true;
    float fps_acc = 0.0f;
    int fps_frames = 0;
    char fpschar[16] = "FPS: 0";
    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) running = false;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // Timing
        QueryPerformanceCounter(&curr);
        float dt = (float)(curr.QuadPart - prev.QuadPart) / (float)freq.QuadPart;
        prev = curr;

        // Rotation input
        Vec3 torque = {0};
        if (GetAsyncKeyState('W') & 0x8000) torque.x -= 1.0f;
        if (GetAsyncKeyState('S') & 0x8000) torque.x += 1.0f;
        if (GetAsyncKeyState('A') & 0x8000) torque.y += 1.0f;
        if (GetAsyncKeyState('D') & 0x8000) torque.y -= 1.0f;
        if (GetAsyncKeyState('Q') & 0x8000) torque.z += 1.0f;
        if (GetAsyncKeyState('E') & 0x8000) torque.z -= 1.0f;

        angVel = vec3_add(angVel, vec3_scale(torque, dt));
        Quat wQuat = { angVel.x, angVel.y, angVel.z, 0.0f };
        Quat delta = quat_mul(rot, wQuat);
        rot.x += delta.x * 0.5f * dt;
        rot.y += delta.y * 0.5f * dt;
        rot.z += delta.z * 0.5f * dt;
        rot.w += delta.w * 0.5f * dt;
        rot = quat_normalize(rot);

        if (GetAsyncKeyState('R') & 0x8000) {
            //pos = (Vec3){0, 0, 0};
            vel = (Vec3){0, 0, 0};
            angVel = (Vec3){0, 0, 0,};
            asteroid_material.flags ^= MATERIAL_IS_FLAT;

            planet_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/planet.frag");
            skybox_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/skybox.frag");
            glEnable(GL_LINE_SMOOTH);

            univ_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/univ.frag");
        }
        // TRANSLATION INPUT
        Vec3 dir = {0};
        if (GetAsyncKeyState(VK_UP) & 0x8000)      dir.z -= 1.0f;
        if (GetAsyncKeyState(VK_DOWN) & 0x8000)    dir.z += 1.0f;
        if (GetAsyncKeyState(VK_LEFT) & 0x8000)    dir.x -= 1.0f;
        if (GetAsyncKeyState(VK_RIGHT) & 0x8000)   dir.x += 1.0f;
        if (GetAsyncKeyState(VK_SHIFT) & 0x8000)   dir.y += 1.0f;
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) dir.y -= 1.0f;

        Vec3 worldThrust = quat_rotate_vec3(rot, dir);
        vel = vec3_add(vel, vec3_scale(worldThrust, thrust));
        pos = vec3_add(pos, vec3_scale(vel, dt));

        // CAMERA
        Quat viewQuat = quat_conjugate(rot);
        quat_to_matrix(&viewQuat, cview);      // cview = R (temporary)

        Vec3 inv_pos = vec3_invert(pos);
        mat4_identity(tmp);
        mat4_translate(tmp, inv_pos.x, inv_pos.y, inv_pos.z); // tmp = T

        mat4_multiply(cview, cview, tmp);      // cview = R * T

        mat4_perspective(projection, 90.0f, aspectRatio, 0.1f, 10000.0f);


        glViewport(0, 0, win_w, win_h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDepthMask(GL_FALSE);
        glDepthFunc(GL_LEQUAL);
        glUseProgram(skybox_shader.id);
        glUniform1i(glGetUniformLocation(skybox_shader.id,"u_is_skybox"), 1);

        // Remove translation from view
        Mat4 view_no_translate;
        memcpy(view_no_translate, cview, sizeof(Mat4));
        view_no_translate[12] = view_no_translate[13] = view_no_translate[14] = 0.0f;

        glUniformMatrix4fv(skybox_shader.u_view_loc, 1, GL_FALSE, view_no_translate);
        glUniformMatrix4fv(skybox_shader.u_projection_loc, 1, GL_FALSE, projection);

        glBindVertexArray(skybox_vao);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
        glDepthFunc(GL_LESS); // Restore for normal geometry
        glDepthMask(GL_TRUE);

        Vec3 light_dir = vec3_normalize((Vec3){2.0f,-4.0f,1.0f});
        Mesh* a_mesh = mesh_pool[asteroid.mesh_id];
        const Material* a_mat = (a_mesh && a_mesh->material_id >= 0 && a_mesh->material_id < MAX_MATERIALS)
                                ? material_pool[a_mesh->material_id] : 0;
        uint32_t a_flags = (a_mesh ? a_mesh->flags : 0) | (a_mat ? a_mat->flags : 0);

        set_common_uniforms(&univ_shader, cview, projection, pos, light_dir, a_mat, a_flags);
        glPointSize(1.0f);
        draw_object(&asteroid, &univ_shader, mesh_pool, GL_TRIANGLES);

        glUseProgram(planet_shader.id);
        glUniform1i(glGetUniformLocation(planet_shader.id,"u_is_skybox"), 0);

        glUniformMatrix4fv(planet_shader.u_view_loc, 1, GL_FALSE, cview);
        glUniformMatrix4fv(planet_shader.u_projection_loc, 1, GL_FALSE, projection);
        glUniformMatrix4fv(planet_shader.u_light_space_matrix_loc, 1, GL_FALSE, light_space_matrix);

        glUniform3f(glGetUniformLocation(planet_shader.id, "u_light_dir"), light_dir.x, light_dir.y, light_dir.z);
        glUniform3f(glGetUniformLocation(planet_shader.id, "u_view_pos"), pos.x, pos.y, pos.z);
        mat4_perspective(projection, 45.0f, aspectRatio, 0.1f, 10000.0f);
        draw_object(&gas_planet, &planet_shader, mesh_pool, GL_TRIANGLES);

        //glDisable(GL_DEPTH_TEST);

        char fpschar[16];
        //unsigned fps = fps > (unsigned)(1.0f / dt) ? fps : (unsigned)(1.0f / dt);
        unsigned fps = (unsigned )(1.0f / dt);

        *(unsigned*)fpschar = 0x20535046; //FPS
        fpschar[4] = ':'; fpschar[5] = ' ';
        u32_to_str(fpschar + 6, fps);

        if (frame == 0){
            hud_clear();
            hud_draw_string(2, 2, fpschar);
        }
        //hud_draw_string(2, 2, "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");

        draw_hud();
        //glEnable(GL_DEPTH_TEST);

        SwapBuffers(hDC);

        if (handle_done(hAst)) { handle_close(&hAst); upload_mesh(&asteroid_mesh); mesh_pool[1] = &asteroid_mesh; }
        if (handle_done(hGas)) { handle_close(&hGas);  upload_mesh(&gas_mesh);     mesh_pool[2] = &gas_mesh; }

        if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) Sleep(0);
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            return 0;

        case WM_SIZE: {
            win_w = LOWORD(lParam);
            if (!win_w) win_w = 1;
            win_h = HIWORD(lParam); if (!win_h) win_h = 1;
            glViewport(0, 0, win_w, win_h);
            aspectRatio = (float)win_w / (float)win_h;

            return 0;
        }

        case WM_CLOSE:
            ClipCursor(NULL);
            PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            ClipCursor(NULL);
            snd_shutdown();
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(hRC); hRC = NULL;
            ReleaseDC(hWnd, hDC);  hDC  = NULL;
            return 0;

        case WM_MOUSEMOVE: {
            RECT windowRect;
            GetClientRect(hWnd, &windowRect);
            int centerX = (windowRect.left + windowRect.right) / 2;
            int centerY = (windowRect.top  + windowRect.bottom) / 2;
            POINT centerScreen = {centerX, centerY};
            ClientToScreen(hWnd, &centerScreen);
            if (view.y > 89.9f)  view.y = 89.9f;
            if (view.y < -89.9f) view.y = -89.9f;
            return 0;
        }

        //case WM_LBUTTONDOWN: lasers = TRUE;  return 0;
        //case WM_LBUTTONUP:   lasers = FALSE; return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                ShowCursor(TRUE);
                ReleaseCapture();
            }
            return 0;

        case WM_SYSKEYDOWN:
            if (wParam == VK_MENU) {
                return 0;
            }
            break;

        case WM_SYSCHAR:
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;

        default:
            break;
    }
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}




