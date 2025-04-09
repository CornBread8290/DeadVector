#include <windows.h>
#include <gl/gl.h>
#include <math.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;


LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

HGLRC hRC = NULL; // OpenGL Rendering Context
HDC hDC = NULL;   // Device Context
HWND hWnd = NULL; // Window Handle

float posX = 0.0f;
float posY = 0.0f;
float posZ = 0.0f;
float pitch = 0.0f;
float yaw = 0.0f;
float roll = 0.0f;

uint16_t keyState = 0;

void SetupPixelFormat(HDC hdc) {
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

    int pixelFormat = ChoosePixelFormat(hdc, &pfd);
    /*if (pixelFormat == 0) {
        MessageBox(NULL, "Failed to choose pixel format.", "Error", MB_OK | MB_ICONERROR);
        exit(1);
    }*/

    if (!SetPixelFormat(hdc, pixelFormat, &pfd)) {
        MessageBox(NULL, "Failed to set pixel format.", "Error", MB_OK | MB_ICONERROR);
        exit(1);
    }
}

void ResizeGLScene(GLsizei width, GLsizei height) {
    if (height == 0) height = 1; // Prevent division by zero
    glViewport(0, 0, width, height);

    float fovY = 45.0f;
    float aspectRatio = (float)width / (float)height;
    float zNear = 1.0f;
    float zFar = 100.0f;

    float f = 1.0f / tanf((fovY * 3.14159265358979323846f / 180.0f) / 2.0f); // Convert degrees to radians

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

void RenderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();

    glRotatef(pitch*57.2957795131 , 1.0f, 0.0f, 0.0f); // pitch: up/down on X
    glRotatef(yaw*57.2957795131 ,   0.0f, 1.0f, 0.0f); // yaw: turn left/right on Y
    glRotatef(roll*57.2957795131 ,  0.0f, 0.0f, 1.0f); // roll: spin around Z

    glTranslatef(-posX, -posY, -posZ);

    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 1.0f, -5.0f);
    glVertex3f(-1.0f, -1.0f, -5.0f);
    glVertex3f(1.0f, -1.0f, -5.0f);
    glEnd();

    SwapBuffers(hDC);
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

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    
    const char *className = "OGL";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Failed to register window class.", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    hWnd = CreateWindowEx(0, className, "Dead Vector", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
    if (!hWnd) {
        MessageBox(NULL, "Failed to create window.", "Error", MB_OK | MB_ICONERROR);
        return 1;
    }

//INITGL     
    hDC = GetDC(hwnd);
    SetupPixelFormat(hDC);

    hRC = wglCreateContext(hDC);
    if (!hRC) {
        MessageBox(NULL, "Failed to create OpenGL rendering context.", "Error", MB_OK | MB_ICONERROR);
        exit(1);
    }

    if (!wglMakeCurrent(hDC, hRC)) {
        MessageBox(NULL, "Failed to make OpenGL rendering context current.", "Error", MB_OK | MB_ICONERROR);
        exit(1);
    }

    glShadeModel(GL_SMOOTH);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);




    
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (keyState & 0x01) {  // W key pressed
            posX += 0.1f * sinf(yaw);  // Move forward
            posZ -= 0.1f * cosf(yaw);  // Move forward
        }
        
        if (keyState & 0x02) {  // A key pressed
            posX -= 0.1f * cosf(yaw);  // Move left
            posZ -= 0.1f * sinf(yaw);  // Move left
        }
        
        if (keyState & 0x04) {  // S key pressed
            posX -= 0.1f * sinf(yaw);  // Move backward
            posZ += 0.1f * cosf(yaw);  // Move backward
        }
        
        if (keyState & 0x08) {  // D key pressed
            posX += 0.1f * cosf(yaw);  // Move right
            posZ += 0.1f * sinf(yaw);  // Move right
        }
        
        RenderScene();
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
                        case 'Q': keyBit = 0x16; break;
                        case 'E': keyBit = 0x32; break;
                        case VK_LSHIFT: keyBit = 0x64; break;
                        case VK_LCONTROL: keyBit = 0x128; break;

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
                        } else {
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
        yaw += deltaX * 0.001f;
        pitch += deltaY * 0.001f;
        break;
        
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}
