#include <windows.h>
#include <gl/gl.h>
#include "defs.h"

typedef struct {
    float* vertices;
    int vertex_count;
    float* normals;
    float* colors; // RGB per vertex
} Mesh;

HGLRC hRC; // OpenGL Rendering Context
HDC hDC;   // Device Context
HWND hWnd; // Window Handle

float aspectRatio;

double playback = 1.0;
uint16_t keyState = 0;
BOOL lasers = FALSE;

Quat rot = {0.0f, 0.0f, 0.0f, 1.0f}; //Pitch, Yaw, Roll, W
Vec3 angVel = {0}; // current angular velocity vector (radians/sec)
float dt = 0.01;

Vec2 view = {0.0f, 0.0f};
Vec3 pos = {0.0f, 0.0f, 0.0f};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void playSoundEffect(int type, float pitch, float volume, float pan, int channel);
void stopSound(int channel);

Quat quat_mul(Quat a, Quat b) {
    Quat q;
    q.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
    q.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
    q.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
    q.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
    return q;
}
Quat quat_axis_angle(float x, float y, float z, float angle_rad) {
    float s = sinf(angle_rad * 0.5f);
    Quat q;
    q.x = x * s;
    q.y = y * s;
    q.z = z * s;
    q.w = cosf(angle_rad * 0.5f);
    return q;
}
Quat quat_normalize(Quat q) {
    float mag = sqrtf(q.x*q.x + q.y*q.y + q.z*q.z + q.w*q.w);
    q.x /= mag;
    q.y /= mag;
    q.z /= mag;
    q.w /= mag;
    return q;
}
static inline void quat_to_matrix(const Quat* q, float* m) {
    float x2 = q->x + q->x, y2 = q->y + q->y, z2 = q->z + q->z;
    float xx = q->x * x2, yy = q->y * y2, zz = q->z * z2;
    float xy = q->x * y2, xz = q->x * z2, yz = q->y * z2;
    float wx = q->w * x2, wy = q->w * y2, wz = q->w * z2;

    m[0] = 1.0f - (yy + zz);
    m[1] = xy + wz;
    m[2] = xz - wy;
    m[3] = 0.0f;

    m[4] = xy - wz;
    m[5] = 1.0f - (xx + zz);
    m[6] = yz + wx;
    m[7] = 0.0f;

    m[8]  = xz + wy;
    m[9]  = yz - wx;
    m[10] = 1.0f - (xx + yy);
    m[11] = 0.0f;

    m[12] = m[13] = m[14] = 0.0f;
    m[15] = 1.0f;
}
Vec3 quat_rotate_vec3(Quat q, Vec3 v) {
    // q * v * conj(q)
    Vec3 out;
    
    Vec3 u = { q.x, q.y, q.z };
    
    Vec3 uv = {
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x
    };
    
    Vec3 uuv = {
        u.y * uv.z - u.z * uv.y,
        u.z * uv.x - u.x * uv.z,
        u.x * uv.y - u.y * uv.x
    };

    uv.x *= 2.0f * q.w;
    uv.y *= 2.0f * q.w;
    uv.z *= 2.0f * q.w;

    uuv.x *= 2.0f;
    uuv.y *= 2.0f;
    uuv.z *= 2.0f;

    out.x = v.x + uv.x + uuv.x;
    out.y = v.y + uv.y + uuv.y;
    out.z = v.z + uv.z + uuv.z;

    return out;
}
Quat quat_conjugate(Quat q) {
    return (Quat){ -q.x, -q.y, -q.z, q.w };
}
void SetProjectionMatrix(float zNear, float zFar) {
    float fovY = 90.0f;
    float f = 1.0f / meTanf((fovY * pi / 180.0f) / 2.0f); // Convert degrees to radians
    float mat[16] = {0};

    mat[0] = f / aspectRatio;
    mat[5] = f;
    mat[10] = (zFar + zNear) / (zNear - zFar);
    mat[11] = -1.0f;
    mat[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    mat[15] = 0.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(mat);
    glMatrixMode(GL_MODELVIEW);
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


void draw_mesh(const Mesh* mesh) {
    if (!mesh || mesh->vertex_count <= 0 || !mesh->vertices) return;

    glBegin(GL_TRIANGLES);
    for (int i = 0; i < mesh->vertex_count; ++i) {
        if (mesh->normals) glNormal3fv(&mesh->normals[i * 3]);
        if (mesh->colors) glColor3fv(&mesh->colors[i * 3]);
        glVertex3fv(&mesh->vertices[i * 3]);
    }
    glEnd();
}

Mesh generate_sphere(int slices, int stacks, float radius) {
    int tris_per_quad = 6;
    int total_quads = slices * stacks;
    int vertex_count = total_quads * tris_per_quad;

    float* verts = malloc(sizeof(float) * vertex_count * 3);
    float* norms = malloc(sizeof(float) * vertex_count * 3);
    float* cols  = malloc(sizeof(float) * vertex_count * 3);

    int index = 0;

    for (int i = 0; i < slices; ++i) {
        float theta1 = (float)i / slices * 2.0f * pi;
        float theta2 = (float)(i + 1) / slices * 2.0f * pi;

        for (int j = 0; j < stacks; ++j) {
            float phi1 = (float)j / stacks * pi - pi / 2.0f;
            float phi2 = (float)(j + 1) / stacks * pi - pi / 2.0f;

            float x[4], y[4], z[4];

            x[0] = cosf(phi1) * cosf(theta1);
            y[0] = sinf(phi1);
            z[0] = cosf(phi1) * sinf(theta1);

            x[1] = cosf(phi1) * cosf(theta2);
            y[1] = sinf(phi1);
            z[1] = cosf(phi1) * sinf(theta2);

            x[2] = cosf(phi2) * cosf(theta2);
            y[2] = sinf(phi2);
            z[2] = cosf(phi2) * sinf(theta2);

            x[3] = cosf(phi2) * cosf(theta1);
            y[3] = sinf(phi2);
            z[3] = cosf(phi2) * sinf(theta1);

            int tri_indices[6] = {0, 1, 2, 2, 3, 0};

            for (int k = 0; k < 6; ++k) {
                int vi = tri_indices[k];

                verts[index * 3 + 0] = x[vi] * radius;
                verts[index * 3 + 1] = y[vi] * radius;
                verts[index * 3 + 2] = z[vi] * radius;

                norms[index * 3 + 0] = x[vi];
                norms[index * 3 + 1] = y[vi];
                norms[index * 3 + 2] = z[vi];

                cols[index * 3 + 0] = 0.6f;
                cols[index * 3 + 1] = 0.6f;
                cols[index * 3 + 2] = 0.6f;

                index++;
            }
        }
    }

    Mesh sphere = { verts, vertex_count, norms, cols };
    return sphere;
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

            float r0 = dark_brown[0] + (light_tan[0] - dark_brown[0]) * band0;
            float g0 = dark_brown[1] + (light_tan[1] - dark_brown[1]) * band0;
            float b0 = dark_brown[2] + (light_tan[2] - dark_brown[2]) * band0;

            float r1 = dark_brown[0] + (light_tan[0] - dark_brown[0]) * band1;
            float g1 = dark_brown[1] + (light_tan[1] - dark_brown[1]) * band1;
            float b1 = dark_brown[2] + (light_tan[2] - dark_brown[2]) * band1;

            glColor3f(r0, g0, b0);
            glNormal3f(x * zr0, y * zr0, z0); 
            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);

            glColor3f(r1, g1, b1);
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
        float red   = base * 0.3f;    
        float green = base * 0.6f;   
        float blue  = base * 1.0f;       
        float alpha = 0.3f + 0.5f * fabs(cos(b * 0.3f));
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2 * pi * i / segments;
            float x = cos(angle);
            float y = sin(angle);

            glNormal3f((x+1)/2, y, 0.0f);
            glColor4f(red, green, blue, alpha); glVertex2f(x * r0, y * r0);
            glNormal3f((x+1)/2, y, 0.0f);
            glColor4f(red, green, blue, alpha); glVertex2f(x * r1, y * r1);
        }
        glEnd();
    }

}


void tick() {
    
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

    glShadeModel(GL_SMOOTH);
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

        if (index >=2048 * 1024 * 3) continue;
        int r = starData[index + 0] + (int)(101 * w);
        int g = starData[index + 1] + (int)(67 * w);
        int b = starData[index + 2] + (int)(50 * w);

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





    Mesh gain_igger;









    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f}; 
    Quat Rvel = {0.0f, 0.0f, 0.0f, 1.0f};
    float thrust = 0.009f;
    //float thrust = 3000.0f;

    MSG msg;

    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    
    //MAIN LOOP
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_LIGHTING);
        glEnable(GL_BLEND);
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
            playSoundEffect(SND_BEEP, 60, 0.85f, 0, 3);
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
                glRotatef(30, 1.0f, 0, 0);
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
            SetProjectionMatrix(10000.0f, 4000000000.0f);
            glPushMatrix();
                glTranslatef(0.0f, 0.0f, -5000000.0f); 
                drawPlanet(1000000.0f, 200, 200); //p1
                drawRings(1200000.0f, 2000000.0f, 1024, 45); //p1
            glPopMatrix(); 

            glClear(GL_DEPTH_BUFFER_BIT);

            //near
            SetProjectionMatrix(0.01f, 40000.0f);
            
            glPushMatrix();
                glTranslatef(0.0f, 0.0f, -5.0f);
                glBegin(GL_TRIANGLES);
                glColor3f(0.0f, 1.0f, 0.0f); 
                glVertex3f(0.0f, 1.0f, 0.0f);
                glVertex3f(-1.0f, -1.0f, 0.0f);
                glVertex3f(1.0f, -1.0f, 0.0f);
                glEnd();





                Mesh rock = generate_sphere(67, 67, 1);
                draw_mesh(&rock);
            glPopMatrix();






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

            view.x += (LOWORD(lParam) - centerX)*0.01;
            view.y += (HIWORD(lParam) - centerY)*0.01;
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