#define WIN32_LEAN_AND_MEAN
#include "defs.h"
#include "3utils.h"
#include "4Font.h"
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
GLuint skybox_vao, skybox_vbo, skybox_ibo;
Mat4 light_space_matrix;
float aspectRatio = 8.0f/6.0f;

double playback = 1.0;

const Material* material_pool[MAX_MATERIALS];
Mesh* mesh_pool[256];
Vec3 pos = {0};      // Ship world position
Vec3 vel = {0};      // Ship linear velocity
Vec3 angVel = {0};   // Ship angular velocity (radians/sec)
Quat rot = {0, 0, 0, 1}; // Ship orientation
Vec2 view = {0, 0};  // Head pitch/yaw in radians
float dt = 0.016f;


float RF = 0.0f;
float IR = 0.0f;
float VIS = 1.0f;
float RAD = 0.25f;

float dRF = 0.0f;
float dIR = 0.0f;
float dVIS = 0.00f;
float dRAD =0.0f;

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

void create_spaceship(Mesh* m) {
    mesh_clear(m);

    static const VertexFormat right_verts[] = {
        {{0.00f, -0.20f, -1.20f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.25f, 0.00f, -1.20f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.18f, -1.20f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, -0.15f, 0.80f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.20f, 0.00f, 0.80f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.15f, 0.80f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, -0.05f, 1.50f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.08f, 0.00f, 1.50f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.05f, 1.50f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.20f, -0.08f, 0.30f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.20f, -0.02f, 0.30f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.25f, -0.05f, -0.10f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.70f, -0.10f, 0.50f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.70f, -0.04f, 0.50f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{1.20f, -0.12f, -0.30f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{1.20f, -0.06f, -0.30f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{1.50f, -0.13f, -0.90f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{1.50f, -0.10f, -0.90f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.90f, -0.08f, -0.80f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.40f, -0.06f, -0.60f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.15f, 0.70f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.12f, 0.15f, 0.60f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.38f, 0.70f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.14f, 0.32f, 0.60f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.02f, 0.42f, 0.90f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.11f, 0.36f, 0.85f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.25f, 1.10f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.08f, 0.22f, 1.05f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.16f, 0.20f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.20f, 0.20f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.20f, 0.00f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.16f, 0.00f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.17f, -0.30f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.19f, -0.30f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.19f, -0.10f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.17f, -0.10f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.15f, -0.70f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.22f, -0.70f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.22f, -0.40f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.55f, -0.15f, -0.40f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.18f, -1.00f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.18f, -1.20f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.45f, -1.05f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.48f, -1.22f}, {0,0,1}, {1,1,1,1}, {0,0}},
        {{0.00f, 0.65f, -1.25f}, {0,0,1}, {1,1,1,1}, {0,0}}
    };

    static const unsigned int right_indices[] = {
        0, 1, 2, 0, 1, 4, 0, 4, 3, 1, 2, 5, 1, 5, 4, 3, 4, 7, 3, 7, 6, 4, 5, 8, 4, 8, 7, 6, 7, 8, 9, 12, 10, 10, 12, 13, 12, 14, 13, 13, 14, 15, 14, 16, 15, 15, 16, 18, 16, 18, 17, 13, 15, 18, 13, 18, 19, 10, 13, 19, 10, 19, 11, 9, 10, 12, 11, 19, 9, 19, 18, 9, 9, 18, 14, 9, 14, 12, 20, 22, 21, 21, 22, 23, 22, 24, 23, 23, 24, 25, 24, 26, 25, 25, 26, 27, 28, 29, 33, 28, 33, 32, 29, 30, 34, 29, 34, 33, 30, 31, 35, 30, 35, 34, 31, 28, 32, 31, 32, 35, 32, 33, 37, 32, 37, 36, 33, 34, 38, 33, 38, 37, 34, 35, 39, 34, 39, 38, 35, 32, 36, 35, 36, 39, 28, 29, 30, 28, 30, 31, 36, 37, 38, 36, 38, 39, 40, 42, 41, 41, 42, 43, 42, 44, 43, 40, 41, 42, 41, 43, 42, 42, 43, 44
    };

    // Add right side vertices and indices
    ensure_v(m, 45);
    for(int i = 0; i < 45; i++) {
        m->vertices[m->vertex_count++] = right_verts[i];
    }

    ensure_i(m, 174);
    for(int i = 0; i < 174; i++) {
        m->indices[m->index_count++] = right_indices[i];
    }

    Mask right_only = mask_box(m, (Vec3){0.05f, -10, -10}, (Vec3){10, 10, 10});
    Mask left_side = sel_mirror(m, right_only, (Vec3){1, 0, 0}, 0.0f, 1);
    free(right_only);
    free(left_side);

    upload_mesh(m);
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

void generate_asteroid_cubesphere(Mesh* mesh, Vec3 center, float radius, int subdivisions)
{
    mesh->material_id = 1;
    if (subdivisions < 1) subdivisions = 1;

    const int faces = 6;
    int verts_per_face = (subdivisions + 1) * (subdivisions + 1);
    int estimated_vertices = verts_per_face * faces;
    int estimated_triangles = faces * subdivisions * subdivisions * 2;
    int estimated_indices = estimated_triangles * 3;

    init_mesh(mesh, estimated_vertices, estimated_indices);

    float step = 2.0f / subdivisions;

    for (int f = 0; f < faces; f++) {
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

                float base_shape    = fbm(dir, 3, 0.6f, 1.5f);
                float deform1       = 1.0f + base_shape * 0.7f;
                Vec3  bump_dir      = vec3_scale(dir, 3.0f);
                float surface_noise = fbm(bump_dir, 5, 0.5f, 2.5f);
                float deform2       = 1.0f + surface_noise * 0.25f;
                float deform        = deform1 * deform2;

                Vec3 final_pos = {
                    center.x + dir.x * radius * deform,
                    center.y + dir.y * radius * deform,
                    center.z + dir.z * radius * deform
                };

                float base_gray  = 0.33f + base_shape * 0.08f;
                float brown_tint = (fbm(vec3_scale(dir, 1.3f), 2, 0.5f, 2.0f) - 0.5f) * 0.12f;
                float blue_tint  = (1.0f - deform2) * fbm(vec3_scale(dir, 1.8f), 2, 0.5f, 2.0f) * 0.12f;

                Color4 color = {
                    .r = fminf(fmaxf(base_gray + brown_tint * 0.6f - blue_tint * 0.2f, 0.0f), 1.0f),
                    .g = fminf(fmaxf(base_gray + brown_tint * 0.4f - blue_tint * 0.1f, 0.0f), 1.0f),
                    .b = fminf(fmaxf(base_gray + blue_tint, 0.0f), 1.0f),
                    .a = 1.0f
                };

                VertexFormat vert = {
                    .position = final_pos,
                    .normal   = (Vec3){0,0,0},
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

                ensure_i(mesh, 6);

                if (flip) {
                    mesh->indices[mesh->index_count++] = a;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = b;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = d;
                    mesh->indices[mesh->index_count++] = b;
                } else {
                    mesh->indices[mesh->index_count++] = a;
                    mesh->indices[mesh->index_count++] = b;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = b;
                    mesh->indices[mesh->index_count++] = d;
                }
            }
        }
    }
}

void generate_gas_giant(Mesh* mesh, Vec3 center, float radius, int subdivisions)
{
    mesh->material_id = 1;
    if (subdivisions < 1) subdivisions = 1;

    const int faces = 6;
    int verts_per_face = (subdivisions + 1) * (subdivisions + 1);
    int estimated_vertices = verts_per_face * faces;
    int estimated_triangles = faces * subdivisions * subdivisions * 2;
    int estimated_indices = estimated_triangles * 3;

    init_mesh(mesh, estimated_vertices, estimated_indices);

    float step = 2.0f / subdivisions;

    for (int f = 0; f < faces; f++) {
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

                Vec3 final_pos = {
                    center.x + dir.x * radius,
                    center.y + dir.y * radius,
                    center.z + dir.z * radius
                };

                VertexFormat vert = {
                    .position = final_pos,
                    .normal   = dir,
                    .color    = (Color4){1,1,1,1},
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

                ensure_i(mesh, 6);

                if (flip) {
                    mesh->indices[mesh->index_count++] = a;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = b;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = d;
                    mesh->indices[mesh->index_count++] = b;
                } else {
                    mesh->indices[mesh->index_count++] = a;
                    mesh->indices[mesh->index_count++] = b;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = c;
                    mesh->indices[mesh->index_count++] = b;
                    mesh->indices[mesh->index_count++] = d;
                }
            }
        }
    }
}


void set_common_matrices(ShaderProgram* shader, const float* view, const float* projection) {
    glUseProgram(shader->id);
    glUniformMatrix4fv(shader->u_view_loc, 1, GL_FALSE, view);
    glUniformMatrix4fv(shader->u_projection_loc, 1, GL_FALSE, projection);
}

void set_common_uniforms(ShaderProgram* s,
                         const float* view,
                         const float* proj,
                         Vec3 cam_pos,
                         Vec3 light_dir,
                         const Material* mat,
                         uint32_t flags)
{
    glUseProgram(s->id);

    if (s->u_view_loc        != -1) glUniformMatrix4fv(s->u_view_loc,        1, GL_FALSE, view);
    if (s->u_projection_loc  != -1) glUniformMatrix4fv(s->u_projection_loc,  1, GL_FALSE, proj);
    if (s->u_light_space_matrix_loc != -1) glUniformMatrix4fv(s->u_light_space_matrix_loc, 1, GL_FALSE, light_space_matrix);

    GLint loc;
    if ((loc = glGetUniformLocation(s->id, "u_view_pos"))  != -1) glUniform3f(loc, cam_pos.x,  cam_pos.y,  cam_pos.z);
    if ((loc = glGetUniformLocation(s->id, "u_light_dir")) != -1) glUniform3f(loc, light_dir.x, light_dir.y, light_dir.z);
    if ((loc = glGetUniformLocation(s->id, "u_is_skybox")) != -1) glUniform1i(loc, 0);

    if (mat) {
        if (s->u_material_albedo_loc    != -1) glUniform4f(s->u_material_albedo_loc,    mat->albedo.r, mat->albedo.g, mat->albedo.b, mat->albedo.a);
        if (s->u_material_roughness_loc != -1) glUniform1f(s->u_material_roughness_loc, mat->roughness);
        if (s->u_material_metallic_loc  != -1) glUniform1f(s->u_material_metallic_loc,  mat->metallic);
        if (s->u_material_emissive_loc  != -1) glUniform1f(s->u_material_emissive_loc,  mat->emissiveF0);
    }
    if ((loc = glGetUniformLocation(s->id, "u_flags")) != -1)
        glUniform1i(loc, (GLint)flags);
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
    generate_asteroid_cubesphere(m, a->v0, a->f0, a->i0);
    calculate_smooth_normals(m);
}

static void gas_job(void *vp){
    Argv *g = (Argv*)vp;
    Mesh *m  = (Mesh*)g->p0;
    generate_gas_giant(m, g->v0, g->f0, g->i0);
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
        .flags    = OBJ_FLAG_VISIBLE
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
        .emissiveF0 = 0.5f
    };
    material_pool[1] = &asteroid_material;
    Material metal_material = {
        .albedo = {1.0f, 1.0f, 1.0f, 1.0f},
        .roughness = 0.1f,
        .metallic = 1.0f,
        .emissiveF0 = 0.0f,
    };
    material_pool[2] = &metal_material;
    Material glass_material = {
        .albedo = {1.0f, 1.0f, 1.0f, 0.9f},
        .roughness = 0.1f,
        .metallic = 1.0f,
        .emissiveF0 = 0.0f,
    };
    material_pool[3] = &glass_material;

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


    //Ship
    Mesh spaceship;
    spaceship.material_id = 2;
    //init_mesh(&spaceship, 10000, 30000);
    //create_spaceship(&spaceship);

    Mesh spaceship1;
    spaceship1 = (Mesh){ .vertices = (VertexFormat[]){ { {-0.500000f,0.093750f,0.687500f}, {-0.665000f,-0.200800f,0.719400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.164062f,0.765625f}, {-0.665000f,-0.200800f,0.719400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.468750f,0.242188f,0.757812f}, {-0.665000f,-0.200800f,0.719400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.242188f,0.671875f}, {-0.665000f,-0.200800f,0.719400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.546875f,0.054688f,0.578125f}, {-0.829400f,-0.303600f,0.468900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.500000f,0.093750f,0.687500f}, {-0.829400f,-0.303600f,0.468900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.242188f,0.671875f}, {-0.829400f,-0.303600f,0.468900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.625000f,0.242188f,0.562500f}, {-0.829400f,-0.303600f,0.468900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,-0.023438f,0.617188f}, {-0.415500f,-0.793300f,0.444900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.031250f,0.718750f}, {-0.415500f,-0.793300f,0.444900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.500000f,0.093750f,0.687500f}, {-0.415500f,-0.793300f,0.444900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.546875f,0.054688f,0.578125f}, {-0.415500f,-0.793300f,0.444900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.031250f,0.718750f}, {-0.360000f,-0.508900f,0.782000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.132812f,0.781250f}, {-0.360000f,-0.508900f,0.782000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.164062f,0.765625f}, {-0.360000f,-0.508900f,0.782000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.500000f,0.093750f,0.687500f}, {-0.360000f,-0.508900f,0.782000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {0.078700f,-0.539400f,0.838400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.164062f,0.796875f}, {0.078700f,-0.539400f,0.838400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.132812f,0.781250f}, {0.078700f,-0.539400f,0.838400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.031250f,0.718750f}, {0.078700f,-0.539400f,0.838400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.054688f,0.648438f}, {0.269600f,-0.841300f,0.468500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {0.269600f,-0.841300f,0.468500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.031250f,0.718750f}, {0.269600f,-0.841300f,0.468500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,-0.023438f,0.617188f}, {0.269600f,-0.841300f,0.468500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,0.242188f,0.656250f}, {0.770700f,-0.335200f,0.542000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,0.242188f,0.742188f}, {0.770700f,-0.335200f,0.542000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {0.770700f,-0.335200f,0.542000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.054688f,0.648438f}, {0.770700f,-0.335200f,0.542000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,0.242188f,0.742188f}, {0.468900f,-0.194000f,0.861700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.242188f,0.242188f,0.796875f}, {0.468900f,-0.194000f,0.861700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.164062f,0.796875f}, {0.468900f,-0.194000f,0.861700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {0.468900f,-0.194000f,0.861700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.390625f,0.742188f}, {0.476700f,0.190700f,0.858100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.328125f,0.796875f}, {0.476700f,0.190700f,0.858100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.242188f,0.242188f,0.796875f}, {0.476700f,0.190700f,0.858100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,0.242188f,0.742188f}, {0.476700f,0.190700f,0.858100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.437500f,0.648438f}, {0.767200f,0.326400f,0.552100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.390625f,0.742188f}, {0.767200f,0.326400f,0.552100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,0.242188f,0.742188f}, {0.767200f,0.326400f,0.552100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,0.242188f,0.656250f}, {0.767200f,0.326400f,0.552100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.515625f,0.617188f}, {0.251900f,0.817300f,0.518200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.453125f,0.718750f}, {0.251900f,0.817300f,0.518200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.390625f,0.742188f}, {0.251900f,0.817300f,0.518200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.437500f,0.648438f}, {0.251900f,0.817300f,0.518200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.453125f,0.718750f}, {0.094900f,0.569600f,0.816400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.359375f,0.781250f}, {0.094900f,0.569600f,0.816400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.328125f,0.796875f}, {0.094900f,0.569600f,0.816400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.390625f,0.742188f}, {0.094900f,0.569600f,0.816400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.500000f,0.390625f,0.687500f}, {-0.366700f,0.537000f,0.759700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.328125f,0.765625f}, {-0.366700f,0.537000f,0.759700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.359375f,0.781250f}, {-0.366700f,0.537000f,0.759700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.453125f,0.718750f}, {-0.366700f,0.537000f,0.759700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.546875f,0.437500f,0.578125f}, {-0.414100f,0.767200f,0.489800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.500000f,0.390625f,0.687500f}, {-0.414100f,0.767200f,0.489800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.453125f,0.718750f}, {-0.414100f,0.767200f,0.489800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.515625f,0.617188f}, {-0.414100f,0.767200f,0.489800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.625000f,0.242188f,0.562500f}, {-0.827700f,0.295200f,0.477100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.242188f,0.671875f}, {-0.827700f,0.295200f,0.477100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.500000f,0.390625f,0.687500f}, {-0.827700f,0.295200f,0.477100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.546875f,0.437500f,0.578125f}, {-0.827700f,0.295200f,0.477100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.242188f,0.671875f}, {-0.671300f,0.197100f,0.714500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.468750f,0.242188f,0.757812f}, {-0.671300f,0.197100f,0.714500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.328125f,0.765625f}, {-0.671300f,0.197100f,0.714500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.500000f,0.390625f,0.687500f}, {-0.671300f,0.197100f,0.714500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.335938f,0.781250f}, {-0.811100f,0.324400f,-0.486700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.328125f,0.765625f}, {-0.811100f,0.324400f,-0.486700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.468750f,0.242188f,0.757812f}, {-0.811100f,0.324400f,-0.486700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.242188f,0.773438f}, {-0.811100f,0.324400f,-0.486700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.375000f,0.804688f}, {-0.205200f,0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.359375f,0.781250f}, {-0.205200f,0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.328125f,0.765625f}, {-0.205200f,0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.335938f,0.781250f}, {-0.205200f,0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.335938f,0.820312f}, {0.422300f,0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.328125f,0.796875f}, {0.422300f,0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.359375f,0.781250f}, {0.422300f,0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.375000f,0.804688f}, {0.422300f,0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.242188f,0.820312f}, {0.824100f,0.322500f,-0.465800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.242188f,0.242188f,0.796875f}, {0.824100f,0.322500f,-0.465800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.328125f,0.796875f}, {0.824100f,0.322500f,-0.465800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.335938f,0.820312f}, {0.824100f,0.322500f,-0.465800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.156250f,0.820312f}, {0.813700f,-0.348700f,-0.465000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.164062f,0.796875f}, {0.813700f,-0.348700f,-0.465000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.242188f,0.242188f,0.796875f}, {0.813700f,-0.348700f,-0.465000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.242188f,0.820312f}, {0.813700f,-0.348700f,-0.465000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.117188f,0.804688f}, {0.422300f,-0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.132812f,0.781250f}, {0.422300f,-0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.164062f,0.796875f}, {0.422300f,-0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.156250f,0.820312f}, {0.422300f,-0.780600f,-0.460700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.156250f,0.781250f}, {-0.205200f,-0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.164062f,0.765625f}, {-0.205200f,-0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.132812f,0.781250f}, {-0.205200f,-0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.117188f,0.804688f}, {-0.205200f,-0.820600f,-0.533400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.242188f,0.773438f}, {-0.799500f,-0.351000f,-0.487500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.468750f,0.242188f,0.757812f}, {-0.799500f,-0.351000f,-0.487500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.164062f,0.765625f}, {-0.799500f,-0.351000f,-0.487500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.156250f,0.781250f}, {-0.799500f,-0.351000f,-0.487500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.242188f,0.773438f}, {-0.400000f,-0.062300f,0.914400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.156250f,0.781250f}, {-0.400000f,-0.062300f,0.914400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {-0.400000f,-0.062300f,0.914400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {-0.306900f,-0.175400f,0.935400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.156250f,0.781250f}, {-0.306900f,-0.175400f,0.935400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.117188f,0.804688f}, {-0.306900f,-0.175400f,0.935400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.117188f,0.804688f}, {-0.094500f,-0.183500f,0.978500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.156250f,0.820312f}, {-0.094500f,-0.183500f,0.978500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {-0.094500f,-0.183500f,0.978500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.156250f,0.820312f}, {0.062400f,-0.028300f,0.997700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.242188f,0.820312f}, {0.062400f,-0.028300f,0.997700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {0.062400f,-0.028300f,0.997700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.242188f,0.820312f}, {0.062400f,0.026000f,0.997700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.335938f,0.820312f}, {0.062400f,0.026000f,0.997700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {0.062400f,0.026000f,0.997700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,0.335938f,0.820312f}, {-0.099600f,0.172900f,0.979900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.375000f,0.804688f}, {-0.099600f,0.172900f,0.979900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {-0.099600f,0.172900f,0.979900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.375000f,0.804688f}, {-0.303600f,0.165600f,0.938300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.335938f,0.781250f}, {-0.303600f,0.165600f,0.938300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {-0.303600f,0.165600f,0.938300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.445312f,0.335938f,0.781250f}, {-0.400200f,0.057200f,0.914700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.242188f,0.773438f}, {-0.400200f,0.057200f,0.914700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,0.242188f,0.828125f}, {-0.400200f,0.057200f,0.914700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.945312f,0.640625f}, {-0.123100f,-0.861600f,0.492400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.929688f,0.632812f}, {-0.123100f,-0.861600f,0.492400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.968750f,0.554688f}, {-0.123100f,-0.861600f,0.492400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.984375f,0.578125f}, {-0.123100f,-0.861600f,0.492400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.929688f,0.632812f}, {-0.219000f,-0.864700f,0.452000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.914062f,0.632812f}, {-0.219000f,-0.864700f,0.452000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,-0.945312f,0.523438f}, {-0.219000f,-0.864700f,0.452000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.968750f,0.554688f}, {-0.219000f,-0.864700f,0.452000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.914062f,0.632812f}, {-0.590200f,-0.455000f,0.666800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,-0.820312f,0.664062f}, {-0.590200f,-0.455000f,0.666800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.367188f,-0.890625f,0.531250f}, {-0.590200f,-0.455000f,0.666800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,-0.945312f,0.523438f}, {-0.590200f,-0.455000f,0.666800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,-0.820312f,0.664062f}, {-0.768900f,-0.050600f,0.637400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.703125f,0.687500f}, {-0.768900f,-0.050600f,0.637400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,-0.695312f,0.570312f}, {-0.768900f,-0.050600f,0.637400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.367188f,-0.890625f,0.531250f}, {-0.768900f,-0.050600f,0.637400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.703125f,0.687500f}, {-0.779600f,0.090000f,0.619700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.445312f,0.710938f}, {-0.779600f,0.090000f,0.619700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,-0.437500f,0.570312f}, {-0.779600f,0.090000f,0.619700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,-0.695312f,0.570312f}, {-0.779600f,0.090000f,0.619700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.398438f,-0.046875f,0.671875f}, {-0.324100f,-0.818800f,0.473900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.140625f,0.531250f}, {-0.324100f,-0.818800f,0.473900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,-0.187500f,0.562500f}, {-0.324100f,-0.818800f,0.473900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.101562f,0.812500f}, {-0.324100f,-0.818800f,0.473900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.054688f,0.625000f}, {-0.385700f,-0.662900f,0.641700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,-0.039062f,0.539062f}, {-0.385700f,-0.662900f,0.641700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.140625f,0.531250f}, {-0.385700f,-0.662900f,0.641700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.398438f,-0.046875f,0.671875f}, {-0.385700f,-0.662900f,0.641700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.203125f,0.601562f}, {-0.689500f,-0.419300f,0.590600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.148438f,0.445312f}, {-0.689500f,-0.419300f,0.590600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,-0.039062f,0.539062f}, {-0.689500f,-0.419300f,0.590600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.054688f,0.625000f}, {-0.689500f,-0.419300f,0.590600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.742188f,0.375000f,0.656250f}, {-0.658800f,-0.363400f,0.658800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.429688f,0.593750f}, {-0.658800f,-0.363400f,0.658800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.148438f,0.445312f}, {-0.658800f,-0.363400f,0.658800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.203125f,0.601562f}, {-0.658800f,-0.363400f,0.658800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.687500f,0.414062f,0.726562f}, {-0.546500f,0.370700f,0.750900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.710938f,0.484375f,0.625000f}, {-0.546500f,0.370700f,0.750900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.429688f,0.593750f}, {-0.546500f,0.370700f,0.750900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.742188f,0.375000f,0.656250f}, {-0.546500f,0.370700f,0.750900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.546875f,0.796875f}, {-0.506400f,0.646400f,0.570600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.601562f,0.687500f}, {-0.506400f,0.646400f,0.570600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.710938f,0.484375f,0.625000f}, {-0.506400f,0.646400f,0.570600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.687500f,0.414062f,0.726562f}, {-0.506400f,0.646400f,0.570600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,0.640625f,0.835938f}, {-0.609200f,0.516700f,0.601500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.320312f,0.757812f,0.734375f}, {-0.609200f,0.516700f,0.601500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.601562f,0.687500f}, {-0.609200f,0.516700f,0.601500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.546875f,0.796875f}, {-0.609200f,0.516700f,0.601500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.617188f,0.851562f}, {0.044100f,0.661000f,0.749100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.718750f,0.757812f}, {0.044100f,0.661000f,0.749100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.320312f,0.757812f,0.734375f}, {0.044100f,0.661000f,0.749100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,0.640625f,0.835938f}, {0.044100f,0.661000f,0.749100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,0.429688f,0.843750f}, {0.724600f,0.318700f,0.611000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,0.492188f,0.750000f}, {0.724600f,0.318700f,0.611000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.718750f,0.757812f}, {0.724600f,0.318700f,0.611000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.617188f,0.851562f}, {0.724600f,0.318700f,0.611000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.351562f,0.820312f}, {0.588000f,0.555400f,0.588000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.429688f,0.742188f}, {0.588000f,0.555400f,0.588000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,0.492188f,0.750000f}, {0.588000f,0.555400f,0.588000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,0.429688f,0.843750f}, {0.588000f,0.555400f,0.588000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.617188f,0.851562f}, {-0.536100f,-0.390900f,0.748200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,0.468750f,0.757812f}, {-0.536100f,-0.390900f,0.748200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.414062f,0.773438f}, {-0.536100f,-0.390900f,0.748200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,0.429688f,0.843750f}, {-0.536100f,-0.390900f,0.748200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,0.640625f,0.835938f}, {-0.220700f,-0.469000f,0.855200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,0.476562f,0.742188f}, {-0.220700f,-0.469000f,0.855200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,0.468750f,0.757812f}, {-0.220700f,-0.469000f,0.855200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.617188f,0.851562f}, {-0.220700f,-0.469000f,0.855200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,0.640625f,0.835938f}, {0.079400f,-0.532100f,0.842900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.546875f,0.796875f}, {0.079400f,-0.532100f,0.842900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,0.437500f,0.718750f}, {0.079400f,-0.532100f,0.842900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,0.476562f,0.742188f}, {0.079400f,-0.532100f,0.842900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,0.546875f,0.796875f}, {0.082500f,-0.657500f,0.749000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.687500f,0.414062f,0.726562f}, {0.082500f,-0.657500f,0.749000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.375000f,0.664062f}, {0.082500f,-0.657500f,0.749000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,0.437500f,0.718750f}, {0.082500f,-0.657500f,0.749000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.687500f,0.414062f,0.726562f}, {-0.045700f,-0.566700f,0.822600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.742188f,0.375000f,0.656250f}, {-0.045700f,-0.566700f,0.822600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.296875f,0.648438f}, {-0.045700f,-0.566700f,0.822600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.375000f,0.664062f}, {-0.045700f,-0.566700f,0.822600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.742188f,0.375000f,0.656250f}, {-0.278400f,-0.213000f,0.936500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.203125f,0.601562f}, {-0.278400f,-0.213000f,0.936500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.625000f,0.187500f,0.648438f}, {-0.278400f,-0.213000f,0.936500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.296875f,0.648438f}, {-0.278400f,-0.213000f,0.936500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.203125f,0.601562f}, {-0.381300f,-0.182400f,0.906300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.054688f,0.625000f}, {-0.381300f,-0.182400f,0.906300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.062500f,0.671875f}, {-0.381300f,-0.182400f,0.906300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.625000f,0.187500f,0.648438f}, {-0.381300f,-0.182400f,0.906300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.054688f,0.625000f}, {-0.335700f,-0.287800f,0.896900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.398438f,-0.046875f,0.671875f}, {-0.335700f,-0.287800f,0.896900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.015625f,0.703125f}, {-0.335700f,-0.287800f,0.896900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.062500f,0.671875f}, {-0.335700f,-0.287800f,0.896900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.398438f,-0.046875f,0.671875f}, {-0.376200f,0.060300f,0.924600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.101562f,0.812500f}, {-0.376200f,0.060300f,0.924600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {-0.376200f,0.060300f,0.924600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.015625f,0.703125f}, {-0.376200f,0.060300f,0.924600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.046875f,0.726562f}, {0.135200f,0.268000f,0.953900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.140625f,0.750000f}, {0.135200f,0.268000f,0.953900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {0.135200f,0.268000f,0.953900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.101562f,0.812500f}, {0.135200f,0.268000f,0.953900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.351562f,0.820312f}, {-0.396100f,-0.432100f,0.810200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,0.429688f,0.843750f}, {-0.396100f,-0.432100f,0.810200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.414062f,0.773438f}, {-0.396100f,-0.432100f,0.810200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,0.304688f,0.765625f}, {-0.396100f,-0.432100f,0.810200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.210938f,0.765625f}, {-0.185600f,-0.247400f,0.951000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.351562f,0.820312f}, {-0.185600f,-0.247400f,0.951000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,0.304688f,0.765625f}, {-0.185600f,-0.247400f,0.951000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,0.210938f,0.757812f}, {-0.185600f,-0.247400f,0.951000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,0.210938f,0.757812f}, {-0.009900f,-0.194800f,0.980800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.140625f,0.750000f}, {-0.009900f,-0.194800f,0.980800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.046875f,0.726562f}, {-0.009900f,-0.194800f,0.980800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.210938f,0.765625f}, {-0.009900f,-0.194800f,0.980800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.945312f,0.640625f}, {-0.072100f,-0.696600f,0.713800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.890625f,0.687500f}, {-0.072100f,-0.696600f,0.713800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,-0.882812f,0.695312f}, {-0.072100f,-0.696600f,0.713800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.929688f,0.632812f}, {-0.072100f,-0.696600f,0.713800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.929688f,0.632812f}, {-0.186300f,-0.572300f,0.798600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,-0.882812f,0.695312f}, {-0.186300f,-0.572300f,0.798600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.835938f,0.710938f}, {-0.186300f,-0.572300f,0.798600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.914062f,0.632812f}, {-0.186300f,-0.572300f,0.798600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.914062f,0.632812f}, {-0.315700f,-0.270800f,0.909400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.835938f,0.710938f}, {-0.315700f,-0.270800f,0.909400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.718750f,0.734375f}, {-0.315700f,-0.270800f,0.909400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,-0.820312f,0.664062f}, {-0.315700f,-0.270800f,0.909400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.687500f,0.734375f}, {-0.306300f,-0.026500f,0.951600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,-0.445312f,0.750000f}, {-0.306300f,-0.026500f,0.951600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.445312f,0.710938f}, {-0.306300f,-0.026500f,0.951600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.703125f,0.687500f}, {-0.306300f,-0.026500f,0.951600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.703125f,0.687500f}, {-0.326600f,-0.130600f,0.936100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.265625f,-0.820312f,0.664062f}, {-0.326600f,-0.130600f,0.936100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.718750f,0.734375f}, {-0.326600f,-0.130600f,0.936100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.687500f,0.734375f}, {-0.326600f,-0.130600f,0.936100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.445312f,0.750000f}, {0.013700f,0.057400f,0.998300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.328125f,0.742188f}, {0.013700f,0.057400f,0.998300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.085938f,-0.289062f,0.742188f}, {0.013700f,0.057400f,0.998300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,-0.445312f,0.750000f}, {0.013700f,0.057400f,0.998300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.445312f,0.750000f}, {0.002600f,-0.065600f,0.997800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,-0.445312f,0.750000f}, {0.002600f,-0.065600f,0.997800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.687500f,0.734375f}, {0.002600f,-0.065600f,0.997800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.679688f,0.734375f}, {0.002600f,-0.065600f,0.997800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.687500f,0.734375f}, {-0.000000f,-0.000000f,1.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.718750f,0.734375f}, {-0.000000f,-0.000000f,1.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.765625f,0.734375f}, {-0.000000f,-0.000000f,1.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.679688f,0.734375f}, {-0.000000f,-0.000000f,1.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.273438f,0.781250f}, {-0.817400f,-0.574400f,-0.044200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,-0.226562f,0.796875f}, {-0.817400f,-0.574400f,-0.044200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.226562f,0.750000f}, {-0.817400f,-0.574400f,-0.044200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.085938f,-0.289062f,0.742188f}, {-0.817400f,-0.574400f,-0.044200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,-0.226562f,0.796875f}, {-0.949400f,0.229700f,-0.214400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.132812f,0.781250f}, {-0.949400f,0.229700f,-0.214400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,-0.148438f,0.742188f}, {-0.949400f,0.229700f,-0.214400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.226562f,0.750000f}, {-0.949400f,0.229700f,-0.214400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.132812f,0.781250f}, {-0.082500f,0.907300f,-0.412400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.039062f,-0.125000f,0.781250f}, {-0.082500f,0.907300f,-0.412400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.140625f,0.742188f}, {-0.082500f,0.907300f,-0.412400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,-0.148438f,0.742188f}, {-0.082500f,0.907300f,-0.412400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.039062f,-0.125000f,0.781250f}, {0.883600f,0.355500f,0.304700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.187500f,0.796875f}, {0.883600f,0.355500f,0.304700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.195312f,0.750000f}, {0.883600f,0.355500f,0.304700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.140625f,0.742188f}, {0.883600f,0.355500f,0.304700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.273438f,0.781250f}, {-0.420700f,-0.879700f,0.221800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.085938f,-0.289062f,0.742188f}, {-0.420700f,-0.879700f,0.221800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.328125f,0.742188f}, {-0.420700f,-0.879700f,0.221800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.320312f,0.781250f}, {-0.420700f,-0.879700f,0.221800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,-0.250000f,0.804688f}, {-0.287300f,-0.574700f,0.766300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.273438f,0.781250f}, {-0.287300f,-0.574700f,0.766300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.320312f,0.781250f}, {-0.287300f,-0.574700f,0.766300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.289062f,0.804688f}, {-0.287300f,-0.574700f,0.766300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.148438f,0.812500f}, {0.654200f,0.601900f,0.458000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.203125f,0.828125f}, {0.654200f,0.601900f,0.458000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.187500f,0.796875f}, {0.654200f,0.601900f,0.458000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.039062f,-0.125000f,0.781250f}, {0.654200f,0.601900f,0.458000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.156250f,0.812500f}, {-0.105200f,0.789200f,0.605100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.148438f,0.812500f}, {-0.105200f,0.789200f,0.605100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.039062f,-0.125000f,0.781250f}, {-0.105200f,0.789200f,0.605100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.132812f,0.781250f}, {-0.105200f,0.789200f,0.605100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.226562f,0.828125f}, {-0.758200f,0.291600f,0.583200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.156250f,0.812500f}, {-0.758200f,0.291600f,0.583200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.132812f,0.781250f}, {-0.758200f,0.291600f,0.583200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,-0.226562f,0.796875f}, {-0.758200f,0.291600f,0.583200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,-0.250000f,0.804688f}, {-0.388900f,-0.713000f,0.583400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.226562f,0.828125f}, {-0.388900f,-0.713000f,0.583400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,-0.226562f,0.796875f}, {-0.388900f,-0.713000f,0.583400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.273438f,0.781250f}, {-0.388900f,-0.713000f,0.583400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.156250f,0.812500f}, {-0.046300f,0.231400f,0.971800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.226562f,0.828125f}, {-0.046300f,0.231400f,0.971800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.203125f,0.828125f}, {-0.046300f,0.231400f,0.971800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.148438f,0.812500f}, {-0.046300f,0.231400f,0.971800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,-0.250000f,0.804688f}, {-0.033500f,-0.401800f,0.915100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.289062f,0.804688f}, {-0.033500f,-0.401800f,0.915100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.203125f,0.828125f}, {-0.033500f,-0.401800f,0.915100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.226562f,0.828125f}, {-0.033500f,-0.401800f,0.915100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.101562f,0.812500f}, {0.445200f,-0.161000f,0.880900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,-0.148438f,0.742188f}, {0.445200f,-0.161000f,0.880900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.140625f,0.742188f}, {0.445200f,-0.161000f,0.880900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.046875f,0.726562f}, {0.445200f,-0.161000f,0.880900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.242188f,0.710938f}, {0.218200f,-0.436400f,0.872900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.226562f,0.750000f}, {0.218200f,-0.436400f,0.872900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.101562f,-0.148438f,0.742188f}, {0.218200f,-0.436400f,0.872900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.101562f,0.812500f}, {0.218200f,-0.436400f,0.872900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.312500f,0.710938f}, {-0.434100f,-0.129000f,0.891600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.085938f,-0.289062f,0.742188f}, {-0.434100f,-0.129000f,0.891600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.226562f,0.750000f}, {-0.434100f,-0.129000f,0.891600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.242188f,0.710938f}, {-0.434100f,-0.129000f,0.891600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.445312f,0.710938f}, {-0.300800f,0.050100f,0.952400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.078125f,-0.445312f,0.750000f}, {-0.300800f,0.050100f,0.952400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.085938f,-0.289062f,0.742188f}, {-0.300800f,0.050100f,0.952400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.312500f,0.710938f}, {-0.300800f,0.050100f,0.952400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.312500f,0.710938f}, {-0.812300f,0.301000f,0.499600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.257812f,-0.312500f,0.554688f}, {-0.812300f,0.301000f,0.499600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,-0.437500f,0.570312f}, {-0.812300f,0.301000f,0.499600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.445312f,0.710938f}, {-0.812300f,0.301000f,0.499600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.242188f,0.710938f}, {-0.875300f,0.257400f,0.409300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.250000f,0.554688f}, {-0.875300f,0.257400f,0.409300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.257812f,-0.312500f,0.554688f}, {-0.875300f,0.257400f,0.409300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.312500f,0.710938f}, {-0.875300f,0.257400f,0.409300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.242188f,0.710938f}, {-0.938500f,0.160100f,0.306000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.101562f,0.812500f}, {-0.938500f,0.160100f,0.306000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,-0.187500f,0.562500f}, {-0.938500f,0.160100f,0.306000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.250000f,0.554688f}, {-0.938500f,0.160100f,0.306000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.773438f,0.718750f}, {-0.223700f,-0.653900f,0.722700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.765625f,0.734375f}, {-0.223700f,-0.653900f,0.722700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.718750f,0.734375f}, {-0.223700f,-0.653900f,0.722700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.742188f,0.726562f}, {-0.223700f,-0.653900f,0.722700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.742188f,0.726562f}, {0.153600f,-0.199700f,0.967700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,-0.718750f,0.734375f}, {0.153600f,-0.199700f,0.967700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.835938f,0.710938f}, {0.153600f,-0.199700f,0.967700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.820312f,0.710938f}, {0.153600f,-0.199700f,0.967700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.820312f,0.710938f}, {0.273300f,-0.102500f,0.956500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.117188f,-0.835938f,0.710938f}, {0.273300f,-0.102500f,0.956500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,-0.882812f,0.695312f}, {0.273300f,-0.102500f,0.956500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.867188f,0.687500f}, {0.273300f,-0.102500f,0.956500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.867188f,0.687500f}, {0.097600f,0.195200f,0.975900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,-0.882812f,0.695312f}, {0.097600f,0.195200f,0.975900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.890625f,0.687500f}, {0.097600f,0.195200f,0.975900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.875000f,0.687500f}, {0.097600f,0.195200f,0.975900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.851562f,0.632812f}, {0.158200f,0.949400f,0.271300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.867188f,0.687500f}, {0.158200f,0.949400f,0.271300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.875000f,0.687500f}, {0.158200f,0.949400f,0.271300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.859375f,0.632812f}, {0.158200f,0.949400f,0.271300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.812500f,0.640625f}, {0.693400f,0.708200f,0.132800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.820312f,0.710938f}, {0.693400f,0.708200f,0.132800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.867188f,0.687500f}, {0.693400f,0.708200f,0.132800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.851562f,0.632812f}, {0.693400f,0.708200f,0.132800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.750000f,0.664062f}, {1.000000f,-0.000000f,-0.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.742188f,0.726562f}, {1.000000f,-0.000000f,-0.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.820312f,0.710938f}, {1.000000f,-0.000000f,-0.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.812500f,0.640625f}, {1.000000f,-0.000000f,-0.000000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.781250f,0.656250f}, {-0.305100f,-0.945000f,0.118100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.773438f,0.718750f}, {-0.305100f,-0.945000f,0.118100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.742188f,0.726562f}, {-0.305100f,-0.945000f,0.118100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.750000f,0.664062f}, {-0.305100f,-0.945000f,0.118100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.851562f,0.632812f}, {-0.029800f,-0.298100f,0.954100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.859375f,0.632812f}, {-0.029800f,-0.298100f,0.954100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.781250f,0.656250f}, {-0.029800f,-0.298100f,0.954100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.750000f,0.664062f}, {-0.029800f,-0.298100f,0.954100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.812500f,0.640625f}, {-0.135300f,-0.347900f,0.927700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.046875f,-0.851562f,0.632812f}, {-0.135300f,-0.347900f,0.927700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.093750f,-0.750000f,0.664062f}, {-0.135300f,-0.347900f,0.927700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.187500f,0.156250f,0.773438f}, {0.508500f,-0.275500f,0.815800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.140625f,0.750000f}, {0.508500f,-0.275500f,0.815800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,0.210938f,0.757812f}, {0.508500f,-0.275500f,0.815800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.171875f,0.218750f,0.781250f}, {0.508500f,-0.275500f,0.815800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.171875f,0.218750f,0.781250f}, {0.384300f,-0.041900f,0.922300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.132812f,0.210938f,0.757812f}, {0.384300f,-0.041900f,0.922300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,0.304688f,0.765625f}, {0.384300f,-0.041900f,0.922300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,0.296875f,0.781250f}, {0.384300f,-0.041900f,0.922300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,0.296875f,0.781250f}, {0.208300f,0.037400f,0.977400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,0.304688f,0.765625f}, {0.208300f,0.037400f,0.977400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.414062f,0.773438f}, {0.208300f,0.037400f,0.977400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,0.375000f,0.781250f}, {0.208300f,0.037400f,0.977400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.109375f,0.781250f}, {0.572100f,-0.476700f,0.667400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {0.572100f,-0.476700f,0.667400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.140625f,0.750000f}, {0.572100f,-0.476700f,0.667400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.187500f,0.156250f,0.773438f}, {0.572100f,-0.476700f,0.667400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.062500f,0.742188f}, {0.136900f,-0.753100f,0.643500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.015625f,0.703125f}, {0.136900f,-0.753100f,0.643500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.093750f,0.742188f}, {0.136900f,-0.753100f,0.643500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.109375f,0.781250f}, {0.136900f,-0.753100f,0.643500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.101562f,0.718750f}, {-0.408800f,-0.607100f,0.681400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.062500f,0.671875f}, {-0.408800f,-0.607100f,0.681400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.015625f,0.703125f}, {-0.408800f,-0.607100f,0.681400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.062500f,0.742188f}, {-0.408800f,-0.607100f,0.681400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.578125f,0.195312f,0.679688f}, {-0.574000f,-0.413000f,0.707000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.625000f,0.187500f,0.648438f}, {-0.574000f,-0.413000f,0.707000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.062500f,0.671875f}, {-0.574000f,-0.413000f,0.707000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.101562f,0.718750f}, {-0.574000f,-0.413000f,0.707000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.585938f,0.289062f,0.687500f}, {-0.566500f,-0.096800f,0.818300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.296875f,0.648438f}, {-0.566500f,-0.096800f,0.818300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.625000f,0.187500f,0.648438f}, {-0.566500f,-0.096800f,0.818300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.578125f,0.195312f,0.679688f}, {-0.566500f,-0.096800f,0.818300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.351562f,0.695312f}, {-0.570300f,0.118000f,0.812900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.375000f,0.664062f}, {-0.570300f,0.118000f,0.812900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.296875f,0.648438f}, {-0.570300f,0.118000f,0.812900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.585938f,0.289062f,0.687500f}, {-0.570300f,0.118000f,0.812900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.421875f,0.398438f,0.773438f}, {-0.482300f,0.562100f,0.671900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,0.437500f,0.718750f}, {-0.482300f,0.562100f,0.671900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.375000f,0.664062f}, {-0.482300f,0.562100f,0.671900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.351562f,0.695312f}, {-0.482300f,0.562100f,0.671900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.429688f,0.757812f}, {-0.260400f,0.611400f,0.747300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,0.476562f,0.742188f}, {-0.260400f,0.611400f,0.747300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,0.437500f,0.718750f}, {-0.260400f,0.611400f,0.747300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.421875f,0.398438f,0.773438f}, {-0.260400f,0.611400f,0.747300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.421875f,0.773438f}, {-0.164000f,0.360700f,0.918200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,0.468750f,0.757812f}, {-0.164000f,0.360700f,0.918200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,0.476562f,0.742188f}, {-0.164000f,0.360700f,0.918200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.429688f,0.757812f}, {-0.164000f,0.360700f,0.918200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,0.375000f,0.781250f}, {0.017800f,0.249500f,0.968200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,0.414062f,0.773438f}, {0.017800f,0.249500f,0.968200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,0.468750f,0.757812f}, {0.017800f,0.249500f,0.968200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.421875f,0.773438f}, {0.017800f,0.249500f,0.968200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,0.359375f,0.757812f}, {-0.327300f,-0.416600f,0.848100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,0.375000f,0.781250f}, {-0.327300f,-0.416600f,0.848100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.421875f,0.773438f}, {-0.327300f,-0.416600f,0.848100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.281250f,0.398438f,0.765625f}, {-0.327300f,-0.416600f,0.848100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.281250f,0.398438f,0.765625f}, {-0.281100f,-0.261000f,0.923500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.273438f,0.421875f,0.773438f}, {-0.281100f,-0.261000f,0.923500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.429688f,0.757812f}, {-0.281100f,-0.261000f,0.923500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.406250f,0.750000f}, {-0.281100f,-0.261000f,0.923500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.406250f,0.750000f}, {0.254200f,-0.651400f,0.714900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.429688f,0.757812f}, {0.254200f,-0.651400f,0.714900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.421875f,0.398438f,0.773438f}, {0.254200f,-0.651400f,0.714900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.414062f,0.390625f,0.750000f}, {0.254200f,-0.651400f,0.714900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.414062f,0.390625f,0.750000f}, {0.026000f,-0.845500f,0.533300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.421875f,0.398438f,0.773438f}, {0.026000f,-0.845500f,0.533300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.351562f,0.695312f}, {0.026000f,-0.845500f,0.533300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.531250f,0.335938f,0.679688f}, {0.026000f,-0.845500f,0.533300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.531250f,0.335938f,0.679688f}, {0.351800f,-0.260600f,0.899100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.562500f,0.351562f,0.695312f}, {0.351800f,-0.260600f,0.899100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.585938f,0.289062f,0.687500f}, {0.351800f,-0.260600f,0.899100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.554688f,0.281250f,0.671875f}, {0.351800f,-0.260600f,0.899100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.554688f,0.281250f,0.671875f}, {0.352300f,-0.011000f,0.935800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.585938f,0.289062f,0.687500f}, {0.352300f,-0.011000f,0.935800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.578125f,0.195312f,0.679688f}, {0.352300f,-0.011000f,0.935800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.546875f,0.210938f,0.671875f}, {0.352300f,-0.011000f,0.935800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.546875f,0.210938f,0.671875f}, {0.131700f,0.460800f,0.877700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.578125f,0.195312f,0.679688f}, {0.131700f,0.460800f,0.877700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.101562f,0.718750f}, {0.131700f,0.460800f,0.877700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.117188f,0.703125f}, {0.131700f,0.460800f,0.877700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.117188f,0.703125f}, {0.034200f,0.615900f,0.787000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.476562f,0.101562f,0.718750f}, {0.034200f,0.615900f,0.787000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.062500f,0.742188f}, {0.034200f,0.615900f,0.787000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.085938f,0.726562f}, {0.034200f,0.615900f,0.787000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.085938f,0.726562f}, {-0.360300f,0.583600f,0.727700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.375000f,0.062500f,0.742188f}, {-0.360300f,0.583600f,0.727700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.109375f,0.781250f}, {-0.360300f,0.583600f,0.727700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.242188f,0.125000f,0.757812f}, {-0.360300f,0.583600f,0.727700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.242188f,0.125000f,0.757812f}, {-0.498800f,0.530000f,0.685800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.226562f,0.109375f,0.781250f}, {-0.498800f,0.530000f,0.685800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.187500f,0.156250f,0.773438f}, {-0.498800f,0.530000f,0.685800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.171875f,0.750000f}, {-0.498800f,0.530000f,0.685800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.195312f,0.296875f,0.757812f}, {-0.666700f,-0.333300f,0.666700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,0.296875f,0.781250f}, {-0.666700f,-0.333300f,0.666700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,0.375000f,0.781250f}, {-0.666700f,-0.333300f,0.666700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,0.359375f,0.757812f}, {-0.666700f,-0.333300f,0.666700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.195312f,0.226562f,0.750000f}, {-0.816500f,-0.073100f,0.572700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.171875f,0.218750f,0.781250f}, {-0.816500f,-0.073100f,0.572700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,0.296875f,0.781250f}, {-0.816500f,-0.073100f,0.572700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.195312f,0.296875f,0.757812f}, {-0.816500f,-0.073100f,0.572700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,0.171875f,0.750000f}, {-0.784000f,0.116100f,0.609800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.187500f,0.156250f,0.773438f}, {-0.784000f,0.116100f,0.609800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.171875f,0.218750f,0.781250f}, {-0.784000f,0.116100f,0.609800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.195312f,0.226562f,0.750000f}, {-0.784000f,0.116100f,0.609800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,0.460938f,0.609375f}, {0.530600f,0.811100f,-0.246100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,0.492188f,0.750000f}, {0.530600f,0.811100f,-0.246100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.429688f,0.742188f}, {0.530600f,0.811100f,-0.246100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.406250f,0.601562f}, {0.530600f,0.811100f,-0.246100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.195312f,0.664062f,0.617188f}, {0.851100f,0.369500f,-0.373000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.718750f,0.757812f}, {0.851100f,0.369500f,-0.373000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.062500f,0.492188f,0.750000f}, {0.851100f,0.369500f,-0.373000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,0.460938f,0.609375f}, {0.851100f,0.369500f,-0.373000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.687500f,0.593750f}, {0.244600f,0.867500f,-0.433100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.320312f,0.757812f,0.734375f}, {0.244600f,0.867500f,-0.433100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.156250f,0.718750f,0.757812f}, {0.244600f,0.867500f,-0.433100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.195312f,0.664062f,0.617188f}, {0.244600f,0.867500f,-0.433100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.554688f,0.554688f}, {-0.592400f,0.746500f,-0.303000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.601562f,0.687500f}, {-0.592400f,0.746500f,-0.303000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.320312f,0.757812f,0.734375f}, {-0.592400f,0.746500f,-0.303000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.687500f,0.593750f}, {-0.592400f,0.746500f,-0.303000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.679688f,0.453125f,0.492188f}, {-0.368500f,0.875800f,-0.311800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.710938f,0.484375f,0.625000f}, {-0.368500f,0.875800f,-0.311800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.492188f,0.601562f,0.687500f}, {-0.368500f,0.875800f,-0.311800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.554688f,0.554688f}, {-0.368500f,0.875800f,-0.311800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.406250f,0.460938f}, {-0.282100f,0.915100f,-0.288000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.429688f,0.593750f}, {-0.282100f,0.915100f,-0.288000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.710938f,0.484375f,0.625000f}, {-0.282100f,0.915100f,-0.288000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.679688f,0.453125f,0.492188f}, {-0.282100f,0.915100f,-0.288000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.164062f,0.375000f}, {-0.856100f,0.134000f,-0.499100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.148438f,0.445312f}, {-0.856100f,0.134000f,-0.499100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.429688f,0.593750f}, {-0.856100f,0.134000f,-0.499100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.406250f,0.460938f}, {-0.856100f,0.134000f,-0.499100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.000000f,0.414062f}, {-0.534200f,-0.723300f,-0.437600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,-0.039062f,0.539062f}, {-0.534200f,-0.723300f,-0.437600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.148438f,0.445312f}, {-0.534200f,-0.723300f,-0.437600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.164062f,0.375000f}, {-0.534200f,-0.723300f,-0.437600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.093750f,0.468750f}, {-0.384900f,-0.813100f,-0.436800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.140625f,0.531250f}, {-0.384900f,-0.813100f,-0.436800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,-0.039062f,0.539062f}, {-0.384900f,-0.813100f,-0.436800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.000000f,0.414062f}, {-0.384900f,-0.813100f,-0.436800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.414062f,0.257812f}, {-0.233500f,-0.580600f,-0.780000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.484375f,0.281250f}, {-0.233500f,-0.580600f,-0.780000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.570312f,0.320312f}, {-0.233500f,-0.580600f,-0.780000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.539062f,0.359375f}, {-0.233500f,-0.580600f,-0.780000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.539062f,0.359375f}, {-0.244900f,-0.058300f,-0.967800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.570312f,0.320312f}, {-0.244900f,-0.058300f,-0.967800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.804688f,0.343750f}, {-0.244900f,-0.058300f,-0.967800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,-0.757812f,0.367188f}, {-0.244900f,-0.058300f,-0.967800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,-0.757812f,0.367188f}, {-0.116300f,-0.453500f,-0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.804688f,0.343750f}, {-0.116300f,-0.453500f,-0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.976562f,0.460938f}, {-0.116300f,-0.453500f,-0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.945312f,0.437500f}, {-0.116300f,-0.453500f,-0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.976562f,0.460938f}, {-0.115200f,-0.983600f,-0.138800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.984375f,0.578125f}, {-0.115200f,-0.983600f,-0.138800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.968750f,0.554688f}, {-0.115200f,-0.983600f,-0.138800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.945312f,0.437500f}, {-0.115200f,-0.983600f,-0.138800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.945312f,0.437500f}, {-0.118400f,-0.966900f,-0.226000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.968750f,0.554688f}, {-0.118400f,-0.966900f,-0.226000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,-0.945312f,0.523438f}, {-0.118400f,-0.966900f,-0.226000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,-0.914062f,0.398438f}, {-0.118400f,-0.966900f,-0.226000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,-0.914062f,0.398438f}, {-0.959700f,-0.008500f,-0.280800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,-0.945312f,0.523438f}, {-0.959700f,-0.008500f,-0.280800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.367188f,-0.890625f,0.531250f}, {-0.959700f,-0.008500f,-0.280800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.289062f,-0.710938f,0.382812f}, {-0.959700f,-0.008500f,-0.280800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.289062f,-0.710938f,0.382812f}, {-0.931900f,0.162900f,-0.324200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.367188f,-0.890625f,0.531250f}, {-0.931900f,0.162900f,-0.324200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,-0.695312f,0.570312f}, {-0.931900f,0.162900f,-0.324200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.500000f,0.390625f}, {-0.931900f,0.162900f,-0.324200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.539062f,0.359375f}, {-0.162600f,0.020700f,-0.986500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,-0.757812f,0.367188f}, {-0.162600f,0.020700f,-0.986500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.289062f,-0.710938f,0.382812f}, {-0.162600f,0.020700f,-0.986500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.500000f,0.390625f}, {-0.162600f,0.020700f,-0.986500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.164062f,-0.945312f,0.437500f}, {0.018800f,-0.217700f,-0.975800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.328125f,-0.914062f,0.398438f}, {0.018800f,-0.217700f,-0.975800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.289062f,-0.710938f,0.382812f}, {0.018800f,-0.217700f,-0.975800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.140625f,-0.757812f,0.367188f}, {0.018800f,-0.217700f,-0.975800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.125000f,-0.539062f,0.359375f}, {-0.753800f,-0.292600f,-0.588400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.500000f,0.390625f}, {-0.753800f,-0.292600f,-0.588400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.351562f,0.406250f}, {-0.753800f,-0.292600f,-0.588400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.414062f,0.257812f}, {-0.753800f,-0.292600f,-0.588400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.250000f,-0.500000f,0.390625f}, {-0.919600f,0.137900f,-0.367800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.351562f,-0.695312f,0.570312f}, {-0.919600f,0.137900f,-0.367800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,-0.437500f,0.570312f}, {-0.919600f,0.137900f,-0.367800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.351562f,0.406250f}, {-0.919600f,0.137900f,-0.367800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.226562f,0.468750f}, {-0.929700f,0.312700f,-0.194400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.218750f,-0.281250f,0.429688f}, {-0.929700f,0.312700f,-0.194400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.257812f,-0.312500f,0.554688f}, {-0.929700f,0.312700f,-0.194400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.250000f,0.554688f}, {-0.929700f,0.312700f,-0.194400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.218750f,-0.281250f,0.429688f}, {-0.912000f,0.337600f,-0.232900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.351562f,0.406250f}, {-0.912000f,0.337600f,-0.232900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.312500f,-0.437500f,0.570312f}, {-0.912000f,0.337600f,-0.232900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.257812f,-0.312500f,0.554688f}, {-0.912000f,0.337600f,-0.232900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.226562f,0.468750f}, {-0.940700f,0.333800f,-0.060700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.250000f,0.554688f}, {-0.940700f,0.333800f,-0.060700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,-0.187500f,0.562500f}, {-0.940700f,0.333800f,-0.060700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,-0.171875f,0.500000f}, {-0.940700f,0.333800f,-0.060700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.093750f,0.468750f}, {-0.176100f,-0.880500f,-0.440200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,-0.171875f,0.500000f}, {-0.176100f,-0.880500f,-0.440200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,-0.187500f,0.562500f}, {-0.176100f,-0.880500f,-0.440200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.140625f,0.531250f}, {-0.176100f,-0.880500f,-0.440200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.343750f,-0.148438f,-0.539062f}, {-0.370800f,-0.473300f,-0.799100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.054688f,-0.664062f}, {-0.370800f,-0.473300f,-0.799100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.070312f,-0.828125f}, {-0.370800f,-0.473300f,-0.799100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.195312f,-0.671875f}, {-0.370800f,-0.473300f,-0.799100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.296875f,-0.312500f,-0.265625f}, {-0.310700f,-0.828400f,-0.466000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.343750f,-0.148438f,-0.539062f}, {-0.310700f,-0.828400f,-0.466000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.195312f,-0.671875f}, {-0.310700f,-0.828400f,-0.466000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.382812f,-0.351562f}, {-0.310700f,-0.828400f,-0.466000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.390625f,0.164062f}, {-0.279300f,-0.951500f,-0.128700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.296875f,-0.312500f,-0.265625f}, {-0.279300f,-0.951500f,-0.128700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.382812f,-0.351562f}, {-0.279300f,-0.951500f,-0.128700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.460938f,0.187500f}, {-0.279300f,-0.951500f,-0.128700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.414062f,0.257812f}, {-0.313900f,-0.932100f,-0.180700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.390625f,0.164062f}, {-0.313900f,-0.932100f,-0.180700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.460938f,0.187500f}, {-0.313900f,-0.932100f,-0.180700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,-0.484375f,0.281250f}, {-0.313900f,-0.932100f,-0.180700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.390625f,0.164062f}, {-0.976200f,-0.208300f,-0.060900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.179688f,-0.414062f,0.257812f}, {-0.976200f,-0.208300f,-0.060900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.234375f,-0.351562f,0.406250f}, {-0.976200f,-0.208300f,-0.060900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.218750f,-0.281250f,0.429688f}, {-0.976200f,-0.208300f,-0.060900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.734375f,-0.046875f,0.070312f}, {-0.826700f,-0.506600f,0.244700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.000000f,0.414062f}, {-0.826700f,-0.506600f,0.244700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.164062f,0.375000f}, {-0.826700f,-0.506600f,0.244700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.234375f,0.054688f}, {-0.826700f,-0.506600f,0.244700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.054688f,-0.664062f}, {-0.344900f,-0.115800f,-0.931500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.437500f,-0.703125f}, {-0.344900f,-0.115800f,-0.931500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.562500f,-0.851562f}, {-0.344900f,-0.115800f,-0.931500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.070312f,-0.828125f}, {-0.344900f,-0.115800f,-0.931500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.929688f,-0.070312f}, {-0.120300f,0.964400f,0.235500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.851562f,0.234375f}, {-0.120300f,0.964400f,0.235500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.898438f,0.289062f}, {-0.120300f,0.964400f,0.235500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.984375f,-0.078125f}, {-0.120300f,0.964400f,0.235500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.867188f,-0.382812f}, {-0.127500f,0.974400f,-0.185100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.929688f,-0.070312f}, {-0.127500f,0.974400f,-0.185100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.984375f,-0.078125f}, {-0.127500f,0.974400f,-0.185100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.898438f,-0.546875f}, {-0.127500f,0.974400f,-0.185100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.437500f,-0.703125f}, {-0.349200f,0.594700f,-0.724100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.867188f,-0.382812f}, {-0.349200f,0.594700f,-0.724100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.898438f,-0.546875f}, {-0.349200f,0.594700f,-0.724100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.562500f,-0.851562f}, {-0.349200f,0.594700f,-0.724100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.406250f,0.335938f}, {-0.415300f,0.898100f,-0.144900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.406250f,0.460938f}, {-0.415300f,0.898100f,-0.144900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.679688f,0.453125f,0.492188f}, {-0.415300f,0.898100f,-0.144900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,0.453125f,0.281250f}, {-0.415300f,0.898100f,-0.144900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.562500f,0.125000f}, {-0.184500f,0.703600f,0.686300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.406250f,0.335938f}, {-0.184500f,0.703600f,0.686300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,0.453125f,0.281250f}, {-0.184500f,0.703600f,0.686300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.703125f,0.054688f}, {-0.184500f,0.703600f,0.686300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.617188f,-0.117188f}, {-0.605600f,0.779400f,0.160800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.562500f,0.125000f}, {-0.605600f,0.779400f,0.160800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.703125f,0.054688f}, {-0.605600f,0.779400f,0.160800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.750000f,-0.195312f}, {-0.605600f,0.779400f,0.160800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.539062f,-0.359375f}, {-0.703300f,0.680600f,-0.205300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.617188f,-0.117188f}, {-0.703300f,0.680600f,-0.205300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.750000f,-0.195312f}, {-0.703300f,0.680600f,-0.205300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.679688f,-0.445312f}, {-0.703300f,0.680600f,-0.205300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.679688f,-0.445312f}, {-0.667900f,0.200700f,-0.716600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.328125f,-0.585938f}, {-0.667900f,0.200700f,-0.716600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.265625f,-0.437500f}, {-0.667900f,0.200700f,-0.716600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.539062f,-0.359375f}, {-0.667900f,0.200700f,-0.716600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.679688f,-0.445312f}, {-0.494800f,0.434200f,-0.752800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.867188f,-0.382812f}, {-0.494800f,0.434200f,-0.752800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.437500f,-0.703125f}, {-0.494800f,0.434200f,-0.752800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.328125f,-0.585938f}, {-0.494800f,0.434200f,-0.752800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.750000f,-0.195312f}, {-0.642300f,0.745900f,-0.176100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.929688f,-0.070312f}, {-0.642300f,0.745900f,-0.176100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.867188f,-0.382812f}, {-0.642300f,0.745900f,-0.176100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.679688f,-0.445312f}, {-0.642300f,0.745900f,-0.176100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.703125f,0.054688f}, {-0.718200f,0.678800f,0.153000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.851562f,0.234375f}, {-0.718200f,0.678800f,0.153000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.929688f,-0.070312f}, {-0.718200f,0.678800f,0.153000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.750000f,-0.195312f}, {-0.718200f,0.678800f,0.153000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,0.453125f,0.281250f}, {-0.738800f,0.397200f,0.544400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.523438f,0.429688f}, {-0.738800f,0.397200f,0.544400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.851562f,0.234375f}, {-0.738800f,0.397200f,0.544400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,0.703125f,0.054688f}, {-0.738800f,0.397200f,0.544400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.632812f,0.453125f,0.281250f}, {-0.342800f,0.926100f,-0.157900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.679688f,0.453125f,0.492188f}, {-0.342800f,0.926100f,-0.157900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.554688f,0.554688f}, {-0.342800f,0.926100f,-0.157900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.523438f,0.429688f}, {-0.342800f,0.926100f,-0.157900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.453125f,0.851562f,0.234375f}, {-0.227000f,0.574000f,0.786700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.523438f,0.429688f}, {-0.227000f,0.574000f,0.786700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.570312f,0.570312f}, {-0.227000f,0.574000f,0.786700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.898438f,0.289062f}, {-0.227000f,0.574000f,0.786700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.687500f,0.593750f}, {0.172200f,0.104600f,-0.979500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.195312f,0.664062f,0.617188f}, {0.172200f,0.104600f,-0.979500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,0.460938f,0.609375f}, {0.172200f,0.104600f,-0.979500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.554688f,0.554688f}, {0.172200f,0.104600f,-0.979500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.523438f,0.429688f}, {-0.042500f,0.915000f,0.401300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.554688f,0.554688f}, {-0.042500f,0.915000f,0.401300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,0.460938f,0.609375f}, {-0.042500f,0.915000f,0.401300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.570312f,0.570312f}, {-0.042500f,0.915000f,0.401300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.570312f,0.570312f}, {0.161600f,0.184700f,0.969400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.109375f,0.460938f,0.609375f}, {0.161600f,0.184700f,0.969400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {0.000000f,0.406250f,0.601562f}, {0.161600f,0.184700f,0.969400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.234375f,0.054688f}, {-0.979100f,0.197300f,0.048300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.164062f,0.375000f}, {-0.979100f,0.197300f,0.048300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.406250f,0.460938f}, {-0.979100f,0.197300f,0.048300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.406250f,0.335938f}, {-0.979100f,0.197300f,0.048300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.562500f,0.125000f}, {-0.947000f,0.091800f,0.307900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.320312f,-0.046875f}, {-0.947000f,0.091800f,0.307900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.234375f,0.054688f}, {-0.947000f,0.091800f,0.307900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.406250f,0.335938f}, {-0.947000f,0.091800f,0.307900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.617188f,-0.117188f}, {-0.979400f,0.190500f,-0.066100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.328125f,-0.203125f}, {-0.979400f,0.190500f,-0.066100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.320312f,-0.046875f}, {-0.979400f,0.190500f,-0.066100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.562500f,0.125000f}, {-0.979400f,0.190500f,-0.066100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.617188f,-0.117188f}, {-0.993800f,0.031200f,-0.107000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.539062f,-0.359375f}, {-0.993800f,0.031200f,-0.107000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.265625f,-0.437500f}, {-0.993800f,0.031200f,-0.107000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.328125f,-0.203125f}, {-0.993800f,0.031200f,-0.107000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,-0.195312f,-0.210938f}, {-0.711600f,-0.700800f,0.050100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.296875f,-0.312500f,-0.265625f}, {-0.711600f,-0.700800f,0.050100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.390625f,0.164062f}, {-0.711600f,-0.700800f,0.050100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.406250f,-0.171875f,0.148438f}, {-0.711600f,-0.700800f,0.050100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,-0.195312f,-0.210938f}, {-0.372200f,-0.924300f,0.084700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.406250f,-0.171875f,0.148438f}, {-0.372200f,-0.924300f,0.084700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.734375f,-0.046875f,0.070312f}, {-0.372200f,-0.924300f,0.084700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.593750f,-0.125000f,-0.164062f}, {-0.372200f,-0.924300f,0.084700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.406250f,-0.171875f,0.148438f}, {-0.446500f,-0.864400f,0.231000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.093750f,0.468750f}, {-0.446500f,-0.864400f,0.231000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.601562f,0.000000f,0.414062f}, {-0.446500f,-0.864400f,0.231000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.734375f,-0.046875f,0.070312f}, {-0.446500f,-0.864400f,0.231000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.218750f,-0.281250f,0.429688f}, {-0.606600f,-0.757800f,0.240500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.226562f,0.468750f}, {-0.606600f,-0.757800f,0.240500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.093750f,0.468750f}, {-0.606600f,-0.757800f,0.240500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.406250f,-0.171875f,0.148438f}, {-0.606600f,-0.757800f,0.240500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.406250f,-0.171875f,0.148438f}, {-0.732500f,-0.636800f,0.240700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.390625f,0.164062f}, {-0.732500f,-0.636800f,0.240700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.218750f,-0.281250f,0.429688f}, {-0.732500f,-0.636800f,0.240700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.210938f,-0.226562f,0.468750f}, {-0.263700f,-0.449900f,0.853300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.203125f,-0.171875f,0.500000f}, {-0.263700f,-0.449900f,0.853300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.437500f,-0.093750f,0.468750f}, {-0.263700f,-0.449900f,0.853300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.023438f,-0.546875f}, {-0.556800f,-0.318100f,-0.767300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,-0.007812f,-0.429688f}, {-0.556800f,-0.318100f,-0.767300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.265625f,-0.437500f}, {-0.556800f,-0.318100f,-0.767300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.328125f,-0.585938f}, {-0.556800f,-0.318100f,-0.767300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.023438f,-0.546875f}, {-0.500400f,-0.280700f,-0.819000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.617188f,0.328125f,-0.585938f}, {-0.500400f,-0.280700f,-0.819000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.460938f,0.437500f,-0.703125f}, {-0.500400f,-0.280700f,-0.819000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.054688f,-0.664062f}, {-0.500400f,-0.280700f,-0.819000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.023438f,-0.546875f}, {-0.319000f,-0.849400f,-0.420500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,-0.195312f,-0.210938f}, {-0.319000f,-0.849400f,-0.420500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.593750f,-0.125000f,-0.164062f}, {-0.319000f,-0.849400f,-0.420500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,-0.007812f,-0.429688f}, {-0.319000f,-0.849400f,-0.420500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.023438f,-0.546875f}, {-0.719800f,-0.635600f,-0.279300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.343750f,-0.148438f,-0.539062f}, {-0.719800f,-0.635600f,-0.279300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.296875f,-0.312500f,-0.265625f}, {-0.719800f,-0.635600f,-0.279300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.429688f,-0.195312f,-0.210938f}, {-0.719800f,-0.635600f,-0.279300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.343750f,-0.148438f,-0.539062f}, {-0.497200f,-0.440800f,-0.747300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.484375f,0.023438f,-0.546875f}, {-0.497200f,-0.440800f,-0.747300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.335938f,0.054688f,-0.664062f}, {-0.497200f,-0.440800f,-0.747300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.921875f,0.359375f,-0.218750f}, {-0.350600f,0.380700f,0.855700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.406250f,-0.234375f}, {-0.350600f,0.380700f,0.855700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.476562f,-0.312500f}, {-0.350600f,0.380700f,0.855700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.414062f,-0.289062f}, {-0.350600f,0.380700f,0.855700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.437500f,-0.390625f}, {-0.456600f,0.171500f,0.873000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.414062f,-0.289062f}, {-0.456600f,0.171500f,0.873000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.476562f,-0.312500f}, {-0.456600f,0.171500f,0.873000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.507812f,-0.421875f}, {-0.456600f,0.171500f,0.873000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.265625f,0.289062f,-0.406250f}, {-0.258300f,0.105500f,0.960300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.437500f,-0.390625f}, {-0.258300f,0.105500f,0.960300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.507812f,-0.421875f}, {-0.258300f,0.105500f,0.960300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.351562f,0.320312f,-0.421875f}, {-0.258300f,0.105500f,0.960300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.078125f,-0.406250f}, {-0.245500f,-0.080200f,0.966100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.265625f,0.289062f,-0.406250f}, {-0.245500f,-0.080200f,0.966100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.351562f,0.320312f,-0.421875f}, {-0.245500f,-0.080200f,0.966100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.281250f,0.054688f,-0.429688f}, {-0.245500f,-0.080200f,0.966100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.031250f,-0.039062f,-0.304688f}, {-0.464300f,-0.059900f,0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.078125f,-0.406250f}, {-0.464300f,-0.059900f,0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.281250f,0.054688f,-0.429688f}, {-0.464300f,-0.059900f,0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.101562f,-0.328125f}, {-0.464300f,-0.059900f,0.883700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,-0.070312f,-0.132812f}, {-0.622500f,-0.304500f,0.721000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.031250f,-0.039062f,-0.304688f}, {-0.622500f,-0.304500f,0.721000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.101562f,-0.328125f}, {-0.622500f,-0.304500f,0.721000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,-0.140625f,-0.125000f}, {-0.622500f,-0.304500f,0.721000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.023438f,-0.210938f}, {-0.450000f,0.659000f,0.602700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.000000f,-0.367188f}, {-0.450000f,0.659000f,0.602700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.031250f,-0.039062f,-0.304688f}, {-0.450000f,0.659000f,0.602700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,-0.070312f,-0.132812f}, {-0.450000f,0.659000f,0.602700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.000000f,-0.367188f}, {0.266700f,0.830900f,0.488400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.093750f,-0.445312f}, {0.266700f,0.830900f,0.488400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.078125f,-0.406250f}, {0.266700f,0.830900f,0.488400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.031250f,-0.039062f,-0.304688f}, {0.266700f,0.830900f,0.488400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.093750f,-0.445312f}, {0.828400f,0.229100f,0.511100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.250000f,-0.445312f}, {0.828400f,0.229100f,0.511100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.265625f,0.289062f,-0.406250f}, {0.828400f,0.229100f,0.511100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.078125f,-0.406250f}, {0.828400f,0.229100f,0.511100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.250000f,-0.445312f}, {0.525100f,-0.356600f,0.772700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.171875f,0.359375f,-0.437500f}, {0.525100f,-0.356600f,0.772700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.437500f,-0.390625f}, {0.525100f,-0.356600f,0.772700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.265625f,0.289062f,-0.406250f}, {0.525100f,-0.356600f,0.772700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.171875f,0.359375f,-0.437500f}, {-0.454600f,-0.566500f,0.687300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.343750f,-0.359375f}, {-0.454600f,-0.566500f,0.687300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.414062f,-0.289062f}, {-0.454600f,-0.566500f,0.687300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.437500f,-0.390625f}, {-0.454600f,-0.566500f,0.687300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.945312f,0.304688f,-0.289062f}, {-0.699600f,-0.449700f,0.555200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.921875f,0.359375f,-0.218750f}, {-0.699600f,-0.449700f,0.555200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.414062f,-0.289062f}, {-0.699600f,-0.449700f,0.555200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.343750f,-0.359375f}, {-0.699600f,-0.449700f,0.555200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,-0.023438f,-0.171875f}, {-0.722000f,-0.682700f,-0.112600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.593750f,-0.125000f,-0.164062f}, {-0.722000f,-0.682700f,-0.112600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.734375f,-0.046875f,0.070312f}, {-0.722000f,-0.682700f,-0.112600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.000000f,-0.070312f}, {-0.722000f,-0.682700f,-0.112600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,-0.070312f,-0.132812f}, {0.191900f,0.286000f,0.938800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,-0.140625f,-0.125000f}, {0.191900f,0.286000f,0.938800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.593750f,-0.125000f,-0.164062f}, {0.191900f,0.286000f,0.938800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,-0.023438f,-0.171875f}, {0.191900f,0.286000f,0.938800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.000000f,-0.070312f}, {-0.904800f,-0.373400f,-0.204700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.734375f,-0.046875f,0.070312f}, {-0.904800f,-0.373400f,-0.204700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.234375f,0.054688f}, {-0.904800f,-0.373400f,-0.204700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.320312f,-0.046875f}, {-0.904800f,-0.373400f,-0.204700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.921875f,0.359375f,-0.218750f}, {-0.103400f,0.155100f,0.982500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.289062f,-0.210938f}, {-0.103400f,0.155100f,0.982500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.328125f,-0.203125f}, {-0.103400f,0.155100f,0.982500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.406250f,-0.234375f}, {-0.103400f,0.155100f,0.982500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.812500f,-0.015625f,-0.273438f}, {-0.084100f,0.931800f,0.353000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.023438f,-0.210938f}, {-0.084100f,0.931800f,0.353000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,-0.070312f,-0.132812f}, {-0.084100f,0.931800f,0.353000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,-0.023438f,-0.171875f}, {-0.084100f,0.931800f,0.353000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,-0.023438f,-0.171875f}, {-0.644600f,-0.088300f,0.759400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,0.039062f,-0.187500f}, {-0.644600f,-0.088300f,0.759400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.015625f,-0.273438f}, {-0.644600f,-0.088300f,0.759400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.812500f,-0.015625f,-0.273438f}, {-0.644600f,-0.088300f,0.759400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.015625f,-0.273438f}, {-0.430900f,0.474000f,0.767800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,0.039062f,-0.187500f}, {-0.430900f,0.474000f,0.767800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.757812f,0.093750f,-0.273438f}, {-0.430900f,0.474000f,0.767800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.085938f,-0.273438f}, {-0.430900f,0.474000f,0.767800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,0.039062f,-0.187500f}, {-0.803200f,-0.484700f,0.346200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.203125f,-0.210938f}, {-0.803200f,-0.484700f,0.346200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.835938f,0.171875f,-0.273438f}, {-0.803200f,-0.484700f,0.346200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.757812f,0.093750f,-0.273438f}, {-0.803200f,-0.484700f,0.346200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.835938f,0.171875f,-0.273438f}, {-0.581100f,-0.412800f,0.701400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.203125f,-0.210938f}, {-0.581100f,-0.412800f,0.701400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.289062f,-0.210938f}, {-0.581100f,-0.412800f,0.701400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.242188f,-0.265625f}, {-0.581100f,-0.412800f,0.701400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.242188f,-0.265625f}, {-0.591000f,-0.430500f,0.682200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.289062f,-0.210938f}, {-0.591000f,-0.430500f,0.682200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.921875f,0.359375f,-0.218750f}, {-0.591000f,-0.430500f,0.682200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.945312f,0.304688f,-0.289062f}, {-0.591000f,-0.430500f,0.682200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.289062f,-0.210938f}, {-0.981800f,-0.180400f,-0.059100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.203125f,-0.210938f}, {-0.981800f,-0.180400f,-0.059100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.320312f,-0.046875f}, {-0.981800f,-0.180400f,-0.059100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.328125f,-0.203125f}, {-0.981800f,-0.180400f,-0.059100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,0.039062f,-0.187500f}, {-0.910500f,-0.396500f,-0.117500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.000000f,-0.070312f}, {-0.910500f,-0.396500f,-0.117500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.320312f,-0.046875f}, {-0.910500f,-0.396500f,-0.117500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.796875f,0.203125f,-0.210938f}, {-0.910500f,-0.396500f,-0.117500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,0.039062f,-0.187500f}, {-0.997200f,-0.018100f,-0.072500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.718750f,-0.023438f,-0.171875f}, {-0.997200f,-0.018100f,-0.072500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.726562f,0.000000f,-0.070312f}, {-0.997200f,-0.018100f,-0.072500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.234375f,-0.320312f}, {-0.731300f,-0.654300f,0.192500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.242188f,-0.265625f}, {-0.731300f,-0.654300f,0.192500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.945312f,0.304688f,-0.289062f}, {-0.731300f,-0.654300f,0.192500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.953125f,0.289062f,-0.343750f}, {-0.731300f,-0.654300f,0.192500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.171875f,-0.320312f}, {-0.786700f,-0.607900f,0.107300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.835938f,0.171875f,-0.273438f}, {-0.786700f,-0.607900f,0.107300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.242188f,-0.265625f}, {-0.786700f,-0.607900f,0.107300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.234375f,-0.320312f}, {-0.786700f,-0.607900f,0.107300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.765625f,0.093750f,-0.320312f}, {-0.702200f,-0.702200f,0.117000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.757812f,0.093750f,-0.273438f}, {-0.702200f,-0.702200f,0.117000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.835938f,0.171875f,-0.273438f}, {-0.702200f,-0.702200f,0.117000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.171875f,-0.320312f}, {-0.702200f,-0.702200f,0.117000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.078125f,-0.320312f}, {-0.184000f,0.981600f,-0.051100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.085938f,-0.273438f}, {-0.184000f,0.981600f,-0.051100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.757812f,0.093750f,-0.273438f}, {-0.184000f,0.981600f,-0.051100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.765625f,0.093750f,-0.320312f}, {-0.184000f,0.981600f,-0.051100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.015625f,-0.320312f}, {-0.935200f,0.330100f,0.128400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.015625f,-0.273438f}, {-0.935200f,0.330100f,0.128400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.085938f,-0.273438f}, {-0.935200f,0.330100f,0.128400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.078125f,-0.320312f}, {-0.935200f,0.330100f,0.128400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.812500f,-0.015625f,-0.320312f}, {-0.663300f,-0.746300f,0.055300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.812500f,-0.015625f,-0.273438f}, {-0.663300f,-0.746300f,0.055300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.015625f,-0.273438f}, {-0.663300f,-0.746300f,0.055300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.015625f,-0.320312f}, {-0.663300f,-0.746300f,0.055300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.015625f,-0.265625f}, {0.008500f,0.997000f,0.076700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.023438f,-0.210938f}, {0.008500f,0.997000f,0.076700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.812500f,-0.015625f,-0.273438f}, {0.008500f,0.997000f,0.076700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.812500f,-0.015625f,-0.320312f}, {0.008500f,0.997000f,0.076700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.953125f,0.289062f,-0.343750f}, {-0.623700f,-0.706100f,0.335400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.945312f,0.304688f,-0.289062f}, {-0.623700f,-0.706100f,0.335400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.343750f,-0.359375f}, {-0.623700f,-0.706100f,0.335400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.328125f,-0.414062f}, {-0.623700f,-0.706100f,0.335400f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.328125f,-0.414062f}, {-0.273300f,-0.892500f,0.358700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.343750f,-0.359375f}, {-0.273300f,-0.892500f,0.358700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.171875f,0.359375f,-0.437500f}, {-0.273300f,-0.892500f,0.358700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.343750f,-0.484375f}, {-0.273300f,-0.892500f,0.358700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.343750f,-0.484375f}, {0.832800f,-0.508000f,-0.220000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.171875f,0.359375f,-0.437500f}, {0.832800f,-0.508000f,-0.220000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.250000f,-0.445312f}, {0.832800f,-0.508000f,-0.220000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.257812f,0.242188f,-0.492188f}, {0.832800f,-0.508000f,-0.220000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.257812f,0.242188f,-0.492188f}, {0.833900f,0.237700f,-0.498100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.250000f,-0.445312f}, {0.833900f,0.237700f,-0.498100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.093750f,-0.445312f}, {0.833900f,0.237700f,-0.498100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.085938f,-0.484375f}, {0.833900f,0.237700f,-0.498100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.085938f,-0.484375f}, {0.565500f,0.784700f,-0.253900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.093750f,-0.445312f}, {0.565500f,0.784700f,-0.253900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.000000f,-0.367188f}, {0.565500f,0.784700f,-0.253900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.046875f,0.000000f,-0.421875f}, {0.565500f,0.784700f,-0.253900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.046875f,0.000000f,-0.421875f}, {0.056000f,0.996200f,0.067200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.000000f,-0.367188f}, {0.056000f,0.996200f,0.067200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.023438f,-0.210938f}, {0.056000f,0.996200f,0.067200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.015625f,-0.265625f}, {0.056000f,0.996200f,0.067200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.937500f,0.062500f,-0.335938f}, {-0.144500f,0.022200f,0.989300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.015625f,-0.320312f}, {-0.144500f,0.022200f,0.989300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.078125f,-0.320312f}, {-0.144500f,0.022200f,0.989300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.109375f,-0.328125f}, {-0.144500f,0.022200f,0.989300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.000000f,0.125000f,-0.367188f}, {-0.327500f,0.064500f,0.942700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.937500f,0.062500f,-0.335938f}, {-0.327500f,0.064500f,0.942700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.109375f,-0.328125f}, {-0.327500f,0.064500f,0.942700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.960938f,0.171875f,-0.351562f}, {-0.327500f,0.064500f,0.942700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.054688f,0.187500f,-0.382812f}, {-0.312700f,0.023200f,0.949600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.000000f,0.125000f,-0.367188f}, {-0.312700f,0.023200f,0.949600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.960938f,0.171875f,-0.351562f}, {-0.312700f,0.023200f,0.949600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.234375f,-0.375000f}, {-0.312700f,0.023200f,0.949600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.109375f,0.210938f,-0.390625f}, {-0.171000f,0.027400f,0.984900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.054688f,0.187500f,-0.382812f}, {-0.171000f,0.027400f,0.984900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.234375f,-0.375000f}, {-0.171000f,0.027400f,0.984900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.085938f,0.273438f,-0.390625f}, {-0.171000f,0.027400f,0.984900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.234375f,-0.375000f}, {-0.348700f,0.284900f,0.892900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.953125f,0.289062f,-0.343750f}, {-0.348700f,0.284900f,0.892900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.328125f,-0.414062f}, {-0.348700f,0.284900f,0.892900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.085938f,0.273438f,-0.390625f}, {-0.348700f,0.284900f,0.892900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.015625f,0.234375f,-0.375000f}, {-0.400600f,-0.034300f,0.915600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.960938f,0.171875f,-0.351562f}, {-0.400600f,-0.034300f,0.915600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.234375f,-0.320312f}, {-0.400600f,-0.034300f,0.915600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.953125f,0.289062f,-0.343750f}, {-0.400600f,-0.034300f,0.915600f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.109375f,-0.328125f}, {-0.257200f,-0.060300f,0.964500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.171875f,-0.320312f}, {-0.257200f,-0.060300f,0.964500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.234375f,-0.320312f}, {-0.257200f,-0.060300f,0.964500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.960938f,0.171875f,-0.351562f}, {-0.257200f,-0.060300f,0.964500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.843750f,0.171875f,-0.320312f}, {-0.063700f,-0.010600f,0.997900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.109375f,-0.328125f}, {-0.063700f,-0.010600f,0.997900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.828125f,0.078125f,-0.320312f}, {-0.063700f,-0.010600f,0.997900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.765625f,0.093750f,-0.320312f}, {-0.063700f,-0.010600f,0.997900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.937500f,0.062500f,-0.335938f}, {0.363700f,0.703900f,0.610100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.015625f,-0.265625f}, {0.363700f,0.703900f,0.610100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.812500f,-0.015625f,-0.320312f}, {0.363700f,0.703900f,0.610100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.851562f,0.015625f,-0.320312f}, {0.363700f,0.703900f,0.610100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.937500f,0.062500f,-0.335938f}, {-0.629900f,0.035500f,0.775900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.000000f,0.125000f,-0.367188f}, {-0.629900f,0.035500f,0.775900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.046875f,0.000000f,-0.421875f}, {-0.629900f,0.035500f,0.775900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.882812f,-0.015625f,-0.265625f}, {-0.629900f,0.035500f,0.775900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.000000f,0.125000f,-0.367188f}, {-0.447200f,-0.200200f,0.871700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.054688f,0.187500f,-0.382812f}, {-0.447200f,-0.200200f,0.871700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.085938f,-0.484375f}, {-0.447200f,-0.200200f,0.871700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.046875f,0.000000f,-0.421875f}, {-0.447200f,-0.200200f,0.871700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.054688f,0.187500f,-0.382812f}, {-0.507200f,-0.214100f,0.834800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.109375f,0.210938f,-0.390625f}, {-0.507200f,-0.214100f,0.834800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.257812f,0.242188f,-0.492188f}, {-0.507200f,-0.214100f,0.834800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.210938f,0.085938f,-0.484375f}, {-0.507200f,-0.214100f,0.834800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.109375f,0.210938f,-0.390625f}, {-0.525800f,0.261900f,0.809300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.085938f,0.273438f,-0.390625f}, {-0.525800f,0.261900f,0.809300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.343750f,-0.484375f}, {-0.525800f,0.261900f,0.809300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.257812f,0.242188f,-0.492188f}, {-0.525800f,0.261900f,0.809300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.187500f,0.343750f,-0.484375f}, {-0.298000f,0.580200f,0.758000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.085938f,0.273438f,-0.390625f}, {-0.298000f,0.580200f,0.758000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,0.328125f,-0.414062f}, {-0.298000f,0.580200f,0.758000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.789062f,-0.125000f,-0.328125f}, {-0.093000f,-0.992400f,-0.080500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,-0.140625f,-0.125000f}, {-0.093000f,-0.992400f,-0.080500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.101562f,-0.328125f}, {-0.093000f,-0.992400f,-0.080500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.085938f,-0.492188f}, {-0.093000f,-0.992400f,-0.080500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.085938f,-0.492188f}, {-0.500600f,-0.865700f,0.008000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.101562f,-0.328125f}, {-0.500600f,-0.865700f,0.008000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.281250f,0.054688f,-0.429688f}, {-0.500600f,-0.865700f,0.008000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.312500f,0.054688f,-0.531250f}, {-0.500600f,-0.865700f,0.008000f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.312500f,0.054688f,-0.531250f}, {-0.928500f,-0.249700f,0.274800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.281250f,0.054688f,-0.429688f}, {-0.928500f,-0.249700f,0.274800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.351562f,0.320312f,-0.421875f}, {-0.928500f,-0.249700f,0.274800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.367188f,0.296875f,-0.500000f}, {-0.928500f,-0.249700f,0.274800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.367188f,0.296875f,-0.500000f}, {-0.839300f,0.542400f,-0.037800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.351562f,0.320312f,-0.421875f}, {-0.839300f,0.542400f,-0.037800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.507812f,-0.421875f}, {-0.839300f,0.542400f,-0.037800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.250000f,0.468750f,-0.546875f}, {-0.839300f,0.542400f,-0.037800f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.250000f,0.468750f,-0.546875f}, {0.235500f,0.936700f,-0.258900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.234375f,0.507812f,-0.421875f}, {0.235500f,0.936700f,-0.258900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.476562f,-0.312500f}, {0.235500f,0.936700f,-0.258900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.437500f,-0.484375f}, {0.235500f,0.936700f,-0.258900f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.437500f,-0.484375f}, {0.449900f,0.883800f,-0.128500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.476562f,-0.312500f}, {0.449900f,0.883800f,-0.128500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.406250f,-0.234375f}, {0.449900f,0.883800f,-0.128500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.382812f,-0.382812f}, {0.449900f,0.883800f,-0.128500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.789062f,-0.125000f,-0.328125f}, {0.538400f,-0.009800f,-0.842700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.085938f,-0.492188f}, {0.538400f,-0.009800f,-0.842700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.437500f,-0.484375f}, {0.538400f,-0.009800f,-0.842700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.382812f,-0.382812f}, {0.538400f,-0.009800f,-0.842700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.312500f,0.054688f,-0.531250f}, {0.191000f,-0.024100f,-0.981300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.250000f,0.468750f,-0.546875f}, {0.191000f,-0.024100f,-0.981300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.023438f,0.437500f,-0.484375f}, {0.191000f,-0.024100f,-0.981300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.039062f,-0.085938f,-0.492188f}, {0.191000f,-0.024100f,-0.981300f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.312500f,0.054688f,-0.531250f}, {-0.404600f,0.026600f,-0.914100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.367188f,0.296875f,-0.500000f}, {-0.404600f,0.026600f,-0.914100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-1.250000f,0.468750f,-0.546875f}, {-0.404600f,0.026600f,-0.914100f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.890625f,0.406250f,-0.234375f}, {0.781900f,0.623100f,0.019700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.820312f,0.328125f,-0.203125f}, {0.781900f,0.623100f,0.019700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.265625f,-0.437500f}, {0.781900f,0.623100f,0.019700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.382812f,-0.382812f}, {0.781900f,0.623100f,0.019700f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.789062f,-0.125000f,-0.328125f}, {-0.542800f,-0.206300f,-0.814200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.859375f,0.382812f,-0.382812f}, {-0.542800f,-0.206300f,-0.814200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,0.265625f,-0.437500f}, {-0.542800f,-0.206300f,-0.814200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,-0.007812f,-0.429688f}, {-0.542800f,-0.206300f,-0.814200f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.789062f,-0.125000f,-0.328125f}, {0.247400f,-0.923100f,-0.294500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.640625f,-0.007812f,-0.429688f}, {0.247400f,-0.923100f,-0.294500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.593750f,-0.125000f,-0.164062f}, {0.247400f,-0.923100f,-0.294500f}, {1,1,1,1}, {0.000000f,0.000000f} }, { {-0.773438f,-0.140625f,-0.125000f}, {0.247400f,-0.923100f,-0.294500f}, {1,1,1,1}, {0.000000f,0.000000f} } }, .indices = (unsigned[]){ 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23, 24, 25, 26, 24, 26, 27, 28, 29, 30, 28, 30, 31, 32, 33, 34, 32, 34, 35, 36, 37, 38, 36, 38, 39, 40, 41, 42, 40, 42, 43, 44, 45, 46, 44, 46, 47, 48, 49, 50, 48, 50, 51, 52, 53, 54, 52, 54, 55, 56, 57, 58, 56, 58, 59, 60, 61, 62, 60, 62, 63, 64, 65, 66, 64, 66, 67, 68, 69, 70, 68, 70, 71, 72, 73, 74, 72, 74, 75, 76, 77, 78, 76, 78, 79, 80, 81, 82, 80, 82, 83, 84, 85, 86, 84, 86, 87, 88, 89, 90, 88, 90, 91, 92, 93, 94, 92, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 120, 122, 123, 124, 125, 126, 124, 126, 127, 128, 129, 130, 128, 130, 131, 132, 133, 134, 132, 134, 135, 136, 137, 138, 136, 138, 139, 140, 141, 142, 140, 142, 143, 144, 145, 146, 144, 146, 147, 148, 149, 150, 148, 150, 151, 152, 153, 154, 152, 154, 155, 156, 157, 158, 156, 158, 159, 160, 161, 162, 160, 162, 163, 164, 165, 166, 164, 166, 167, 168, 169, 170, 168, 170, 171, 172, 173, 174, 172, 174, 175, 176, 177, 178, 176, 178, 179, 180, 181, 182, 180, 182, 183, 184, 185, 186, 184, 186, 187, 188, 189, 190, 188, 190, 191, 192, 193, 194, 192, 194, 195, 196, 197, 198, 196, 198, 199, 200, 201, 202, 200, 202, 203, 204, 205, 206, 204, 206, 207, 208, 209, 210, 208, 210, 211, 212, 213, 214, 212, 214, 215, 216, 217, 218, 216, 218, 219, 220, 221, 222, 220, 222, 223, 224, 225, 226, 224, 226, 227, 228, 229, 230, 228, 230, 231, 232, 233, 234, 232, 234, 235, 236, 237, 238, 236, 238, 239, 240, 241, 242, 240, 242, 243, 244, 245, 246, 244, 246, 247, 248, 249, 250, 248, 250, 251, 252, 253, 254, 252, 254, 255, 256, 257, 258, 256, 258, 259, 260, 261, 262, 260, 262, 263, 264, 265, 266, 264, 266, 267, 268, 269, 270, 268, 270, 271, 272, 273, 274, 272, 274, 275, 276, 277, 278, 276, 278, 279, 280, 281, 282, 280, 282, 283, 284, 285, 286, 284, 286, 287, 288, 289, 290, 288, 290, 291, 292, 293, 294, 292, 294, 295, 296, 297, 298, 296, 298, 299, 300, 301, 302, 300, 302, 303, 304, 305, 306, 304, 306, 307, 308, 309, 310, 308, 310, 311, 312, 313, 314, 312, 314, 315, 316, 317, 318, 316, 318, 319, 320, 321, 322, 320, 322, 323, 324, 325, 326, 324, 326, 327, 328, 329, 330, 328, 330, 331, 332, 333, 334, 332, 334, 335, 336, 337, 338, 336, 338, 339, 340, 341, 342, 340, 342, 343, 344, 345, 346, 344, 346, 347, 348, 349, 350, 348, 350, 351, 352, 353, 354, 352, 354, 355, 356, 357, 358, 356, 358, 359, 360, 361, 362, 360, 362, 363, 364, 365, 366, 364, 366, 367, 368, 369, 370, 368, 370, 371, 372, 373, 374, 372, 374, 375, 376, 377, 378, 379, 380, 381, 379, 381, 382, 383, 384, 385, 383, 385, 386, 387, 388, 389, 387, 389, 390, 391, 392, 393, 391, 393, 394, 395, 396, 397, 395, 397, 398, 399, 400, 401, 399, 401, 402, 403, 404, 405, 403, 405, 406, 407, 408, 409, 407, 409, 410, 411, 412, 413, 411, 413, 414, 415, 416, 417, 415, 417, 418, 419, 420, 421, 419, 421, 422, 423, 424, 425, 423, 425, 426, 427, 428, 429, 427, 429, 430, 431, 432, 433, 431, 433, 434, 435, 436, 437, 435, 437, 438, 439, 440, 441, 439, 441, 442, 443, 444, 445, 443, 445, 446, 447, 448, 449, 447, 449, 450, 451, 452, 453, 451, 453, 454, 455, 456, 457, 455, 457, 458, 459, 460, 461, 459, 461, 462, 463, 464, 465, 463, 465, 466, 467, 468, 469, 467, 469, 470, 471, 472, 473, 471, 473, 474, 475, 476, 477, 475, 477, 478, 479, 480, 481, 479, 481, 482, 483, 484, 485, 483, 485, 486, 487, 488, 489, 487, 489, 490, 491, 492, 493, 491, 493, 494, 495, 496, 497, 495, 497, 498, 499, 500, 501, 499, 501, 502, 503, 504, 505, 503, 505, 506, 507, 508, 509, 507, 509, 510, 511, 512, 513, 511, 513, 514, 515, 516, 517, 515, 517, 518, 519, 520, 521, 519, 521, 522, 523, 524, 525, 523, 525, 526, 527, 528, 529, 527, 529, 530, 531, 532, 533, 531, 533, 534, 535, 536, 537, 535, 537, 538, 539, 540, 541, 539, 541, 542, 543, 544, 545, 543, 545, 546, 547, 548, 549, 547, 549, 550, 551, 552, 553, 551, 553, 554, 555, 556, 557, 555, 557, 558, 559, 560, 561, 559, 561, 562, 563, 564, 565, 563, 565, 566, 567, 568, 569, 567, 569, 570, 571, 572, 573, 571, 573, 574, 575, 576, 577, 575, 577, 578, 579, 580, 581, 579, 581, 582, 583, 584, 585, 583, 585, 586, 587, 588, 589, 587, 589, 590, 591, 592, 593, 591, 593, 594, 595, 596, 597, 595, 597, 598, 599, 600, 601, 599, 601, 602, 603, 604, 605, 603, 605, 606, 607, 608, 609, 607, 609, 610, 611, 612, 613, 611, 613, 614, 615, 616, 617, 615, 617, 618, 619, 620, 621, 619, 621, 622, 623, 624, 625, 623, 625, 626, 627, 628, 629, 627, 629, 630, 631, 632, 633, 631, 633, 634, 635, 636, 637, 635, 637, 638, 639, 640, 641, 639, 641, 642, 643, 644, 645, 643, 645, 646, 647, 648, 649, 647, 649, 650, 651, 652, 653, 651, 653, 654, 655, 656, 657, 655, 657, 658, 659, 660, 661, 659, 661, 662, 663, 664, 665, 663, 665, 666, 667, 668, 669, 667, 669, 670, 671, 672, 673, 674, 675, 676, 674, 676, 677, 678, 679, 680, 678, 680, 681, 682, 683, 684, 682, 684, 685, 686, 687, 688, 686, 688, 689, 690, 691, 692, 690, 692, 693, 694, 695, 696, 694, 696, 697, 698, 699, 700, 698, 700, 701, 702, 703, 704, 702, 704, 705, 706, 707, 708, 709, 710, 711, 712, 713, 714, 712, 714, 715, 716, 717, 718, 716, 718, 719, 720, 721, 722, 720, 722, 723, 724, 725, 726, 724, 726, 727, 728, 729, 730, 731, 732, 733, 731, 733, 734, 735, 736, 737, 735, 737, 738, 739, 740, 741, 739, 741, 742, 743, 744, 745, 743, 745, 746, 747, 748, 749, 747, 749, 750, 751, 752, 753, 751, 753, 754, 755, 756, 757, 755, 757, 758, 759, 760, 761, 759, 761, 762, 763, 764, 765, 763, 765, 766, 767, 768, 769, 767, 769, 770, 771, 772, 773, 771, 773, 774, 775, 776, 777, 775, 777, 778, 779, 780, 781, 779, 781, 782, 783, 784, 785, 783, 785, 786, 787, 788, 789, 787, 789, 790, 791, 792, 793, 791, 793, 794, 795, 796, 797, 795, 797, 798, 799, 800, 801, 799, 801, 802, 803, 804, 805, 803, 805, 806, 807, 808, 809, 807, 809, 810, 811, 812, 813, 811, 813, 814, 815, 816, 817, 815, 817, 818, 819, 820, 821, 819, 821, 822, 823, 824, 825, 823, 825, 826, 827, 828, 829, 830, 831, 832, 830, 832, 833, 834, 835, 836, 834, 836, 837, 838, 839, 840, 838, 840, 841, 842, 843, 844, 842, 844, 845, 846, 847, 848, 846, 848, 849, 850, 851, 852, 850, 852, 853, 854, 855, 856, 854, 856, 857, 858, 859, 860, 858, 860, 861, 862, 863, 864, 862, 864, 865, 866, 867, 868, 866, 868, 869, 870, 871, 872, 870, 872, 873, 874, 875, 876, 874, 876, 877, 878, 879, 880, 878, 880, 881, 882, 883, 884, 882, 884, 885, 886, 887, 888, 886, 888, 889, 890, 891, 892, 890, 892, 893, 894, 895, 896, 894, 896, 897, 898, 899, 900, 898, 900, 901, 902, 903, 904, 902, 904, 905, 906, 907, 908, 906, 908, 909, 910, 911, 912, 910, 912, 913, 914, 915, 916, 914, 916, 917, 918, 919, 920, 918, 920, 921, 922, 923, 924, 922, 924, 925, 926, 927, 928, 926, 928, 929, 930, 931, 932, 930, 932, 933, 934, 935, 936, 937, 938, 939, 937, 939, 940, 941, 942, 943, 941, 943, 944, 945, 946, 947, 945, 947, 948, 949, 950, 951, 949, 951, 952, 953, 954, 955, 953, 955, 956, 957, 958, 959, 957, 959, 960, 961, 962, 963, 961, 963, 964, 965, 966, 967, 965, 967, 968, 969, 970, 971, 972, 973, 974, 972, 974, 975, 976, 977, 978, 976, 978, 979, 980, 981, 982, 980, 982, 983}, .vertex_count    = 984, .index_count     = 1452, };

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
    glUniform1i(glGetUniformLocation(planet_shader.id, "u_flowTex"), 0);



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




