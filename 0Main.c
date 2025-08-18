#include "defs.h"
#include <stdbool.h>
#include <stdio.h>
#include <glad/gl.h>
#include <glad/wgl.h>

HGLRC hRC;
HDC hDC;   // Device Context
HWND hWnd; // Window Handle


GLuint compile_shader_from_file(const char* filepath, GLenum shader_type) {
    FILE* file = fopen(filepath, "rb");
    if (!file) {
        perror("Couldn't open shader file");
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    rewind(file);

    char* source = malloc(length + 1);
    if (!source) {
        perror("Memory allocation failed");
        fclose(file);
        return 0;
    }

    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);

    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, (const char**)&source, NULL);
    glCompileShader(shader);
    free(source);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), NULL, info);
        fprintf(stderr, "Error compiling shader (%s):\n%s\n", filepath, info);
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}
ShaderProgram create_shader_program_from_files(const char* vert_path, const char* frag_path) {
    GLuint vs = compile_shader_from_file(vert_path, GL_VERTEX_SHADER);
    GLuint fs = compile_shader_from_file(frag_path, GL_FRAGMENT_SHADER);

    if (!vs || !fs) {
        fprintf(stderr, "Shader compile failed\n");
        ShaderProgram null = {0};
        return null;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    GLint linked;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        char info[512];
        glGetProgramInfoLog(prog, sizeof(info), NULL, info);
        fprintf(stderr, "Error linking shader program:\n%s\n", info);
        glDeleteProgram(prog);
        ShaderProgram null = {0};
        return null;
    }

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
        .u_material_emissive_loc = glGetUniformLocation(prog, "u_material_emissive")
    };

    return sp;
}


// OpenGL state
GLuint shadow_fbo, shadow_texture;
ShaderProgram shadow_shader, main_shader;
GLuint skybox_vao, skybox_vbo, skybox_ibo;
Mat4 light_space_matrix;

#include <glad/gl.h>
#include <stdlib.h>
#include <string.h>

// Static GL objects
static GLuint hud_vao = 0, hud_vbo = 0, hud_prog = 0, hud_tex = 0;
static GLint  u_screen_size = -1, u_pos_size = -1, u_sampler0 = -1;

static const char* HUD_VS =
"#version 330 core\n"
"layout(location=0) in vec2 in_pos;\n"
"layout(location=1) in vec2 in_uv;\n"
"uniform vec2 u_screen_size; // pixels\n"
"uniform vec4 u_pos_size;    // x,y,w,h in pixels\n"
"out vec2 v_uv;\n"
"void main(){\n"
"  vec2 p = u_pos_size.xy + in_pos * u_pos_size.zw; // pixel coords\n"
"  vec2 ndc = (p / u_screen_size) * 2.0 - 1.0;      // to NDC\n"
"  ndc.y = -ndc.y;                                   // flip Y for screen space\n"
"  gl_Position = vec4(ndc, 0.0, 1.0);\n"
"  v_uv = in_uv;\n"
"}\n";

static const char* HUD_FS =
"#version 330 core\n"
"in vec2 v_uv;\n"
"out vec4 frag;\n"
"uniform sampler2D u_tex0;\n"
"void main(){ frag = texture(u_tex0, v_uv); }\n";

// Compile helper
static GLuint hud_compile(GLenum type, const char* src){
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if(!ok){ glDeleteShader(s); return 0; }
    return s;
}

static GLuint hud_link(GLuint vs, GLuint fs){
    GLuint p = glCreateProgram();
    glAttachShader(p, vs); glAttachShader(p, fs);
    glLinkProgram(p);
    GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if(!ok){ glDeleteProgram(p); return 0; }
    return p;
}

static GLuint hud_make_black_rgba_tex(void){
    GLuint t=0; glGenTextures(1,&t);
    glBindTexture(GL_TEXTURE_2D, t);
    unsigned char px[4] = {0,0,0,255}; // opaque black
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1,1, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    return t;
}

void hud_init_minimal(void){
    if (hud_vao) return; // already inited

    // Shader
    GLuint vs = hud_compile(GL_VERTEX_SHADER,   HUD_VS);
    GLuint fs = hud_compile(GL_FRAGMENT_SHADER, HUD_FS);
    hud_prog = hud_link(vs, fs);
    glDeleteShader(vs); glDeleteShader(fs);

    u_screen_size = glGetUniformLocation(hud_prog, "u_screen_size");
    u_pos_size    = glGetUniformLocation(hud_prog, "u_pos_size");
    u_sampler0    = glGetUniformLocation(hud_prog, "u_tex0");

    float verts[] = {
        // x, y,  u, v
         0, 0,   0, 0,
         1, 0,   1, 0,
         1, 1,   1, 1,
         0, 0,   0, 0,
         1, 1,   1, 1,
         0, 1,   0, 1,
    };

    glGenVertexArrays(1, &hud_vao);
    glGenBuffers(1, &hud_vbo);
    glBindVertexArray(hud_vao);
    glBindBuffer(GL_ARRAY_BUFFER, hud_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float)*4, (void*)(sizeof(float)*2));
    glBindVertexArray(0);

    // Texture: 1x1 black RGBA8
    hud_tex = hud_make_black_rgba_tex();
}

void hud_set_texture(GLuint tex){
    if (!tex) return;
    if (hud_tex) glDeleteTextures(1, &hud_tex);
    hud_tex = tex;
}

void draw_hud(void){
    if (!hud_vao) hud_init_minimal();

    GLint vp[4]; glGetIntegerv(GL_VIEWPORT, vp);
    float W = (float)vp[2], H = (float)vp[3];

    float x = 8.0f, y = 8.0f, w = 160.0f, h = 40.0f;

    // Render state (very light touch)
    GLboolean blend_was = glIsEnabled(GL_BLEND);
    GLint last_prog = 0, last_tex = 0, last_active = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &last_prog);
    glGetIntegerv(GL_ACTIVE_TEXTURE,  &last_active);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_tex);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(hud_prog);
    glUniform2f(u_screen_size, W, H);
    // u_pos_size.xy = anchor in pixels (top-left), .zw = width/height
    glUniform4f(u_pos_size, x, y, w, h);
    glUniform1i(u_sampler0, 0);

    glBindVertexArray(hud_vao);
    glBindTexture(GL_TEXTURE_2D, hud_tex);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Restore minimal state
    glBindTexture(GL_TEXTURE_2D, last_tex);
    glUseProgram(last_prog);
    if (!blend_was) glDisable(GL_BLEND);
    glActiveTexture(last_active);
}


void setup_shadow_mapping() {
    glGenFramebuffers(1, &shadow_fbo);
    glGenTextures(1, &shadow_texture);
    glBindTexture(GL_TEXTURE_2D, shadow_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void compute_light_space_matrix() {
    Mat4 light_proj, light_view;
    Vec3 light_pos = {2, 4, 2}, target = {0, 0, 0}, up = {0, 1, 0};
    mat4_ortho(light_proj, -10, 10, -10, 10, 1.0f, 20.0f);
    mat4_lookat(light_view, light_pos, target, up);
    mat4_multiply(light_space_matrix, light_proj, light_view);
}
float aspectRatio;

double playback = 1.0;
bool lasers = false;

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

    int verts_per_face = (subdivisions + 1) * (subdivisions + 1);
    int faces = 6;
    int estimated_vertices = verts_per_face * faces;
    int estimated_triangles = 6 * subdivisions * subdivisions * 2;
    int estimated_indices = estimated_triangles * 3;

    init_mesh(mesh, estimated_vertices, estimated_indices);

    float step = 2.0f / subdivisions;

    Vec3 face_normals[6] = {
        { 1, 0, 0 }, { -1, 0, 0 },
        { 0, 1, 0 }, { 0, -1, 0 },
        { 0, 0, 1 }, { 0, 0, -1 }
    };

    for (int f = 0; f < 6; f++) {
        Vec3 normal = face_normals[f];
        Vec3 axis_a = {0}, axis_b = {0};

        if (fabsf(normal.x) > 0.5f) {
            axis_a.z = 1; axis_b.y = 1;
        } else if (fabsf(normal.y) > 0.5f) {
            axis_a.x = 1; axis_b.z = 1;
        } else {
            axis_a.x = 1; axis_b.y = 1;
        }

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

                float base_shape = fbm(dir, 3, 0.6f, 1.5f);
                float deform1 = 1.0f + base_shape * 0.7f;

                Vec3 bump_dir = vec3_scale(dir, 3.0f);
                float surface_noise = fbm(bump_dir, 5, 0.5f, 2.5f);
                float deform2 = 1.0f + surface_noise * 0.25f;
                float deform = deform1 * deform2;

                Vec3 final_pos = {
                    center.x + dir.x * radius * deform,
                    center.y + dir.y * radius * deform,
                    center.z + dir.z * radius * deform
                };

                float base_gray = 0.33f + base_shape * 0.08f;
                float brown_tint = (fbm(vec3_scale(dir, 1.3f), 2, 0.5f, 2.0f) - 0.5f) * 0.12f;
                float blue_tint = (1.0f - deform2) * fbm(vec3_scale(dir, 1.8f), 2, 0.5f, 2.0f) * 0.12f;

                Color4 color = {
                    .r = fminf(fmaxf(base_gray + brown_tint * 0.6f - blue_tint * 0.2f, 0.0f), 1.0f),
                    .g = fminf(fmaxf(base_gray + brown_tint * 0.4f - blue_tint * 0.1f, 0.0f), 1.0f),
                    .b = fminf(fmaxf(base_gray + blue_tint, 0.0f), 1.0f),
                    .a = 1.0f
                };

                VertexFormat vert = {
                    .position = final_pos,
                    .normal = (Vec3){0, 0, 0}, // will be filled later
                    .color = color,
                    .uv = (UV){0},
                    .roughness = 0.7f,
                    .metallic = 0.0f,
                    .emissive = 0.0f
                };

                int index = find_or_add_vertex(mesh, vert);
                vert_idx_grid[i][j] = index;
            }
        }

        BOOL flip = (f == 0 || f == 2 || f == 5);
        for (int i = 0; i < subdivisions; i++) {
            for (int j = 0; j < subdivisions; j++) {
                int a = vert_idx_grid[i][j];
                int b = vert_idx_grid[i+1][j];
                int c = vert_idx_grid[i][j+1];
                int d = vert_idx_grid[i+1][j+1];

                if (mesh->index_count + 6 >= mesh->index_capacity) {
                    mesh->index_capacity *= 2;
                    mesh->indices = realloc(mesh->indices, sizeof(unsigned int) * mesh->index_capacity);
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
void recalculate_flat_normals(Mesh* mesh) {
    if (!mesh || mesh->index_count % 3 != 0) return;

    for (int i = 0; i < mesh->index_count; i += 3) {
        int ia = mesh->indices[i];
        int ib = mesh->indices[i + 1];
        int ic = mesh->indices[i + 2];

        Vec3 a = mesh->vertices[ia].position;
        Vec3 b = mesh->vertices[ib].position;
        Vec3 c = mesh->vertices[ic].position;

        Vec3 u = vec3_sub(b, a);
        Vec3 v = vec3_sub(c, a);
        Vec3 n = vec3_normalize(vec3_cross(u, v));

        mesh->vertices[ia].normal = n;
        mesh->vertices[ib].normal = n;
        mesh->vertices[ic].normal = n;
    }
}

void generate_gas_giant(Mesh* mesh, Vec3 center, float radius, int subdivisions) {
    mesh->material_id = 1; 
    if (subdivisions < 1) subdivisions = 1;

    int verts_per_face = (subdivisions + 1) * (subdivisions + 1);
    int faces = 6;
    int estimated_vertices = verts_per_face * faces;
    int estimated_triangles = 6 * subdivisions * subdivisions * 2;
    int estimated_indices = estimated_triangles * 3;

    init_mesh(mesh, estimated_vertices, estimated_indices);

    float step = 2.0f / subdivisions;
    Vec3 face_normals[6] = {
        { 1, 0, 0 }, { -1, 0, 0 },
        { 0, 1, 0 }, { 0, -1, 0 },
        { 0, 0, 1 }, { 0, 0, -1 }
    };

    for (int f = 0; f < 6; f++) {
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
                Vec3 final_pos = {
                    center.x + dir.x * radius,
                    center.y + dir.y * radius,
                    center.z + dir.z * radius
                };

                VertexFormat vert = {
                    .position = final_pos,
                    .normal = dir,
                    .color = {1, 1, 1, 1},
                    .uv = {0},
                    .roughness = 0.8f,
                    .metallic = 0.0f,
                    .emissive = 0.0f
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


static GLADapiproc APIENTRY glad_wgl_loader(const char* name) {
    return (GLADapiproc)wglGetProcAddress(name);
}
GLADapiproc APIENTRY glad_opengl_loader(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (!p || p == (void*)0x1 || p == (void*)0x2 || p == (void*)0x3 || p == (void*)-1) {
        HMODULE module = LoadLibraryA("opengl32.dll");
        p = (void*)GetProcAddress(module, name);
    }
    return (GLADapiproc)p;
}


LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {

    // 1. Register and create window
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = " ";
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
    if (!gladLoadWGL(hDC, glad_wgl_loader)) {
        MessageBox(0, "Failed to load WGL", "Error", MB_OK);
        return 1;
    }

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
    if (!status || numFormats == 0) {
        MessageBox(0, "Failed to choose multisample pixel format", "Error", MB_OK);
        return 1;
    }

    PIXELFORMATDESCRIPTOR realPFD;
    DescribePixelFormat(hDC, format, sizeof(realPFD), &realPFD);
    SetPixelFormat(hDC, format, &realPFD);





    int attribs[] = {
        0x2091, 3,
        0x2092, 3,
        0x9126, 0x00000001,
        0
    };
        typedef HGLRC (WINAPI *PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int *);
    PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB =
        (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

    if (!wglCreateContextAttribsARB) {
        MessageBox(0, "Could not load wglCreateContextAttribsARB", "Fatal Error", MB_OK);
        exit(1);
    }

    hRC = wglCreateContextAttribsARB(hDC, 0, attribs);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(dummyRC);
    if (!hRC) {
        MessageBox(0, "wglCreateContextAttribsARB failed!", "Fatal Error", MB_OK);
        return 1;
    }

    if (!wglMakeCurrent(hDC, hRC)) {
        MessageBox(0, "wglMakeCurrent failed!", "Error", MB_OK);
        return 1;
    }
    // 4. Load OpenGL functions
    if (!gladLoadGL(glad_opengl_loader)) {
        MessageBox(0, "Failed to load OpenGL", "Error", MB_OK);
        return 1;
    }

    glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    //glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);



    // 5. Build shaders
    ShaderProgram test_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/asteroid.frag");
    glUseProgram(test_shader.id);
    ShaderProgram skybox_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/skybox.frag");
    skybox_shader.u_view_loc = glGetUniformLocation(skybox_shader.id, "u_view");
    skybox_shader.u_projection_loc = glGetUniformLocation(skybox_shader.id, "u_projection");

    
    glGenFramebuffers(1, &shadow_fbo);
    glGenTextures(1, &shadow_texture);
    glBindTexture(GL_TEXTURE_2D, shadow_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

    float borderColor[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadow_texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);


    // 6. Projection matrix (identity here)
    float projection[16];
    float cview_rot[16];
    float cview_trans[16];
    float cview[16];
    float model[16];
    float pv[16];
    float mvp[16];

    mat4_perspective(projection, 90.0f, aspectRatio, 0.1f, 10000.0f);

    Quat rot_conj = quat_conjugate(rot);
    quat_to_matrix(&rot_conj, cview_rot);      // turns into matrix

    Vec3 inv_pos = vec3_invert(pos);          // move world in *opposite* direction
    mat4_identity(cview_trans);
    mat4_translate(cview_trans, inv_pos.x, inv_pos.y, inv_pos.z);

    mat4_multiply(cview, cview_rot, cview_trans);

    mat4_identity(model);

    mat4_multiply(pv, projection, cview);
    mat4_multiply(mvp, pv, model);


    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f}; 
    Quat Rvel = {0.0f, 0.0f, 0.0f, 1.0f};
    float thrust = 0.03f;

    LARGE_INTEGER freq, prev, curr;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&prev);


    Object cube = {
    .position = {0.0f, 0.0f, 0.0f},
    .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
    .scale = {1.0f, 1.0f, 1.0f},
    .mesh_id = 0,
    .flags = OBJ_FLAG_VISIBLE,
    };

    Mesh cube_mesh = {
        .vertices = (VertexFormat[]) {
            // Back face (0–3)
            {{-0.5f, -0.5f, -0.5f}, { 0,  0, -1}, {0}, {1,0,0,1}, {0,0}, 0.5f,0,0,},
            {{ 0.5f, -0.5f, -0.5f}, { 0,  0, -1}, {0}, {1,0,0,1}, {1,0}, 0.5f,0,0,},
            {{ 0.5f,  0.5f, -0.5f}, { 0,  0, -1}, {0}, {1,0,0,1}, {1,1}, 0.5f,0,0,},
            {{-0.5f,  0.5f, -0.5f}, { 0,  0, -1}, {0}, {1,0,0,1}, {0,1}, 0.5f,0,0,},

            // Front face (4–7)
            {{-0.5f, -0.5f,  0.5f}, { 0,  0,  1}, {0}, {1,0,0,1}, {0,0}, 0.5f,0,0,},
            {{ 0.5f, -0.5f,  0.5f}, { 0,  0,  1}, {0}, {1,0,0,1}, {1,0}, 0.5f,0,0,},
            {{ 0.5f,  0.5f,  0.5f}, { 0,  0,  1}, {0}, {1,0,0,1}, {1,1}, 0.5f,0,0,},
            {{-0.5f,  0.5f,  0.5f}, { 0,  0,  1}, {0}, {1,0,0,1}, {0,1}, 0.5f,0,0,},

            // Left face (8–11)
            {{-0.5f, -0.5f,  0.5f}, {-1,  0,  0}, {0}, {1,0,0,1}, {0,0}, 0.5f,0,0,},
            {{-0.5f, -0.5f, -0.5f}, {-1,  0,  0}, {0}, {1,0,0,1}, {1,0}, 0.5f,0,0,},
            {{-0.5f,  0.5f, -0.5f}, {-1,  0,  0}, {0}, {1,0,0,1}, {1,1}, 0.5f,0,0,},
            {{-0.5f,  0.5f,  0.5f}, {-1,  0,  0}, {0}, {1,0,0,1}, {0,1}, 0.5f,0,0,},

            // Right face (12–15)
            {{ 0.5f, -0.5f, -0.5f}, { 1,  0,  0}, {0}, {1,0,0,1}, {0,0}, 0.5f,0,0,},
            {{ 0.5f, -0.5f,  0.5f}, { 1,  0,  0}, {0}, {1,0,0,1}, {1,0}, 0.5f,0,0,},
            {{ 0.5f,  0.5f,  0.5f}, { 1,  0,  0}, {0}, {1,0,0,1}, {1,1}, 0.5f,0,0,},
            {{ 0.5f,  0.5f, -0.5f}, { 1,  0,  0}, {0}, {1,0,0,1}, {0,1}, 0.5f,0,0,},

            // Top face (16–19)
            {{-0.5f,  0.5f, -0.5f}, { 0,  1,  0}, {0}, {1,0,0,1}, {0,0}, 0.5f,0,0,},
            {{ 0.5f,  0.5f, -0.5f}, { 0,  1,  0}, {0}, {1,0,0,1}, {1,0}, 0.5f,0,0,},
            {{ 0.5f,  0.5f,  0.5f}, { 0,  1,  0}, {0}, {1,0,0,1}, {1,1}, 0.5f,0,0,},
            {{-0.5f,  0.5f,  0.5f}, { 0,  1,  0}, {0}, {1,0,0,1}, {0,1}, 0.5f,0,0,},

            // Bottom face (20–23)
            {{-0.5f, -0.5f,  0.5f}, { 0, -1,  0}, {0}, {1,0,0,1}, {0,0}, 0.5f,0,0,},
            {{ 0.5f, -0.5f,  0.5f}, { 0, -1,  0}, {0}, {1,0,0,1}, {1,0}, 0.5f,0,0,},
            {{ 0.5f, -0.5f, -0.5f}, { 0, -1,  0}, {0}, {1,0,0,1}, {1,1}, 0.5f,0,0,},
            {{-0.5f, -0.5f, -0.5f}, { 0, -1,  0}, {0}, {1,0,0,1}, {0,1}, 0.5f,0,0,}
        },
        .indices = (unsigned int[]) {
            0,1,2, 2,3,0,       // back
            4,5,6, 6,7,4,       // front
            8,9,10, 10,11,8,    // left
            12,13,14, 14,15,12, // right
            16,17,18, 18,19,16, // top
            20,21,22, 22,23,20  // bottom
        },
        .vertex_count = 24,
        .vertex_capacity = 24,
        .index_count = 36,
        .index_capacity = 36,
        .material_id = 0,
        .vao = 0,
        .vbo = 0,
        .ibo = 0,
        .submeshes = (SubMesh[]) {
            {.index_offset = 0, .index_count = 36, .material_id = 0}
        },
        .submesh_count = 1,
        .flags = 0
    };
    upload_mesh(&cube_mesh);
    mesh_pool[0] = &cube_mesh;

    Mesh asteroid_mesh;
    generate_asteroid_cubesphere(&asteroid_mesh, (Vec3){0, 0, 0}, 5.0f, 100);
    recalculate_flat_normals(&asteroid_mesh);
    upload_mesh(&asteroid_mesh);
    mesh_pool[1] = &asteroid_mesh;
    Object asteroid = {
        .position = {0.0f, 0.0f, -40.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh_id = 1,
        .flags = OBJ_FLAG_VISIBLE,
    };

    Mesh gas_mesh;
    generate_gas_giant(&gas_mesh, (Vec3){0, 0, 0}, 20000.0f, 100);
    //generate_asteroid_cubesphere(&gas_mesh, (Vec3){0, 0, 0}, 20.0f, 100);
    upload_mesh(&gas_mesh);
    mesh_pool[2] = &gas_mesh;

    Object gas_planet = {
        .position = {0, 0, 100000},
        .rotation = {0, 0, 0, 1},
        .scale = {1, 1, 1},
        .mesh_id = 2,   
        .flags = OBJ_FLAG_VISIBLE
    };

    ShaderProgram planet_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/planet.frag"); 
    set_common_matrices(&test_shader, cview, projection);
    set_common_matrices(&skybox_shader, cview, projection);
    set_common_matrices(&planet_shader, cview, projection);

    const static Material test_material = {
        .albedo = {1, 0, 0, 1},
        .roughness = 0.5f,
        .metallic = 0.0f,
        .emissive = 0.0f
    };
    material_pool[0] = &test_material;
    const static Material asteroid_material = {
        .albedo = {0.4f, 0.3f, 0.2f, 1.0f}, // dusty brown rock
        .roughness = 0.9f,
        .metallic = 0.1f,
        .emissive = 0.0f
    };
    material_pool[1] = &asteroid_material;

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

    //HUD
    hud_init_minimal();



    
    // 8. Main loop
    MSG msg;
    bool running = true;
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
            pos = (Vec3){0, 0, 0};
            vel = (Vec3){0, 0, 0};

            glDeleteProgram(test_shader.id); // clean up the old
            test_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/asteroid.frag");
            glUseProgram(test_shader.id);

            glDeleteProgram(skybox_shader.id);
            skybox_shader = create_shader_program_from_files("shaders/unified.vert", "shaders/skybox.frag");
            glUseProgram(skybox_shader.id);
            glEnable(GL_LINE_SMOOTH);
            
        }
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


        Mat4 light_proj, light_view, light_space_matrix;
        mat4_ortho(light_proj, -10, 10, -10, 10, 0.1f, 50.0f);
        Vec3 light_pos = { -2.0f, 4.0f, -1.0f };
        Vec3 light_target = { 0.0f, 0.0f, 0.0f };
        Vec3 light_up = { 0.0f, 1.0f, 0.0f };
        mat4_lookat(light_view, light_pos, light_target, light_up);
        mat4_multiply(light_space_matrix, light_proj, light_view);

        glViewport(0, 0, 1024, 1024);
        glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
        glClear(GL_DEPTH_BUFFER_BIT);


        glUseProgram(shadow_shader.id);

        glUniformMatrix4fv(shadow_shader.u_light_space_matrix_loc, 1, GL_FALSE, light_space_matrix);
        //draw_object(&cube, &shadow_shader, mesh_pool, GL_TRIANGLES);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glViewport(0, 0, 800, 600);
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

        glUseProgram(test_shader.id);
        glUniform1i(glGetUniformLocation(test_shader.id,"u_is_skybox"), 0);

        // Set light direction
        Vec3 light_dir = vec3_normalize((Vec3){2.0f, -4.0f, 1.0f});
        GLint light_dir_loc = glGetUniformLocation(test_shader.id, "u_light_dir");
        glUniform3f(light_dir_loc, light_dir.x, light_dir.y, light_dir.z);
        GLint u_view_pos = glGetUniformLocation(test_shader.id, "u_view_pos");
        glUniform3f(u_view_pos, pos.x, pos.y, pos.z); // camera position



        // Set matrices
        glUniformMatrix4fv(test_shader.u_view_loc, 1, GL_FALSE, cview);
        glUniformMatrix4fv(test_shader.u_projection_loc, 1, GL_FALSE, projection);
        glUniformMatrix4fv(test_shader.u_light_space_matrix_loc, 1, GL_FALSE, light_space_matrix);

        // Set shadow map
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, shadow_texture);
        glUniform1i(glGetUniformLocation(test_shader.id, "u_shadow_map"), 1);

        //draw_object(&cube, &test_shader, mesh_pool, GL_TRIANGLES);
        draw_object(&asteroid, &test_shader, mesh_pool, GL_TRIANGLES);


        glUseProgram(planet_shader.id);
        glUniform1i(glGetUniformLocation(planet_shader.id,"u_is_skybox"), 0);

        glUniformMatrix4fv(planet_shader.u_view_loc, 1, GL_FALSE, cview);
        glUniformMatrix4fv(planet_shader.u_projection_loc, 1, GL_FALSE, projection);
        glUniformMatrix4fv(planet_shader.u_light_space_matrix_loc, 1, GL_FALSE, light_space_matrix);

        glUniform3f(glGetUniformLocation(planet_shader.id, "u_light_dir"), light_dir.x, light_dir.y, light_dir.z);
        glUniform3f(glGetUniformLocation(planet_shader.id, "u_view_pos"), pos.x, pos.y, pos.z);
        mat4_perspective(projection, 45.0f, aspectRatio, 1000.0f, 40000000000.0f);
        draw_object(&gas_planet, &planet_shader, mesh_pool, GL_TRIANGLES);

        glDisable(GL_DEPTH_TEST);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_hud();
        glEnable(GL_DEPTH_TEST);
        
        SwapBuffers(hDC);

        if (!PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE)) Sleep(0);
    }
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            //ShowCursor(FALSE);
            SetCapture(hWnd);
            {
                // Clip cursor to client area
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
            break;
        case WM_SIZE:
            GLsizei width = LOWORD(lParam);
            GLsizei height = HIWORD(lParam);
            if (height == 0) height = 1; 
            glViewport(0, 0, width, height);
            aspectRatio = (float)width / (float)height;
            //SetProjectionMatrix(1.0f, 4000000000.0f);
            //mat4_perspective(&projectionMatrix, 45.0f, aspectRatio, 1.0f, 4000000000.0f);
            {
                // Update clip region on resize
                RECT rect;
                GetClientRect(hWnd, &rect);
                POINT ul = {rect.left, rect.top};
                POINT lr = {rect.right, rect.bottom};
                ClientToScreen(hWnd, &ul);
                ClientToScreen(hWnd, &lr);
                RECT clipRect = {ul.x, ul.y, lr.x, lr.y};
                ClipCursor(&clipRect);
            }
            break;
        case WM_CLOSE:
            ClipCursor(NULL); // Release cursor clip
            PostQuitMessage(0);
            break;
        case WM_DESTROY:
            ClipCursor(NULL); // Release cursor clip
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(hRC);
            hRC = NULL;
            ReleaseDC(hWnd, hDC);
            hDC = NULL;
            break;
        case WM_MOUSEMOVE: 
            //for first person
            RECT windowRect;
            GetClientRect(hWnd, &windowRect);
            int centerX = (windowRect.left + windowRect.right) / 2;
            int centerY = (windowRect.top + windowRect.bottom) / 2;

            POINT centerScreen = {centerX, centerY};
            ClientToScreen(hWnd, &centerScreen);
            //SetCursorPos(centerScreen.x, centerScreen.y);

            //view.x += (LOWORD(lParam) - centerX)*0.01;
            //view.y += (HIWORD(lParam) - centerY)*0.01;
            if (view.y > 89.9f) view.y = 89.9f;
            if (view.y < -89.9f) view.y = -89.9f;

            break;
        case WM_LBUTTONDOWN:
            //playSoundEffect(SND_GUN, 440, 1, 0, 0);
            lasers = TRUE;
            break;
        case WM_LBUTTONUP:
            lasers = FALSE;
            break;
        case WM_KEYDOWN:
                if (wParam == VK_ESCAPE) {
                    ShowCursor(TRUE);
                    ReleaseCapture();
                }
            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
            
    }
    return 0;
}





