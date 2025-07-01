#include <windows.h>
#include <gl/gl.h>
#include "defs.h"

typedef struct {
    float* vertices;
    int vertex_count;
    float* normals;
} Mesh;

HGLRC hRC; // OpenGL Rendering Context
HDC hDC;   // Device Context
HWND hWnd; // Window Handle

float aspectRatio;

double playback = 1.0;
uint16_t keyState = 0;
BOOL lasers = FALSE;

Quat rot = {0.0f, 0.0f, 0.0f, 1.0f}; //Pitch, Yaw, Roll, W
//Quat view = {0.0f, 0.0f, 0.0f, 1.0f};
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

        // Increase noise frequency for more detail
        float n0 = value_noise1d((float)(i-1) * 1.2f);
        float n1 = value_noise1d((float)i * 1.2f);

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
/*
Mesh asteroid(float x, float y, double size){
    Mesh mesh;
    mesh.vertex_count = 128;
    mesh.vertices = (float*)malloc(sizeof(float) * 3 * mesh.vertex_count);
}*/

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

    // HUD texture data
    unsigned char textureData[256 * 512 * 3];  
    for (int i = 0; i < 256 * 512; ++i) {
        textureData[i * 3 + 0] = 127;  // Red
        textureData[i * 3 + 1] = 127;  // Green
        textureData[i * 3 + 2] = 127;  // Blue
    }
    
    int width = 256;
    int height = 512;
    
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int i = (y * width + x) * 3;
            
            int checker = ((x / 32) % 2) ^ ((y / 128) % 2);
            textureData[i + 0] = checker ? 255 : 0;
            textureData[i + 1] = checker ? 0 : 0;
            textureData[i + 2] = checker ? 255 : 0;
        }
    }



    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 512, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, textureData);

    GLuint starList = glGenLists(1);
    glNewList(starList, GL_COMPILE);
        glPushMatrix();
            glTranslatef(0.4f, 0.4f, 0.0f);
            glRotatef(40, 1,1,0);
            for (int i = 0; i < NUM_STARS; ++i) {
                float size = xorshift32f();
                glPointSize(size*2);
                glBegin(GL_POINTS);
                glColor4f(1.0f, 1.0f, 1.0f, size*0.8f);

                float x = xorshift32f()-xorshift32f();
                float y = xorshift32f()-xorshift32f();
                float z = (xorshift32f()-xorshift32f())/10;
                glVertex3f(x, y, z);
            }
            glEnd();
        glPopMatrix();
    glEndList();

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f}; 
    Quat Rvel = {0.0f, 0.0f, 0.0f, 1.0f};
    float thrust = 0.003f;

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


        float angle = 0.001f;
        Vec3 right = quat_rotate_vec3(Rvel, (Vec3){1, 0, 0});
        Vec3 up    = quat_rotate_vec3(Rvel, (Vec3){0, 1, 0});
        Vec3 fwd   = quat_rotate_vec3(Rvel, (Vec3){0, 0, 1});

        // Then, apply rotation around these world-space vectors
        if (keyState & 0x01) {
            Rvel = quat_mul(quat_axis_angle(right.x, right.y, right.z,  angle), Rvel);
        }
        if (keyState & 0x04) {
            Rvel = quat_mul(quat_axis_angle(right.x, right.y, right.z, -angle), Rvel);
        }
        if (keyState & 0x02) {
            Rvel = quat_mul(quat_axis_angle(up.x, up.y, up.z, -angle), Rvel);
        }
        if (keyState & 0x08) {
            Rvel = quat_mul(quat_axis_angle(up.x, up.y, up.z,  angle), Rvel);
        }        


        if (keyState & 0x40) { //dir.y += 1;
            playSoundEffect(SND_BEEP, 60, 0.85f, 0, 3);
        }// Shift
        if (keyState & 0x80) { dir.y -= 1; }// Ctrl   
        if (keyState & 0x100) { IR+=0.5f; dir.z -= 100000; } // Up
        if (keyState & 0x200) { dir.z += 1; } // Down
        if (keyState & 0x400) { dir.x -= 1; } // Left
        if (keyState & 0x800) { dir.x += 1; } // Right
        if (keyState & 0x10) { Rvel = quat_mul(quat_axis_angle(0, 0, 1, -0.001), Rvel); }// Q
        if (keyState & 0x20) { Rvel = quat_mul(quat_axis_angle(0, 0, 1, 0.001), Rvel); }// E
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

        pos.x += vel.x;
        pos.y += vel.y;
        pos.z += vel.z;
        
        glLoadIdentity();


        SetProjectionMatrix(0.0001f, 4.0f);
        glPushMatrix();

            //Rotation
            //Rvel = quat_normalize(Rvel);
            rot = quat_mul(Rvel, rot);
            rot = quat_normalize(rot);
            //Quat trot = quat_mul(view, rot);
            float mat[16];
            quat_to_matrix(&rot, mat);
            glMultMatrixf(mat);

            glRotatef(view.x, 0,1,0);
            glRotatef(view.y, 1,0,0);


            GLfloat light_pos[] = { 4.0f, 0.0f, 0.0f, 0.0f };
            glLightfv(GL_LIGHT0, GL_POSITION, light_pos);

            glDisable(GL_LIGHTING);

            //Stars
            glCallList(starList);

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
            glPushMatrix(); //2
                glTranslatef(0.0f, 0.0f, -5000000.0f); 
                drawPlanet(1000000.0f, 20, 20); //p1
                drawRings(1200000.0f, 2000000.0f, 1024, 45); //p1
            glPopMatrix(); //2

            glClear(GL_DEPTH_BUFFER_BIT);

            //near
            SetProjectionMatrix(0.01f, 40000.0f);
            glPushMatrix(); //2
                glTranslatef(0.0f, 0.0f, -5.0f);
                glBegin(GL_TRIANGLES);
                glColor3f(0.0f, 1.0f, 0.0f); 
                glVertex3f(0.0f, 1.0f, 0.0f);
                glVertex3f(-1.0f, -1.0f, 0.0f);
                glVertex3f(1.0f, -1.0f, 0.0f);
                glEnd();
            glPopMatrix(); //2
        glPopMatrix(); //1

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


        glClear(GL_DEPTH_BUFFER_BIT);

        // Reset transformations
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Set orthographic projection for HUD
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity(); 
        

        // HUD 
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_LIGHTING);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
            // Main HUD texture
            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, textureID);
    
           glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(-1.0f, -1.0f);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(1.0f, -1.0f);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(1.0f, -0.2f);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(-1.0f, -0.2f);
            glEnd();
            glDisable(GL_TEXTURE_2D);
        
        dRF += (RF-dRF)*0.01;
        dIR += (IR-dIR)*0.01;
        dVIS += (VIS-dVIS)*0.01;
        dRAD += (RAD - dRAD)*0.01;
        float sig[13];

        for (int i = 0; i < 4; i++)
        sig[i] = xorshift32f()/10 + dRF + (dIR - dRF) * i / 3.0f;

        for (int i = 0; i < 4; i++)
        sig[4 + i] =xorshift32f()/10+ dIR + (dVIS - dIR) * i / 3.0f;

        for (int i = 0; i < 5; i++)
        sig[8 + i] =xorshift32f()/10+ dVIS + (dRAD - dVIS) * i / 4.0f;
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
            break;
        case WM_SIZE:
            GLsizei width = LOWORD(lParam);
            GLsizei height = HIWORD(lParam);
            if (height == 0) height = 1; 
            glViewport(0, 0, width, height);
            aspectRatio = (float)width / (float)height;
            SetProjectionMatrix(1.0f, 4000000000.0f);
            break;
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_DESTROY:
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

            //rot = quat_mul(quat_axis_angle(0, 1, 0, (LOWORD(lParam) - centerX)*0.01), rot);//qYaw
            //rot = quat_mul(quat_axis_angle(1, 0, 0, (HIWORD(lParam) - centerY)*0.01), rot);//qPitch
            //view.x += (LOWORD(lParam) - centerX)*0.01;
            //view.y += (HIWORD(lParam) - centerY)*0.01;

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