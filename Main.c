#include <windows.h>
#include <gl/gl.h>
#include <math.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;

typedef struct { float x, y, z; } Vec3;
typedef struct { float x, y, z, w; } Quat;

HGLRC hRC; // OpenGL Rendering Context
HDC hDC;   // Device Context
HWND hWnd; // Window Handle

uint16_t keyState = 0;
#define pi 3.14159265358979323846f
#define pi2 (pi * 2.0f)




LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

Quat rot = {0.0f, 0.0f, 0.0f, 1.0f};
Vec3 pos = {0.0f, 0.0f, 0.0f};



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
void quat_to_matrix(const Quat* q, float* m) {
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
    
    // Extract the vector part of the quaternion
    Vec3 u = { q.x, q.y, q.z };
    
    // Cross products
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

    // Apply rotation
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

void ResizeGLScene(GLsizei width, GLsizei height) {
    if (height == 0) height = 1; // Prevent division by zero
    glViewport(0, 0, width, height);

    float fovY = 90.0f;
    float aspectRatio = (float)width / (float)height;
    float zNear = 1.0f;
    float zFar = 10000000000000.0f;

    float f = 1.0f / tanf((fovY * pi / 180.0f) / 2.0f); // Convert degrees to radians

    float projectionMatrix[16] = {0};
    projectionMatrix[0] = f / aspectRatio;
    projectionMatrix[5] = f;
    projectionMatrix[10] = (zFar + zNear) / (zNear - zFar);
    projectionMatrix[11] = -1.0f;
    projectionMatrix[14] = (2.0f * zFar * zNear) / (zNear - zFar);
    projectionMatrix[15] = 0.0f;

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(projectionMatrix); // Load the custom projection matrix
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void Cleanup() {
    if (hRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hRC);
        hRC = NULL;
    }
    if (hDC) {
        ReleaseDC(hWnd, hDC);
        hDC = NULL;
    }
}


static uint32_t seed = 12355;
uint32_t xorshift32() {
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}


#define SAMPLE_RATE 8000

enum SoundType {
    SND_BEEP = 0,
    SND_ENGINE,
    SND_GUN,
    SND_HARSH
};

DWORD WINAPI playThread(LPVOID p) {
    PlaySoundA((LPCSTR)((uint8_t*)p), NULL, SND_MEMORY | SND_ASYNC);
    HeapFree(GetProcessHeap(), 0, p);
    return 0;
}
void playSoundEffect(int type, float pitch, float duration, float volume) {
    int NUM_SAMPLES = ((int)(SAMPLE_RATE * duration));
    typedef struct {
        uint8_t buffer[44 + NUM_SAMPLES];
    } SoundData;

    SoundData* snd = (SoundData*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SoundData));
    if (!snd) return;

    uint8_t* buf = snd->buffer;
    memcpy(buf, "RIFF", 4);
    *(uint32_t*)(buf + 4) = 36 + NUM_SAMPLES;
    memcpy(buf + 8, "WAVEfmt ", 8);
    *(uint32_t*)(buf + 16) = 16;
    *(uint16_t*)(buf + 20) = 1;
    *(uint16_t*)(buf + 22) = 1;
    *(uint32_t*)(buf + 24) = SAMPLE_RATE;
    *(uint32_t*)(buf + 28) = SAMPLE_RATE;
    *(uint16_t*)(buf + 32) = 1;
    *(uint16_t*)(buf + 34) = 8;
    memcpy(buf + 36, "data", 4);
    *(uint32_t*)(buf + 40) = NUM_SAMPLES;

    for (int i = 0; i < NUM_SAMPLES; ++i) {
        float t = (float)i / SAMPLE_RATE;
        float s = 0;

        switch (type) {
            case SND_BEEP: {
                float env = expf(-5.0f * t);
                s = sinf(2.0f * 3.14159f * pitch * t);
                break;
            }
            case SND_ENGINE: {
                float noise = (((int)xorshift32() % 256) - 128) / 128.0f * 0.3f;
                float hum = sinf(2.0f * 3.14159f * pitch * t) * 0.6f;
                s = (noise + hum) * 0.5f;
                break;
            }
            case SND_GUN: {
                float decay = expf(-20.0f * t);
                s = ((rand() % 2) ? 1.0f : -1.0f) * decay;
                s *= sinf(2.0f * 3.14159f * pitch * t); 
                break;
            }
            case SND_HARSH: {
                float noise = (((int)xorshift32() % 256) - 128) / 128.0f * 0.3f;
                float hum = sinf(2.0f * 3.14159f * pitch * t) * 0.6f;
                s = (noise + hum) * 0.5f;
                break;
            }
        }

        int v = (int)((s + 1.0f) * 127.5f);
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        buf[44 + i] = (uint8_t)v;
    }

    CreateThread(NULL, 0, playThread, snd, 0, NULL);
}

#define NUM_STARS 64
float star_positions[NUM_STARS][3];


void init_stars() {
    float dist = 100.0f;

    for (int i = 0; i < NUM_STARS; ++i) {
        float theta = (xorshift32() / 4294967295.0f) * pi2;  // 4294967295 = 2³²-1
        float phi = (xorshift32() / 4294967295.0f) * pi;
        float sin_phi = sinf(phi);
        
        star_positions[i][0] = dist * sin_phi * cosf(theta);
        star_positions[i][1] = dist * sin_phi * sinf(theta);
        star_positions[i][2] = dist * cosf(phi);    }
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    const char *className = "OGL";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "error in register window class.", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    hWnd = CreateWindowEx(0, className, "Dead Vector", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    //OpenGL initialization
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

    init_stars(); // Initialize star positions

    // End of OpenGL initialization

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f}; 
    Vec3 spn = {0.0f, 0.0f, 0.0f};
    float thrust = 0.003f;

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


        Vec3 dir = {0};
        if (keyState & 0x01) { dir.z -= 1; } // W
        if (keyState & 0x04) { dir.z += 1; } // S
        if (keyState & 0x02) { dir.x -= 1; } // A
        if (keyState & 0x08) { dir.x += 1; } // D
        if (keyState & 0x40) { dir.y += 1; }// Shift
        if (keyState & 0x80) { dir.y -= 1; }// Ctrl   
        if (keyState & 0x100) {   } // Up


        Quat invRot = quat_conjugate(rot);
        Vec3 worldDir = quat_rotate_vec3(invRot, dir);

        vel.x += worldDir.x * thrust;
        vel.y += worldDir.y * thrust;
        vel.z += worldDir.z * thrust;

        pos.x += vel.x;
        pos.y += vel.y;
        pos.z += vel.z;
        
        if (keyState & 0x10) {  // Q
            Quat dq = quat_axis_angle(0, 0, 1, -0.02);
            rot = quat_mul(dq, rot);
        }
        if (keyState & 0x20) {  // E
          Quat dq = quat_axis_angle(0, 0, 1, 0.02);
         rot = quat_mul(dq, rot);
        }
    



        glLoadIdentity();

        rot = quat_normalize(rot); // keep it tight, babe 💅
        
        float mat[16];
        quat_to_matrix(&rot, mat);
        
        glDisable(GL_DEPTH_TEST);

        glBegin(GL_LINES);
        glLineWidth(100.0f);
        glColor3f(0.5f, 0.5f, 0.5f);
        glVertex3f(-1.0f, 0.0f, -2.0f);
        glVertex3f(1.0f, 0.0f, -2.0f);
        glEnd();

        glMultMatrixf(mat); // rotation matrix applied
        
        glPointSize(2.0f); 
        glBegin(GL_POINTS);
        glColor3f(1.0f, 1.0f, 1.0f);
        for (int i = 0; i < NUM_STARS; ++i) {
            glVertex3fv(star_positions[i]);
        }
        glEnd();

        glEnable(GL_DEPTH_TEST); 
        glTranslatef(-pos.x, -pos.y, -pos.z);
        
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -50000000000.0f); 
        glBegin(GL_TRIANGLES);
        glColor3f(1.0f, 0.0f, 0.0f); 
        glVertex3f(0.0f, 10000000000.0f, 0.0f);
        glVertex3f(-10000000000.0f, -10000000000.0f, 0.0f);
        glVertex3f(10000000000.0f, -10000000000.0f, 0.0f);
        glEnd();
        glPopMatrix();
        
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -5.0f);
        glBegin(GL_TRIANGLES);
        glColor3f(0.0f, 1.0f, 0.0f); // green 
        glVertex3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 0.0f);
        glEnd();
        glPopMatrix();
        



        SwapBuffers(hDC);    
        }

    Cleanup();
    return (int)msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_CREATE:
            ShowCursor(FALSE);
            SetCapture(hwnd);
            break;
        case WM_SIZE:
            ResizeGLScene(LOWORD(lParam), HIWORD(lParam));
            break;
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_DESTROY:
            Cleanup();
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
                        case VK_UP: keyBit = 0x100; 
                            playSoundEffect(SND_BEEP, 800.0f);
                            break;
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
            RECT windowRect;
            GetClientRect(hwnd, &windowRect);
            int centerX = (windowRect.left + windowRect.right) / 2;
            int centerY = (windowRect.top + windowRect.bottom) / 2;

            POINT centerScreen = {centerX, centerY};
            ClientToScreen(hwnd, &centerScreen);
            SetCursorPos(centerScreen.x, centerScreen.y);

            int deltaX = LOWORD(lParam) - centerX;
            int deltaY = HIWORD(lParam) - centerY;

            Quat qPitch = quat_axis_angle(1, 0, 0, deltaY*0.01); // local X
            Quat qYaw   = quat_axis_angle(0, 1, 0, deltaX*0.01);   // local Y

            rot = quat_mul(qYaw, rot);   // Yaw first
            rot = quat_mul(qPitch, rot); // Then pitch
            
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
