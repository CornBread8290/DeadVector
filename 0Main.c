#include <windows.h>
#include <gl/gl.h>
#include "defs.h"

HGLRC hRC; // OpenGL Rendering Context
HDC hDC;   // Device Context
HWND hWnd; // Window Handle

float aspectRatio;

uint16_t keyState = 0;

Quat rot = {0.0f, 0.0f, 0.0f, 1.0f}; //Pitch, Yaw, Roll, W
Vec3 pos = {0.0f, 0.0f, 0.0f};

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void stopSound();
void playSoundEffect(int type, float pitch, float volume, float pan);


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
    float f = 1.0f / tanf((fovY * pi / 180.0f) / 2.0f); // Convert degrees to radians
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
void drawPlanet(float radius, int lats, int longs) {
    for (int i = 0; i <= lats; ++i) {
        float lat0 = pi * (-0.5 + (float)(i - 1) / lats);
        float z0  = sin(lat0);
        float zr0 =  cos(lat0);

        float lat1 = pi * (-0.5 + (float)i / lats);
        float z1 = sin(lat1);
        float zr1 = cos(lat1);

        glBegin(GL_QUAD_STRIP);
        for (int j = 0; j <= longs; ++j) {
            float lng = 2 * pi * (float)(j - 1) / longs;
            float x = cos(lng);
            float y = sin(lng);

            glColor3f(1, (fabs(z0)), (fabs(z0))); 
            glVertex3f(radius * x * zr0, radius * y * zr0, radius * z0);

            glColor3f(1, (fabs(z1)), (fabs(z1)));
            glVertex3f(radius * x * zr1, radius * y * zr1, radius * z1);
        }
        glEnd();
    }
}
void drawRings(float innerRadius, float outerRadius, int segments, int bands) {
    float bandStep = (outerRadius - innerRadius) / bands;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (int b = 0; b < bands; ++b) {
        float r0 = innerRadius + b * bandStep;
        float r1 = r0 + bandStep;

        float base = 0.6f + 0.3f * sin(b * 0.5f); // Makes it undulate smoothly
        float red   = base * 0.3f;                // Just a kiss of warmth
        float green = base * 0.6f;                // Minty glacier vibes
        float blue  = base * 1.0f;                // Ice daddy blue 💙
        float alpha = 0.3f + 0.5f * fabs(cos(b * 0.3f)); // Transparent like my intentions
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= segments; ++i) {
            float angle = 2 * pi * i / segments;
            float x = cos(angle);
            float y = sin(angle);

            glColor4f(red, green, blue, alpha); glVertex2f(x * r0, y * r0);
            glColor4f(red, green, blue, alpha); glVertex2f(x * r1, y * r1);
        }
        glEnd();
    }

    glDisable(GL_BLEND);
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


    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    Vec3 vel = {0.0f, 0.0f, 0.0f}; 
    Quat spn = {0.0f, 0.0f, 0.0f, 1.0f};
    float thrust = 0.003f;

    MSG msg;
    //MAIN LOOP
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Vec3 dir = {0};
        float vol = 0.002f;
        if (keyState & 0x01) { dir.z -= 1; } // W
        if (keyState & 0x04) { dir.z += 1; } // S
        if (keyState & 0x02) { dir.x -= 1; } // A
        if (keyState & 0x08) { dir.x += 1; } // D
        if (keyState & 0x40) { dir.y += 1; }// Shift
        if (keyState & 0x80) { dir.y -= 1; }// Ctrl   
        if (keyState & 0x100) {  thrust = 1000.0f; } // Up
        if (keyState & 0x200) { thrust = 0.003f; } // Down
        if (keyState & 0x400) {   } // Left
        if (keyState & 0x800) {   } // Right
        if (keyState & 0x10) {  // Q
            Quat dq = quat_axis_angle(0, 0, 1, -0.02);
            rot = quat_mul(dq, rot);
        }
        if (keyState & 0x20) {  // E
          Quat dq = quat_axis_angle(0, 0, 1, 0.02);
         rot = quat_mul(dq, rot);
        }
        
        if ((keyState & 0x828) && (keyState & 0x412) == 0){ playSoundEffect(SND_ENGINE, 200.0f,vol, -1); }
        else if ((keyState & 0x412) ) {playSoundEffect(SND_ENGINE, 200.0f,vol, 1);}
        else if (keyState == 0 ){ stopSound();}
        else { playSoundEffect(SND_ENGINE, 200.0f,vol, 0); }

        Quat invRot = quat_conjugate(rot);
        Vec3 worldDir = quat_rotate_vec3(invRot, dir);

        vel.x += worldDir.x * thrust;
        vel.y += worldDir.y * thrust;
        vel.z += worldDir.z * thrust;

        pos.x += vel.x;
        pos.y += vel.y;
        pos.z += vel.z;
        
        glLoadIdentity();

        rot = quat_normalize(rot);
        
        float mat[16];
        quat_to_matrix(&rot, mat);

        //Rotation
        glMultMatrixf(mat);
        
        //Stars
        glDisable(GL_DEPTH_TEST);
        glPointSize(2.0f); 
        glBegin(GL_POINTS);
        glColor3f(1.0f, 1.0f, 1.0f);
        for (int i = 0; i < NUM_STARS; ++i) {
            glVertex3fv(star_positions[i]);
        }
        glEnd();

        //translation
        glEnable(GL_DEPTH_TEST); 
        glTranslatef(-pos.x, -pos.y, -pos.z);

        //far
        SetProjectionMatrix(10000.0f, 4000000000.0f);
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -5000000.0f); 
        drawPlanet(1000000.0f, 20, 20);
        drawRings(1000000.0f, 2000000.0f, 20, 20);
        glPopMatrix();

        glClear(GL_DEPTH_BUFFER_BIT);

        //near
        SetProjectionMatrix(0.01f, 4000.0f);
        glPushMatrix();
        glTranslatef(0.0f, 0.0f, -5.0f);
        glBegin(GL_TRIANGLES);
        glColor3f(0.0f, 1.0f, 0.0f); 
        glVertex3f(0.0f, 1.0f, 0.0f);
        glVertex3f(-1.0f, -1.0f, 0.0f);
        glVertex3f(1.0f, -1.0f, 0.0f);
        glEnd();

        glClear(GL_DEPTH_BUFFER_BIT);

        SetProjectionMatrix(0.01f, 2.0f);

        // Reset transformations
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f); // Set orthographic projection for HUD
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity(); 
        
        // HUD 
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        

        glLineWidth(2.5f);
        glColor4f(0.2f, 1.0f, 0.9f, 0.8f);
        
        glBegin(GL_LINES);
        glVertex3f(-0.05f, 0.0f, -0.2f);
        glVertex3f( 0.05f, 0.0f, -0.2f);
        
        glVertex3f(0.0f, -0.05f, -0.2f);
        glVertex3f(0.0f,  0.05f, -0.2f);
        
        glColor4f(0.2f, 0.8f, 1.0f, 0.3f); 
        for (float i = -0.5f; i <= 0.5f; i += 0.05f) {
            glVertex3f(i, -0.5f, -0.2f);
            glVertex3f(i,  0.5f, -0.2f);
        
            glVertex3f(-0.5f, i, -0.2f);
            glVertex3f( 0.5f, i, -0.2f);
        }
        glEnd();
        
        glDisable(GL_BLEND);
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
            GLsizei width = LOWORD(lParam);
            GLsizei height = HIWORD(lParam);
            if (height == 0) height = 1;  // Prevent division by zero
            glViewport(0, 0, width, height);
            aspectRatio = (float)width / (float)height;
            SetProjectionMatrix(1.0f, 4000000000.0f);
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

            rot = quat_mul(quat_axis_angle(0, 1, 0, (LOWORD(lParam) - centerX)*0.01), rot);//qYaw
            rot = quat_mul(quat_axis_angle(1, 0, 0, (HIWORD(lParam) - centerY)*0.01), rot);//qPitch
            
            break;
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
