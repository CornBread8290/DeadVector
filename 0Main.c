#include <math.h>
#include <windows.h>
#include <gl/gl.h>
#include "defs.h"
#include <stdbool.h>





HGLRC hRC; // OpenGL Rendering Context
HDC hDC;   // Device Context
HWND hWnd; // Window Handle

float aspectRatio;

double playback = 1.0;
uint16_t keyState = 0;
BOOL lasers = FALSE;

Quat rot = {0.0f, 0.0f, 0.0f, 1.0f}; //Pitch, Yaw, Roll, W
Vec3 angVel = {0}; // current angular velocity vector (radians/sec)

Vec2 view = {0.0f, 0.0f};
Vec3 pos = {0.0f, 0.0f, 0.0f};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);




void generate_asteroid_cubesphere(Mesh* mesh, Vec3 center, float radius, int subdivisions) {
    if (subdivisions < 1) subdivisions = 1;

    int verts_per_face = (subdivisions + 1) * (subdivisions + 1);
    int faces = 6;
    int estimated_vertices = verts_per_face * faces;
    int estimated_triangles = 6 * subdivisions * subdivisions * 2;

    init_mesh(mesh, estimated_vertices);

    // Slightly dusty, rocky base material
    mesh->material.ambient  = (Color4){0.12f, 0.12f, 0.12f, 1.0f};
    mesh->material.diffuse  = (Color4){0.35f, 0.35f, 0.35f, 1.0f};
    mesh->material.specular = (Color4){0.1f, 0.1f, 0.1f, 1.0f};
    mesh->material.shininess = 8.0f;

    float step = 2.0f / subdivisions;

    Vec3 face_normals[6] = {
        {  1,  0,  0 }, { -1,  0,  0 }, {  0,  1,  0 },
        {  0, -1,  0 }, {  0,  0,  1 }, {  0,  0, -1 }
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

                float len = sqrtf(p.x * p.x + p.y * p.y + p.z * p.z);
                Vec3 dir = len > 0.0001f ? (Vec3){ p.x / len, p.y / len, p.z / len } : p;

                float base_shape = fbm(dir, 3, 0.6f, 1.5f);
                float deform1 = 1.0f + base_shape * 0.7f;

                Vec3 bump_dir = { dir.x * 3.0f, dir.y * 3.0f, dir.z * 3.0f };
                float surface_noise = fbm(bump_dir, 5, 0.5f, 2.5f);
                float deform2 = 1.0f + surface_noise * 0.25f;
                float deform = deform1 * deform2;
                Vec3 final_pos = {
                    center.x + dir.x * radius * deform,
                    center.y + dir.y * radius * deform,
                    center.z + dir.z * radius * deform
                };

                // Base grayscale (centered at ~0.35)
                float base_gray = 0.33f + base_shape * 0.08f;  // stays between ~0.25 to ~0.41

                // Brown tone (dirt/mineral tint)
                float brown_noise = fbm((Vec3){dir.x * 1.3f, dir.y * 1.3f, dir.z * 1.3f}, 2, 0.5f, 2.0f);
                float brown_tint = (brown_noise - 0.5f) * 0.12f;  // [-0.06, +0.06]

                // Blue ice tint in crevices
                float ice_factor = (1.0f - deform2);
                float ice_noise = fbm((Vec3){dir.x * 1.8f, dir.y * 1.8f, dir.z * 1.8f}, 2, 0.5f, 2.0f);
                float blue_tint = ice_factor * ice_noise * 0.12f; // max +12% blue

                Color4 color = {
                    .r = base_gray + brown_tint * 0.6f - blue_tint * 0.2f,  // balance out excess blue
                    .g = base_gray + brown_tint * 0.4f - blue_tint * 0.1f,
                    .b = base_gray + blue_tint,
                    .a = 1.0f
                };

                // Clamp
                color.r = 0.0f; fminf(fmaxf(color.r, 0.0f), 1.0f);
                color.g = 1.0f; fminf(fmaxf(color.g, 0.0f), 1.0f);
                color.b = 0.0f;//fminf(fmaxf(color.b, 0.0f), 1.0f);

                UV uv = {0};
                int index = find_or_add_vertex(mesh, final_pos, color, uv);
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

        Vec3 a = mesh->vertices[ia];
        Vec3 b = mesh->vertices[ib];
        Vec3 c = mesh->vertices[ic];

        Vec3 u = { b.x - a.x, b.y - a.y, b.z - a.z };
        Vec3 v = { c.x - a.x, c.y - a.y, c.z - a.z };

        Vec3 n = {
            u.y * v.z - u.z * v.y,
            u.z * v.x - u.x * v.z,
            u.x * v.y - u.y * v.x
        };
        float len = sqrtf(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 0.00001f) {
            n.x /= len; n.y /= len; n.z /= len;
        } else {
            n = (Vec3){0, 1, 0}; // fallback
        }

        mesh->normals[ia] = n;
        mesh->normals[ib] = n;
        mesh->normals[ic] = n;
    }
}

DWORD WINAPI mesh_and_normals_thread(LPVOID lpParam) {
    Object* obj = (Object*)lpParam;

    generate_asteroid_cubesphere(&obj->mesh, obj->position, 1.0f, 128);
    obj->flags |= OBJ_FLAG_NEEDS_REBUILD;
    recalculate_flat_normals(&obj->mesh);

    obj->flags &= ~OBJ_FLAG_NORMALS_DIRTY;

    return 0;
}
DWORD WINAPI normal_calc_thread(LPVOID lpParam) {
    Object* obj = (Object*)lpParam;
    recalculate_flat_normals(&obj->mesh);
    obj->flags &= ~OBJ_FLAG_NORMALS_DIRTY;
    return 0;
}

//0 - 1
float value_noise1d(float x) {
    int xi = (int)floorf(x);
    float xf = x - xi;
    // Hash function for pseudo-randomness
    #define HASH1(n) ((n)*2654435761U)
    unsigned int h0 = HASH1(xi);
    unsigned int h1 = HASH1(xi+1);
    float v0 = (h0 & 0xFFFF) / 65535.0f;
    float v1 = (h1 & 0xFFFF) / 65535.0f;
    // Linear interpolation
    return v0 * (1 - xf) + v1 * xf;
}


#include <stdlib.h>
#include <math.h>

void drawRings(float innerRadius, float outerRadius, int segments, int bands) {
    float totalWidth = outerRadius - innerRadius;
    float radius = innerRadius;

    for (int b = 0; b < bands && radius < outerRadius; ++b) {
        float bandFrac = (wangHash(b) / 4294967295.0f)*2; // range: 0.5 to ~1.5
        float maxStep = totalWidth / bands * 2.0f; // Allow wide variations
        float bandStep = fminf(bandFrac * (totalWidth / bands), outerRadius - radius);

        float r0 = radius;
        float r1 = r0 + bandStep;
        radius = r1;

        float base = 0.6f + 0.3f * sin(b * 0.5f); 
        Color4 hue;
        hue.r = base * 0.3f;
        hue.g = base * 0.6f;
        hue.b = base * 1.0f;
        hue.a = 0.4f + 0.5f * fabs(cos(b * 0.3f));

        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2 * pi * i / segments;
            Vec2 vec = {cos(angle), sin(angle)};

            glNormal3f(vec.x / 2, vec.y, 0.0f);
            glColor4fv(hue.data); glVertex2f(vec.x * r0, vec.y * r0);
            glNormal3f(vec.x / 2, vec.y, 0.0f);
            glColor4fv(hue.data); glVertex2f(vec.x * r1, vec.y * r1);
        }
        glEnd();
    }
}

void generatePlanetMesh(Mesh* mesh, float radius, int lats, int longs) {
    init_mesh(mesh, (lats + 1) * (longs + 1) * 2);

    mesh->material = (Material){
        .ambient  = { 0.0f, 0.0f, 0.0f, 1.0f },
        .diffuse  = { 0.6f, 0.5f, 0.4f, 1.0f },
        .specular = { 0.0f, 0.0f, 0.0f, 1.0f },
        .shininess = 1.0f
    };

    float dark_brown[3] = {0.26f, 0.16f, 0.07f};
    float light_tan[3]  = {0.90f, 0.80f, 0.60f};

    for (int i = 0; i <= lats; ++i) {
        float lat = pi * (-0.5f + (float)i / lats);
        float z = sinf(lat);
        float zr = cosf(lat);

        for (int j = 0; j <= longs; ++j) {
            float lng = 2.0f * pi * (float)j / longs;
            float x = cosf(lng);
            float y = sinf(lng);

            Vec3 spherePos = { x * zr, y * zr, z };
            Vec3 pos = { radius * spherePos.x, radius * spherePos.y, radius * spherePos.z };
            Vec3 norm = vec3_normalize(spherePos);
            UV uv = { (float)j / longs, (float)i / lats };

            // Color band based on latitude noise
            float latNoise = value_noise1d((float)i * 0.15f);
            latNoise += 0.5f * value_noise1d((float)i * 0.35f + 100.0f);
            latNoise += 0.25f * value_noise1d((float)i * 1.0f + 200.0f);
            latNoise /= 1.75f;

            float band = 0.25f + 0.75f * latNoise;

            // Add 3D texture using fbm
            Vec3 noiseP = { spherePos.x * 4.0f, spherePos.y * 4.0f, spherePos.z * 4.0f };
            float detail = fbm(noiseP, 5, 0.5f, 2.0f); // swirly juicy goodness
            detail = (detail - 0.5f) * 0.3f + 0.5f; // center and flatten range

            float finalBlend = band * detail;
            Color4 color = {
                dark_brown[0] + (light_tan[0] - dark_brown[0]) * finalBlend,
                dark_brown[1] + (light_tan[1] - dark_brown[1]) * finalBlend,
                dark_brown[2] + (light_tan[2] - dark_brown[2]) * finalBlend,
                1.0f
            };

            find_or_add_vertex(mesh, pos, color, uv);
        }
    }

    for (int i = 0; i < lats; ++i) {
        for (int j = 0; j < longs; ++j) {
            int row1 = i * (longs + 1);
            int row2 = (i + 1) * (longs + 1);

            int i0 = row1 + j;
            int i1 = row2 + j;
            int i2 = row2 + (j + 1);
            int i3 = row1 + (j + 1);

            mesh->indices[mesh->index_count++] = i0;
            mesh->indices[mesh->index_count++] = i2;
            mesh->indices[mesh->index_count++] = i1;

            mesh->indices[mesh->index_count++] = i0;
            mesh->indices[mesh->index_count++] = i3;
            mesh->indices[mesh->index_count++] = i2;
        }
    }

    for (int i = 0; i < mesh->index_count; i += 3) {
        int i0 = mesh->indices[i];
        int i1 = mesh->indices[i + 1];
        int i2 = mesh->indices[i + 2];

        Vec3 v0 = mesh->vertices[i0];
        Vec3 v1 = mesh->vertices[i1];
        Vec3 v2 = mesh->vertices[i2];

        Vec3 edge1 = vec3_sub(v1, v0);
        Vec3 edge2 = vec3_sub(v2, v0);
        Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));

        mesh->normals[i0] = vec3_add(mesh->normals[i0], normal);
        mesh->normals[i1] = vec3_add(mesh->normals[i1], normal);
        mesh->normals[i2] = vec3_add(mesh->normals[i2], normal);
    }

    for (int i = 0; i < mesh->vertex_count; i++) {
        mesh->normals[i] = vec3_normalize(mesh->normals[i]);
    }
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    const char *className = "OGL";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    float RF = 0.0f;
    float IR = 0.0f;
    float VIS = 1.0f;
    float RAD = 0.25f;

    float dRF = 0.0f;
    float dIR = 0.0f;
    float dVIS = 0.00f;
    float dRAD =0.0f;


    RegisterClass(&wc);

    hWnd = CreateWindowEx(0, className, "Dead Vector", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1024, 576, NULL, NULL, hInstance, NULL);

    hDC = GetDC(hWnd);

    PIXELFORMATDESCRIPTOR pfd = {0};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cRedBits = 8;
    pfd.cGreenBits = 8;
    pfd.cBlueBits = 8;
    pfd.cAlphaBits = 8;

    int pixelFormat = ChoosePixelFormat(hDC, &pfd);

    SetPixelFormat(hDC, pixelFormat, &pfd);

    hRC = wglCreateContext(hDC);

    wglMakeCurrent(hDC, hRC);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glDepthFunc(GL_LEQUAL);

    float linePoints[10][2] = {
    {-1.0f,  -0.2f}, { 1.0f,  -0.2f},
    { -0.5f,  -0.2f}, { -0.75f,   1.0f},
    { 0.5f,  -0.2f}, { 0.75f,   1.0f},
    {-0.05f, -0.0f}, {0.05f, 0.0f},
    {0.0f, -0.05f}, {0.0f,  0.05f}
    };


    unsigned char *starData = malloc(2048 * 1024 * 3);
    
    for (int i = 0; i < 500000; ++i) {
        float sum_x = 0.0f, sum_y = 0.0f;

        for (int j = 0; j < 3; ++j) {
            sum_x += xorshift32f();
        }
        for (int j = 0; j < 30; ++j) {
            sum_y += xorshift32f();
        }

        int x = (int)(sum_x / 3.0f * 2.0f *1024);
        int y = (int)(sum_y / 30.0f * 2.0f*512);

        float w = xorshift32f()*2;
        int index = (y * 2048 + x) * 3;

        x = (x - 1024) * ((x - 1024) >= 0 ? 1 : -1);

        if (index >=2048 * 1024 * 3) continue;
        int r = starData[index + 0] + (int)(101 * w);
        int g = starData[index + 1] + (int)(77 * w * (1+x/6024.0f));
        int b = starData[index + 2] + (int)(59 * w * (1+(x/1248.0f)*(x/1248.0f)));

        //        int g = starData[index + 1] + (int)(77 * w + x/70.0f);
        //int b = starData[index + 2] + (int)(59 * w + x/30.0f);

        starData[index + 0] = r > 255 ? 255 : r;
        starData[index + 1] = g > 255 ? 255 : g;
        starData[index + 2] = b > 255 ? 255 : b;
    }
    for (int i = 0; i<1000; i++){
        int index = (xorshift32() % (2048 * 1024)) * 3;
        starData[index] = 255;
        starData[index + 1] =255;
        starData[index + 2] =255;
    }

    GLuint starMap;
    glGenTextures(1, &starMap);
    glBindTexture(GL_TEXTURE_2D, starMap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 2048, 1024, 0, GL_RGB, GL_UNSIGNED_BYTE, starData);
    free(starData);




    Object asteroid_obj = {
        .position = {0.0f, 0.0f, -5.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh = {}, 
        .flags = OBJ_FLAG_NORMALS_DIRTY | OBJ_FLAG_NEEDS_REBUILD,
        .components = NULL
    };
    CreateThread(NULL, 0, mesh_and_normals_thread, &asteroid_obj, 0, NULL);

    Object planet = {
        .position = {0.0f, 0.0f, -5000000.0f},
        .rotation = {-0.5, 0.0, 0.0, 0.8660254},
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh = {}, 
        .flags = 0,
        .components = NULL
    };
    generatePlanetMesh(&planet.mesh, 1000000.0f, 256, 64);


    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f}; 
    Quat Rvel = {0.0f, 0.0f, 0.0f, 1.0f};
    float thrust = 0.03f;
    //float thrust = 3000.0f;
    float dt = 0.018;


    MSG msg;


    GLfloat light_pos[] = { 4.0f, 0.0f, 0.0f, 0.0f }; // directional
    GLfloat light_diffuse[] = { 1.0f, 0.95f, 0.9f, 1.0f };
    GLfloat light_specular[] = { 1.0f, 1.0f, 1.0f, 1.0f };

    glEnable(GL_LIGHT0);
    glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, light_diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, light_specular);

    glEnable(GL_LIGHT1);
    glLightfv(GL_LIGHT1, GL_AMBIENT, (GLfloat[]){ 0.0f, 0.0f, 0.0f, 1.0f });
    glLightfv(GL_LIGHT1, GL_DIFFUSE, (GLfloat[]){ 0.8f, 0.8f, 0.8f, 1.0f });
    glLightfv(GL_LIGHT1, GL_SPECULAR, (GLfloat[]){ 1.0f, 1.0f, 1.0f, 1.0f });
    glLightf(GL_LIGHT1, GL_CONSTANT_ATTENUATION, 1.0f);
    glLightf(GL_LIGHT1, GL_LINEAR_ATTENUATION, 0.0f);
    glLightf(GL_LIGHT1, GL_QUADRATIC_ATTENUATION, 0.45f);



    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE);

    glShadeModel(GL_SMOOTH);
    
    //MAIN LOOP
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_LIGHTING);
        glEnable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        Vec3 dir = {0};
        float vol = 0.002f;
        IR = __builtin_popcount(keyState&~0x100)/15.0f;

        Vec3 torque = {0,0,0};
        if (keyState & 0x01) torque.x += 1.0f; // W - pitch up
        if (keyState & 0x04) torque.x -= 1.0f; // S - pitch down
        if (keyState & 0x02) torque.y -= 1.0f; // A - yaw left
        if (keyState & 0x08) torque.y += 1.0f; // D - yaw right
        if (keyState & 0x10) torque.z -= 1.0f; // Q - roll left
        if (keyState & 0x20) torque.z += 1.0f; // E - roll right

        angVel.x += torque.x * dt;
        angVel.y += torque.y * dt;
        angVel.z += torque.z * dt;
        Quat wQuat = {angVel.x, angVel.y, angVel.z, 0.0f}; // angular velocity quaternion
        Quat delta = quat_mul(wQuat, rot);

        // Scale the result by 0.5 * dt
        rot.x += delta.x * 0.5f * dt;
        rot.y += delta.y * 0.5f * dt;
        rot.z += delta.z * 0.5f * dt;
        rot.w += delta.w * 0.5f * dt;  

        rot = quat_normalize(rot);
        //del?^


        if (keyState & 0x40) { //dir.y += 1;
            //playSoundEffect(SND_BEEP, 60, 0.85f, 0, 3);
            dir.y += 1;
        }// Shift
        if (keyState & 0x80) { dir.y -= 1; }// Ctrl   
        if (keyState & 0x100) { IR+=0.5f; dir.z -= 1; } // Up
        if (keyState & 0x200) { dir.z += 1; } // Down
        if (keyState & 0x400) { dir.x -= 1; } // Left
        if (keyState & 0x800) { dir.x += 1; } // Right
        if (keyState){
            if(IR<0.8) IR += 0.02f;
        }

        if (keyState & 0b101111101101 ) { playSoundEffect(SND_ENGINE, 10.0f,vol, -1, 1); }
        else stopSound(1);
        if (keyState & 0b11111010111) {playSoundEffect(SND_ENGINE, 10.0f,vol, 1, 2);}
        else stopSound(2);
    
        Quat invRot = quat_conjugate(rot);
        Vec3 worldDir = quat_rotate_vec3(invRot, dir);

        vel.x += worldDir.x * thrust;
        vel.y += worldDir.y * thrust;
        vel.z += worldDir.z * thrust;

        pos.x += vel.x * dt;
        pos.y += vel.y * dt;
        pos.z += vel.z * dt;
        
        glLoadIdentity();

        SetProjectionMatrix(0.0001f, 100.0f);
        glPushMatrix();
            glLightfv(GL_LIGHT1, GL_POSITION, (GLfloat[]){ 0.0f, 0.0f, 0.0f, 1.0f });

            glRotatef(view.y, 1,0,0);
            glRotatef(view.x, 0,1,0);



            //Rotation
            rot = quat_normalize(rot);
            float mat[16];
            quat_to_matrix(&rot, mat);
            glMultMatrixf(mat);


            //GLfloat light_pos[] = { 4.0f, 0.0f, 0.0f, 0.0f };
            glLightfv(GL_LIGHT0, GL_POSITION, light_pos);
            glDisable(GL_LIGHTING);

            //Stars
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glEnable(GL_TEXTURE_2D);

            glPushMatrix();
                glRotatef(60, 1.0f, 0, 1);
                glRotatef(30, 0.0f, 1, 0);

                int stacks = 32;
                int slices = 64;
                float radius = 10.0f;

                for (int i = 0; i < stacks; ++i) {
                    float lat0 = pi * (-0.5f + (float)i / stacks);
                    float lat1 = pi * (-0.5f + (float)(i + 1) / stacks);

                    float y0 = sinf(lat0);
                    float y1 = sinf(lat1);
                    float r0 = cosf(lat0);
                    float r1 = cosf(lat1);

                    glBegin(GL_QUAD_STRIP);
                    for (int j = 0; j <= slices; ++j) {
                        float lng = 2.0f * pi * (float)(j) / slices;
                        float x = cosf(lng);
                        float z = sinf(lng);
                        float u = (float)(j) / slices;

                        float v0 =(float)(i) / stacks;
                        float v1 =(float)(i + 1) / stacks;

                        glTexCoord2f(u, v0);
                        glVertex3f(x * r0 * radius, y0 * radius, z * r0 * radius);

                        glTexCoord2f(u, v1);
                        glVertex3f(x * r1 * radius, y1 * radius, z * r1 * radius);
                    }
                    glEnd();
                }

            glPopMatrix();
            glDisable(GL_TEXTURE_2D);


            //Sun
            glColor4f(1,1,1,1);
            glBegin(GL_TRIANGLE_FAN);
            glVertex3f(3,0,0);
            for (int i = 0; i <= 32; i++) {
                float angle = pi2 * i / 32;
                glColor4i(0, 0, 0,200);
                glVertex3f(4+i%2*2, cos(angle), sin(angle));
            }    
            glEnd();
            glEnable(GL_LIGHTING);

            //translation
            glEnable(GL_DEPTH_TEST); 
            glTranslatef(-pos.x, -pos.y, -pos.z);

            //far
            glEnable(GL_CULL_FACE);
            SetProjectionMatrix(10000.0f, 4000000000.0f);
            draw_object(&planet, GL_TRIANGLES);
            glPushMatrix();
                glTranslatef(0.0f, 0.0f, -5000000.0f); 
                glRotatef(-60, 1.0f, 0, 0);
                drawRings(1200000.0f, 2000000.0f, 1024, 100); //p1

            glPopMatrix(); 
            glClear(GL_DEPTH_BUFFER_BIT);

            //near
            SetProjectionMatrix(0.01f, 40000.0f);
            draw_object(&asteroid_obj, GL_TRIANGLES);

            //debug_object(&tmp_obj);




        glPopMatrix();

        //laser
        if (lasers){
            glDisable(GL_LIGHTING);
            glBegin(GL_QUADS);
            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glVertex3f(0.5f, -0.5f, 0.0f);
            glVertex3f(0.0f, 0.0f, -128.0f);
            glColor4f(1.0f, 0.0f, 0.0f, 0.0f);
            glVertex3f(0.7f, -0.5f, 0.0f);
            glVertex3f(0.0f, 0.0f, -128.0f);

            glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            glVertex3f(0.5f, -0.5f, 0.0f);
            glVertex3f(0.0f, 0.0f, -128.0f);
            glColor4f(1.0f, 0.0f, 0.0f, 0.0f);
            glVertex3f(0.35f, -0.5f, 0.0f);
            glVertex3f(0.0f, 0.0f, -128.0f);
            glEnd();
            glEnable(GL_LIGHTING);
        }
        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

        glClear(GL_DEPTH_BUFFER_BIT);

        // Reset transformations
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity(); 
        //redundancy check ^


        
        dRF += (RF-dRF)*0.01;
        dIR += (IR-dIR)*0.01;
        dVIS += (VIS-dVIS)*0.01;
        dRAD += (RAD - dRAD)*0.01;
        float sig[13];

        for (int i = 0; i < 4; i++) sig[i] = xorshift32f()/10 + dRF + (dIR - dRF) * i / 3.0f;

        for (int i = 0; i < 4; i++) sig[4 + i] =xorshift32f()/10+ dIR + (dVIS - dIR) * i / 3.0f;

        for (int i = 0; i < 5; i++) sig[8 + i] =xorshift32f()/10+ dVIS + (dRAD - dVIS) * i / 4.0f;

        //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
       glBegin(GL_LINES);

        for (int i = 0; i < 12; i++){
            
            glVertex2f((float)i*0.02 +0.5, sig[i]*0.1-0.5f);
            glVertex2f((float)(i+1)*0.02 +0.5, sig[i+1]*0.1-0.5f);

        }
        glLineWidth(2.5f);
        
        
        for (int i = 0; i < 10; i++) {
            glVertex3f(linePoints[i][0], linePoints[i][1], -0.2f);
        }
        glEnd();
        glPopMatrix(); 


        SwapBuffers(hDC);    
        }
    return (int)msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            ShowCursor(FALSE);
            SetCapture(hwnd);
            {
                // Clip cursor to client area
                RECT rect;
                GetClientRect(hwnd, &rect);
                POINT ul = {rect.left, rect.top};
                POINT lr = {rect.right, rect.bottom};
                ClientToScreen(hwnd, &ul);
                ClientToScreen(hwnd, &lr);
                RECT clipRect = {ul.x, ul.y, lr.x, lr.y};
                ClipCursor(&clipRect);
            }
            break;
        case WM_SIZE:
            GLsizei width = LOWORD(lParam);
            GLsizei height = HIWORD(lParam);
            if (height == 0) height = 1; 
            glViewport(0, 0, width, height);
            aspectRatio = (float)width / (float)height;
            SetProjectionMatrix(1.0f, 4000000000.0f);
            {
                // Update clip region on resize
                RECT rect;
                GetClientRect(hwnd, &rect);
                POINT ul = {rect.left, rect.top};
                POINT lr = {rect.right, rect.bottom};
                ClientToScreen(hwnd, &ul);
                ClientToScreen(hwnd, &lr);
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
        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                int keyBit = 0;
                switch (wParam) {
                    case 'W': keyBit = 0x01; break;
                    case 'A': keyBit = 0x02; break;
                    case 'S': keyBit = 0x04; break;
                    case 'D': keyBit = 0x08; break;
                    case 'Q': keyBit = 0x10; break;
                    case 'E': keyBit = 0x20; break;
                    case VK_SHIFT: keyBit = 0x40; break;
                    case VK_CONTROL: keyBit = 0x80; break;
                    case VK_UP: keyBit = 0x100; break;
                    case VK_DOWN: keyBit = 0x200; break;
                    case VK_LEFT: keyBit = 0x400; break;
                    case VK_RIGHT: keyBit = 0x800; break;
                    case VK_ESCAPE: 
                        ShowCursor(TRUE);
                        ReleaseCapture();
                        break;
                    default:
                        break; 
                }
                if (keyBit) {
                    if (uMsg == WM_KEYDOWN) {
                        keyState |= keyBit; //Set
                    } else if (uMsg == WM_KEYUP) {
                        keyState &= ~keyBit; //Clear
                    }
                }
            }
            break;
        case WM_MOUSEMOVE: 
            //for first person
            RECT windowRect;
            GetClientRect(hwnd, &windowRect);
            int centerX = (windowRect.left + windowRect.right) / 2;
            int centerY = (windowRect.top + windowRect.bottom) / 2;

            POINT centerScreen = {centerX, centerY};
            ClientToScreen(hwnd, &centerScreen);
            SetCursorPos(centerScreen.x, centerScreen.y);

            //view.x += (LOWORD(lParam) - centerX)*0.01;
            //view.y += (HIWORD(lParam) - centerY)*0.01;
            if (view.y > 89.9f) view.y = 89.9f;
            if (view.y < -89.9f) view.y = -89.9f;

            break;
        case WM_LBUTTONDOWN:
            playSoundEffect(SND_GUN, 440, 1, 0, 0);
            lasers = TRUE;
            break;
        case WM_LBUTTONUP:
            lasers = FALSE;
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
            
    }
    return 0;
}
