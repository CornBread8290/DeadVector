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

#define EPSILON 1e-5f
int vec3_equal(Vec3 a, Vec3 b) {
    return fabsf(a.x - b.x) < EPSILON &&
           fabsf(a.y - b.y) < EPSILON &&
           fabsf(a.z - b.z) < EPSILON;
}
Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){ a.x - b.x, a.y - b.y, a.z - b.z };
}
Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}
float vec3_length(Vec3 v) {
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}
Vec3 vec3_normalize(Vec3 v) {
    float len = vec3_length(v);
    return len > EPSILON ? (Vec3){ v.x / len, v.y / len, v.z / len } : (Vec3){ 0, 0, 0 };
}
Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){ a.x + b.x, a.y + b.y, a.z + b.z };
}
typedef struct {
    Vec3 position;
    Vec3 accumulated;
    int count;
} VertexNormalEntry;

typedef struct {
    VertexNormalEntry* data;
    int count;
    int capacity;
} NormalMap;

void normal_map_init(NormalMap* map) {
    map->capacity = 256;
    map->count = 0;
    map->data = malloc(sizeof(VertexNormalEntry) * map->capacity);
}

void normal_map_add(NormalMap* map, Vec3 position, Vec3 normal) {
    for (int i = 0; i < map->count; ++i) {
        if (vec3_equal(map->data[i].position, position)) {
            map->data[i].accumulated = vec3_add(map->data[i].accumulated, normal);
            map->data[i].count += 1;
            return;
        }
    }

    if (map->count >= map->capacity) {
        map->capacity *= 2;
        map->data = realloc(map->data, sizeof(VertexNormalEntry) * map->capacity);
    }

    map->data[map->count].position = position;
    map->data[map->count].accumulated = normal;
    map->data[map->count].count = 1;
    map->count += 1;
}

Vec3 normal_map_get(NormalMap* map, Vec3 position) {
    for (int i = 0; i < map->count; ++i) {
        if (vec3_equal(map->data[i].position, position)) {
            return vec3_normalize(map->data[i].accumulated);
        }
    }
    return (Vec3){ 0, 0, 0 };
}

void normal_map_free(NormalMap* map) {
    free(map->data);
}
void smooth_normals(Mesh* mesh) {
    if (!mesh || mesh->vertex_count % 3 != 0) return;

    NormalMap map;
    normal_map_init(&map);

    for (int i = 0; i < mesh->vertex_count; i += 3) {
        Vec3 a = mesh->vertices[i];
        Vec3 b = mesh->vertices[i + 1];
        Vec3 c = mesh->vertices[i + 2];

        Vec3 u = vec3_sub(b, a);
        Vec3 v = vec3_sub(c, a);
        Vec3 face_normal = vec3_normalize(vec3_cross(u, v));

        normal_map_add(&map, a, face_normal);
        normal_map_add(&map, b, face_normal);
        normal_map_add(&map, c, face_normal);
    }

    for (int i = 0; i < mesh->vertex_count; ++i) {
        mesh->normals[i] = normal_map_get(&map, mesh->vertices[i]);
    }

    normal_map_free(&map);
}




void generate_asteroid_cubesphere(Mesh* mesh, Vec3 center, float radius, Color4 color, int subdivisions) {
    if (subdivisions < 1) subdivisions = 1;

    int quads_per_face = subdivisions * subdivisions;
    int total_quads = 6 * quads_per_face;
    int estimated_triangles = total_quads * 2;

    init_mesh(mesh, estimated_triangles * 3);

    float step = 2.0f / subdivisions;

    Vec3 face_normals[6] = {
        {  1,  0,  0 }, // +X
        { -1,  0,  0 }, // -X
        {  0,  1,  0 }, // +Y
        {  0, -1,  0 }, // -Y
        {  0,  0,  1 }, // +Z
        {  0,  0, -1 }  // -Z
    };

    // Loop through each face
    for (int f = 0; f < 6; f++) {
        Vec3 normal = face_normals[f];

        Vec3 axis_a = { 0, 0, 0 };
        Vec3 axis_b = { 0, 0, 0 };

        if (fabsf(normal.x) > 0.5f) {
            axis_a.z = 1;
            axis_b.y = 1;
        } else if (fabsf(normal.y) > 0.5f) {
            axis_a.x = 1;
            axis_b.z = 1;
        } else {
            axis_a.x = 1;
            axis_b.y = 1;
        }

        for (int i = 0; i < subdivisions; i++) {
            for (int j = 0; j < subdivisions; j++) {
                float x0 = -1.0f + step * i;
                float x1 = x0 + step;
                float y0 = -1.0f + step * j;
                float y1 = y0 + step;

                Vec3 p00 = {
                    normal.x + axis_a.x * x0 + axis_b.x * y0,
                    normal.y + axis_a.y * x0 + axis_b.y * y0,
                    normal.z + axis_a.z * x0 + axis_b.z * y0
                };
                Vec3 p10 = {
                    normal.x + axis_a.x * x1 + axis_b.x * y0,
                    normal.y + axis_a.y * x1 + axis_b.y * y0,
                    normal.z + axis_a.z * x1 + axis_b.z * y0
                };
                Vec3 p01 = {
                    normal.x + axis_a.x * x0 + axis_b.x * y1,
                    normal.y + axis_a.y * x0 + axis_b.y * y1,
                    normal.z + axis_a.z * x0 + axis_b.z * y1
                };
                Vec3 p11 = {
                    normal.x + axis_a.x * x1 + axis_b.x * y1,
                    normal.y + axis_a.y * x1 + axis_b.y * y1,
                    normal.z + axis_a.z * x1 + axis_b.z * y1
                };

                // Project to sphere and apply deformation
                Vec3 verts[4] = { p00, p10, p01, p11 };
                Vec3 sphere_verts[4];

                for (int v = 0; v < 4; v++) {
                    Vec3 dir = verts[v];
                    float len = sqrtf(dir.x * dir.x + dir.y * dir.y + dir.z * dir.z);
                    if (len > 0.0001f) {
                        dir.x /= len;
                        dir.y /= len;
                        dir.z /= len;
                    }

                    // BIG base shape
                    float base_shape = fbm(dir, 3, 0.6f, 1.5f);
                    float deform1 = 1.0f + base_shape * 0.7f;

                    // DETAIL surface bumps
                    Vec3 bump_dir = { dir.x * 3.0f, dir.y * 3.0f, dir.z * 3.0f };
                    float surface_noise = fbm(bump_dir, 5, 0.5f, 2.5f);
                    float deform2 = 1.0f + surface_noise * 0.2f;

                    float deform = deform1 * deform2;

                    sphere_verts[v].x = center.x + dir.x * radius * deform;
                    sphere_verts[v].y = center.y + dir.y * radius * deform;
                    sphere_verts[v].z = center.z + dir.z * radius * deform;
                }

            BOOL flip = (f == 0 || f == 2 || f == 5); // +X, +Y, -Z

            if (flip) {
                add_triangle(mesh, sphere_verts[0], sphere_verts[2], sphere_verts[1], color);
                add_triangle(mesh, sphere_verts[2], sphere_verts[3], sphere_verts[1], color);
            } else {
                add_triangle(mesh, sphere_verts[0], sphere_verts[1], sphere_verts[2], color);
                add_triangle(mesh, sphere_verts[2], sphere_verts[1], sphere_verts[3], color);
            }
            }
        }
    }
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


void drawPlanet(float radius, int lats, int longs) {
    for (int i = 0; i <= lats; ++i) {
        float lat0 = pi * (-0.5f + (float)(i - 1) / lats);
        float z0  = sinf(lat0);
        float zr0 = cosf(lat0);

        float lat1 = pi * (-0.5f + (float)i / lats);
        float z1 = sinf(lat1);
        float zr1 = cosf(lat1);

        float n0 = value_noise1d((float)(i-1) * 0.15f);
        n0 += 0.5f * value_noise1d((float)(i-1) * 0.35f + 100.0f);
        n0 += 0.25f * value_noise1d((float)(i-1) * 1.0f + 200.0f);
        n0 /= 1.75f; // Normalize

        float n1 = value_noise1d((float)i * 0.15f);
        n1 += 0.5f * value_noise1d((float)i * 0.35f + 100.0f);
        n1 += 0.25f * value_noise1d((float)i * 1.0f + 200.0f);
        n1 /= 1.75f; // Normalize


        float dark_brown[3] = {0.26f, 0.16f, 0.07f};
        float light_tan[3]  = {0.90f, 0.80f, 0.60f};

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= longs; ++j) {
            float lng = 2.0f * pi * (float)(j - 1) / longs;
            float x = cosf(lng);
            float y = sinf(lng);

            float band0 = 0.25f + 0.75f * n0;
            float band1 = 0.25f + 0.75f * n1;

            Color3 c0 = {
            dark_brown[0] + (light_tan[0] - dark_brown[0]) * band0,
            dark_brown[1] + (light_tan[1] - dark_brown[1]) * band0,
            dark_brown[2] + (light_tan[2] - dark_brown[2]) * band0,
            };

            Color3 c1 = {
            dark_brown[0] + (light_tan[0] - dark_brown[0]) * band1,
            dark_brown[1] + (light_tan[1] - dark_brown[1]) * band1,
            dark_brown[2] + (light_tan[2] - dark_brown[2]) * band1,
            };

            glColor3f(c0.r, c0.g, c0.b);
            glNormal3f(x * zr0, y * zr0, z0); 
            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);

            glColor3f(c1.r, c1.g, c1.b);
            glNormal3f(x * zr1, y * zr1, z1); 
            glVertex3f(radius * x * zr1, radius * y * zr1, radius * z1);
        }
        glEnd();
    }
}
void drawRings(float innerRadius, float outerRadius, int segments, int bands) {
    float bandStep = (outerRadius - innerRadius) / bands;

    for (int b = 0; b < bands; ++b) {
        float r0 = innerRadius + b * bandStep;
        float r1 = r0 + bandStep;

        float base = 0.6f + 0.3f * sin(b * 0.5f); 
        Color4 hue;
        hue.r= base * 0.3f;    
        hue.g = base * 0.6f;   
        hue.b  = base * 1.0f;       
        hue.a = 0.3f + 0.5f * fabs(cos(b * 0.3f));
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2 * pi * i / segments;
            Vec2 vec = {cos(angle), sin(angle)};

            glNormal3f(vec.x/2, vec.y, 0.0f);
            glColor4fv(hue.data); glVertex2f(vec.x * r0, vec.y * r0);
            glNormal3f(vec.x/2, vec.y, 0.0f);
            glColor4fv(hue.data); glVertex2f(vec.x * r1, vec.y * r1);
        }
        glEnd();
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




/*
    Mesh tmp_ship;
    init_mesh(&tmp_ship, 64);
    add_cube(&tmp_ship, (Vec3){0,0,0}, 1, (Color4){1.0f,1.0f,1.0f,1.0f});
    add_triangle(&tmp_ship, (Vec3){3, 2, 1}, (Vec3){1, 1, 1}, (Vec3){1, 2, 3}, (Color4){1.0f, 0.0f, 0.0f, 1.0f});
    add_triangle(&tmp_ship, (Vec3){2,0,-1}, (Vec3){2, 0, -2}, (Vec3){2, 1, -1.5f}, (Color4){0.0f, 1.0f, 1.0f, 1.0f});
    Object tmp_obj = {
        .position = {0.0f, 0.0f, -3.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh = tmp_ship,
        .flags = 0,
        .components = NULL
    };
*/
    Mesh asteroid;
    generate_asteroid_cubesphere(&asteroid, (Vec3){0.0f, 0.0f, -5.0f}, 1.0f, (Color4){0.5f, 0.5f, 0.5f, 1.0f}, 256);
    smooth_normals(&asteroid);
    Object asteroid_obj = {
        .position = {0.0f, 0.0f, -5.0f},
        .rotation = {0.0f, 0.0f, 0.0f, 1.0f},
        .scale = {1.0f, 1.0f, 1.0f},
        .mesh = asteroid,
        .flags = 0,
        .components = NULL
    };
    GLuint asteroids = glGenLists(1);
    glNewList(asteroids, GL_COMPILE);
        draw_mesh(&asteroid, GL_TRIANGLES);
    glEndList();



    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f}; 
    Quat Rvel = {0.0f, 0.0f, 0.0f, 1.0f};
    float thrust = 0.03f;
    //float thrust = 3000.0f;
    float dt = 0.018;


    MSG msg;

    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

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

            glRotatef(view.y, 1,0,0);
            glRotatef(view.x, 0,1,0);



            //Rotation
            rot = quat_normalize(rot);
            float mat[16];
            quat_to_matrix(&rot, mat);
            glMultMatrixf(mat);


            GLfloat light_pos[] = { 4.0f, 0.0f, 0.0f, 0.0f };
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
            glPushMatrix();
                glTranslatef(0.0f, 0.0f, -5000000.0f); 
                glRotatef(-60, 1.0f, 0, 0);
                drawPlanet(1000000.0f, 200, 200); //p1
                drawRings(1200000.0f, 2000000.0f, 1024, 45); //p1

            glPopMatrix(); 
            glClear(GL_DEPTH_BUFFER_BIT);

            //near
            SetProjectionMatrix(0.01f, 40000.0f);
            /*
            tmp_obj.rotation = quat_mul(tmp_obj.rotation,  quat_axis_angle(0.0f, 1.0f, 0.0f, 0.01f));
            Vec3 move = direction_between(tmp_obj.position, pos);
            normalize(&move);
            multiply3f(&move, 0.01f);
            tmp_obj.position.x += move.x;
            tmp_obj.position.y += move.y;
            tmp_obj.position.z += move.z;
            */
            //draw_object(&tmp_obj, GL_TRIANGLES);
            glCallList(asteroids);
            //draw_object(&asteroid_obj, GL_TRIANGLES);
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