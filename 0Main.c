#define WIN32_LEAN_AND_MEAN
#include "defs.h"
#include <stdbool.h>
#include <stdio.h>
#include <glad/gl.h>
#include <glad/wgl.h>
#include <stdint.h>

HGLRC hRC;
HDC hDC;   // Device Context
HWND hWnd; // Window Handle
static int win_w = 800, win_h = 600;

GLuint compile_shader_from_file(const char* filepath, GLenum shader_type) {
    FILE* file = fopen(filepath, "rb");

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char* source = (char*)malloc((size_t)length + 1);

    fread(source, 1, (size_t)length, file);
    source[length] = '\0';
    fclose(file);

    const GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, (const char* const*)&source, NULL);
    glCompileShader(shader);
    free(source);

    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint logLen = 0; glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
        if (logLen > 1) { char* log = (char*)malloc(logLen); glGetShaderInfoLog(shader, logLen, NULL, log); fprintf(stderr, "SHADER COMPILE FAIL %s:\n%s\n", filepath, log); free(log); }
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

    glDeleteShader(vs);
    glDeleteShader(fs);

    ShaderProgram sp = {
        .id = prog,
        .u_model_loc = glGetUniformLocation(prog, "u_model"),
        .u_view_loc = glGetUniformLocation(prog, "u_view"),
        .u_projection_loc = glGetUniformLocation(prog, "u_projection"),
        .u_light_space_matrix_loc = glGetUniformLocation(prog, "u_light_space_matrix"),
        .u_material_albedo_loc = glGetUniformLocation(prog, "u_material_albedo"),
        .u_material_roughness_loc = glGetUniformLocation(prog, "u_material_roughness"),
        .u_material_metallic_loc = glGetUniformLocation(prog, "u_material_metallic"),
        .u_material_emissive_loc = glGetUniformLocation(prog, "u_material_emissive"),
        .u_flags_loc = glGetUniformLocation(prog, "u_flags")
    };

    return sp;
}


// OpenGL state
GLuint skybox_vao, skybox_vbo, skybox_ibo;
Mat4 light_space_matrix;


// Static GL objects

/*void compute_light_space_matrix() {
    Mat4 light_proj, light_view;
    Vec3 light_pos = {2, 4, 2}, target = {0, 0, 0}, up = {0, 1, 0};
    mat4_ortho(light_proj, -10, 10, -10, 10, 1.0f, 20.0f);
    mat4_lookat(light_view, light_pos, target, up);
    mat4_multiply(light_space_matrix, light_proj, light_view);
}*/
float aspectRatio = 8.0f/6.0f;

double playback = 1.0;
//bool lasers = false;

const Material* material_pool[MAX_MATERIALS];
Mesh* mesh_pool[256];
Vec3 pos = {0};      // Ship world position
Vec3 vel = {0};      // Ship linear velocity
Vec3 angVel = {0};   // Ship angular velocity (radians/sec)
Quat rot = {0, 0, 0, 1}; // Ship orientation
Vec2 view = {0, 0};  // Head pitch/yaw in radians
float dt = 0.016f;   // Frame time (roughly 60fps)


float RF = 0.0f;
float IR = 0.0f;
float VIS = 1.0f;
float RAD = 0.25f;

float dRF = 0.0f;
float dIR = 0.0f;
float dVIS = 0.00f;
float dRAD =0.0f;

void generate_asteroid_cubesphere(Mesh* mesh, Vec3 center, float radius, int subdivisions) {
    mesh->material_id = 1;
    if (subdivisions < 1) subdivisions = 1;

    const int faces = 6;
    int verts_per_face = (subdivisions + 1) * (subdivisions + 1);
    int estimated_vertices = verts_per_face * faces;
    int estimated_triangles = faces * subdivisions * subdivisions * 2;
    int estimated_indices = estimated_triangles * 3;

    init_mesh(mesh, estimated_vertices, estimated_indices);

    float step = 2.0f / subdivisions;

    for (int f = 0; f < 6; f++) {
        Vec3 face_normals[6] = {
            { 1, 0, 0 }, { -1, 0, 0 },
            { 0, 1, 0 }, {  0,-1, 0 },
            { 0, 0, 1 }, {  0, 0,-1 }
        };
        Vec3 normal = face_normals[f];
        Vec3 axis_a = {0}, axis_b = {0};
        if (fabsf(normal.x) > 0.5f) { axis_a.z = 1; axis_b.y = 1; }
        else if (fabsf(normal.y) > 0.5f) { axis_a.x = 1; axis_b.z = 1; }
        else { axis_a.x = 1; axis_b.y = 1; }

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

                // procedural radial deformation
                float base_shape   = fbm(dir, 3, 0.6f, 1.5f);
                float deform1      = 1.0f + base_shape * 0.7f;
                Vec3 bump_dir      = vec3_scale(dir, 3.0f);
                float surface_noise= fbm(bump_dir, 5, 0.5f, 2.5f);
                float deform2      = 1.0f + surface_noise * 0.25f;
                float deform       = deform1 * deform2;

                Vec3 final_pos = {
                    center.x + dir.x * radius * deform,
                    center.y + dir.y * radius * deform,
                    center.z + dir.z * radius * deform
                };

                // color tint (procedural)
                float base_gray   = 0.33f + base_shape * 0.08f;
                float brown_tint  = (fbm(vec3_scale(dir, 1.3f), 2, 0.5f, 2.0f) - 0.5f) * 0.12f;
                float blue_tint   = (1.0f - deform2) * fbm(vec3_scale(dir, 1.8f), 2, 0.5f, 2.0f) * 0.12f;

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

                int index = find_or_add_vertex(mesh, vert);
                vert_idx_grid[i][j] = index;
            }
        }

        int flip = (f == 0 || f == 2 || f == 5);
        for (int i = 0; i < subdivisions; i++) {
            for (int j = 0; j < subdivisions; j++) {
                int a = vert_idx_grid[i][j];
                int b = vert_idx_grid[i+1][j];
                int c = vert_idx_grid[i][j+1];
                int d = vert_idx_grid[i+1][j+1];

                if (mesh->index_count + 6 > mesh->index_capacity) {
                    mesh->index_capacity *= 2;
                    mesh->indices = (unsigned*)realloc(mesh->indices, sizeof(unsigned) * (size_t)mesh->index_capacity);
                }

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

void generate_gas_giant(Mesh* mesh, Vec3 center, float radius, int subdivisions) {
    mesh->material_id = 1;
    if (subdivisions < 1) subdivisions = 1;

    int faces = 6;
    int verts_per_face = (subdivisions + 1) * (subdivisions + 1) +1;
    int estimated_vertices = verts_per_face * faces;
    int estimated_triangles = faces * subdivisions * subdivisions * 2;
    int estimated_indices = estimated_triangles * 3;

    init_mesh(mesh, estimated_vertices, estimated_indices);

    float step = 2.0f / subdivisions;

    for (int f = 0; f < 6; f++) {
        Vec3 face_normals[6] = {
            { 1, 0, 0 }, { -1, 0, 0 },
            { 0, 1, 0 }, {  0,-1, 0 },
            { 0, 0, 1 }, {  0, 0,-1 }
        };
        const Vec3 normal = face_normals[f];
        Vec3 axis_a = {0}, axis_b = {0};
        if (fabsf(normal.x) > 0.5f) { axis_a.z = 1; axis_b.y = 1; }
        else if (fabsf(normal.y) > 0.5f) { axis_a.x = 1; axis_b.z = 1; }
        else { axis_a.x = 1; axis_b.y = 1; }

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
                    .normal   = dir,        // keep smooth shading for planet
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

void set_common_matrices(ShaderProgram* shader, const float* view, const float* projection) {
    glUseProgram(shader->id);
    glUniformMatrix4fv(shader->u_view_loc, 1, GL_FALSE, view);
    glUniformMatrix4fv(shader->u_projection_loc, 1, GL_FALSE, projection);
}

void set_common_uniforms(ShaderProgram* s,
                         const float* model,
                         const float* view,
                         const float* proj,
                         Vec3 cam_pos,
                         Vec3 light_dir,
                         const Material* mat,
                         uint32_t flags)
{
    glUseProgram(s->id);

    if (s->u_model_loc       != -1) glUniformMatrix4fv(s->u_model_loc,       1, GL_FALSE, model);
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
        if (s->u_material_emissive_loc  != -1) glUniform1f(s->u_material_emissive_loc,  mat->emissive);
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // 1. Register and create window
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "DeadVector";
    RegisterClass(&wc);
    hWnd = CreateWindow(wc.lpszClassName, "Dead Vector 2", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                             NULL, NULL, hInstance, NULL);
    hDC = GetDC(hWnd);

    // 2. Dummy context (to get modern WGL)
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24 };
    int pf = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pf, &pfd);
    HGLRC dummyRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, dummyRC);


    // 3. Load WGL extensions
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

    // 4. Load OpenGL functions

    glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    //glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);





    // 5. Projection matrix (identity here)
    float projection[16];
    float cview_rot[16];
    float cview_trans[16];
    float cview[16];
    float model[16];
    float pv[16];
    float mvp[16];


    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f};
    //thrust = 10.0f;

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
        .albedo = {1, 0, 0, 1}, .roughness = 0.5f, .metallic = 0.0f, .emissive = 0.0f
    };
    material_pool[0] = &test_material;

    static Material asteroid_material = {
        .albedo = {0.95f, 0.95f, 0.95f, 1.0f},
        .roughness = 0.4f,
        .metallic = 1.0f,
        .emissive = 0.0f
    };
    material_pool[1] = &asteroid_material;
    Material metal_material = {
        .albedo = {1.0f, 1.0f, 1.0f, 1.0f},
        .roughness = 0.1f,
        .metallic = 1.0f,
        .emissive = 0.0f,
    };
    material_pool[2] = &metal_material;
    Material glass_material = {
        .albedo = {1.0f, 1.0f, 1.0f, 0.9f},
        .roughness = 0.1f,
        .metallic = 1.0f,
        .emissive = 0.0f,
    };
    material_pool[3] = &glass_material;

    //Skybox generation
    float skyboxVertices[] = {
    -1, -1, -1,  1, -1, -1,  1,  1, -1, -1,  1, -1,
    -1, -1,  1,  1, -1,  1,  1,  1,  1, -1,  1,  1,
    };
    unsigned int skyboxIndices[] = {
        0, 1, 2, 2, 3, 0,  // -Z
        4, 5, 6, 6, 7, 4,  // +Z
        0, 4, 7, 7, 3, 0,  // -X
        1, 5, 6, 6, 2, 1,  // +X
        3, 2, 6, 6, 7, 3,  // +Y
        0, 1, 5, 5, 4, 0   // -Y
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
    Mesh cockpit_mesh;
    //init_mesh(&cockpit_mesh, 512, 2048);

    Object cockpit = {
        .position = {0.0f, 0.0f, 0.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh_id = 3,
        .flags = OBJ_FLAG_VISIBLE,
    };
        //make_box(&cockpit_mesh, (Vec3){-0.7f,-0.5f,-0.8f}, (Vec3){0.7f,0.5f,0.8f});

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
    while (running) {
        float thrust = 0.06f;
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

            test_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/asteroid.frag");
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
        quat_to_matrix(&viewQuat, cview_rot);

        Vec3 inv_pos = vec3_invert(pos);
        mat4_identity(cview_trans);
        mat4_translate(cview_trans, inv_pos.x, inv_pos.y, inv_pos.z);

        mat4_multiply(cview, cview_rot, cview_trans);

        mat4_perspective(projection, 90.0f, aspectRatio, 0.1f, 10000.0f);
        mat4_identity(model);
        mat4_multiply(pv, projection, cview);
        mat4_multiply(mvp, pv, model);



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



        Mat4 T,R,S,model;
        mat4_identity(T); mat4_translate(T, asteroid.position.x, asteroid.position.y, asteroid.position.z);
        quat_to_matrix(&asteroid.rotation, R);
        mat4_identity(S); mat4_scale(S, asteroid.scale.x, asteroid.scale.y, asteroid.scale.z);
        Mat4 TR; mat4_multiply(TR, T, R);
        mat4_multiply(model, TR, S);

        Vec3 light_dir = vec3_normalize((Vec3){2.0f,-4.0f,1.0f});
        Mesh* a_mesh = mesh_pool[asteroid.mesh_id];
        const Material* a_mat = (a_mesh && a_mesh->material_id >= 0 && a_mesh->material_id < MAX_MATERIALS)
                                ? material_pool[a_mesh->material_id] : 0;
        uint32_t a_flags = (a_mesh ? a_mesh->flags : 0) | (a_mat ? a_mat->flags : 0);

        set_common_uniforms(&univ_shader, model, cview, projection, pos, light_dir, a_mat, a_flags);
        glPointSize(1.0f);
        draw_object(&asteroid, &univ_shader, mesh_pool, GL_TRIANGLES);
        draw_object(&cockpit, &univ_shader, mesh_pool, GL_TRIANGLES);



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
        hud_draw_string(2, 2, "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");

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
            {
                RECT rect;
                GetClientRect(hWnd, &rect);
                POINT ul = {rect.left, rect.top};
                POINT lr = {rect.right, rect.bottom};
                ClientToScreen(hWnd, &ul);
                ClientToScreen(hWnd, &lr);
                RECT clipRect = {ul.x, ul.y, lr.x, lr.y};
                //ClipCursor(&clipRect);
            }
            return 0;

        case WM_SIZE: {
            win_w = LOWORD(lParam);
            win_h = HIWORD(lParam); if (!win_h) win_h = 1;
            glViewport(0, 0, win_w, win_h);
            aspectRatio = (float)win_w / (float)win_h;

            RECT rect;
            GetClientRect(hWnd, &rect);
            POINT ul = {rect.left, rect.top};
            POINT lr = {rect.right, rect.bottom};
            ClientToScreen(hWnd, &ul);
            ClientToScreen(hWnd, &lr);
            RECT clipRect = {ul.x, ul.y, lr.x, lr.y};
            //ClipCursor(&clipRect);
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





