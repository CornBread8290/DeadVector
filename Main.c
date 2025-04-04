#include <windows.h>
#include <gl/gl.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
void InitOpenGL(HWND hwnd);
void SetupPixelFormat(HDC hdc);
void RenderScene();

HGLRC hRC = NULL; // OpenGL Rendering Context
HDC hDC = NULL;   // Device Context
HWND hWnd = NULL;  // Window Handle

// Main entry point
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Register the window class
    const char *className = "OpenGLWindowClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    RegisterClass(&wc);

    // Create the window
    hWnd = CreateWindowEx(0, className, "My OpenGL Engine", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);
    if (!hWnd) {
        MessageBox(NULL, "Window creation failed!", "Error", MB_ICONERROR);
        return 0;
    }

    // Set up OpenGL
    InitOpenGL(hWnd);

    // Show the window
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int) msg.wParam;
}

// Window procedure to handle messages
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        case WM_SIZE:
        case WM_SYSCOMMAND:
            if (wParam == SC_MINIMIZE) {
                // Handle window minimizing if needed
            }
            break;
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_DESTROY:
            // Clean up OpenGL
            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(hRC);
            ReleaseDC(hwnd, hDC);
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// Initialize OpenGL settings
void InitOpenGL(HWND hwnd)
{
    hDC = GetDC(hwnd);
    SetupPixelFormat(hDC);

    hRC = wglCreateContext(hDC); // Create OpenGL Rendering Context
    wglMakeCurrent(hDC, hRC);    // Make it current

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnable(GL_DEPTH_TEST); // Enable depth testing for 3D
}

// Set up pixel format for OpenGL rendering
void SetupPixelFormat(HDC hdc)
{
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
    SetPixelFormat(hdc, pixelFormat, &pfd);
}

// Basic rendering function
void RenderScene()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen
    glLoadIdentity();  // Reset any previous transformations

    SwapBuffers(hDC); // Swap buffers for double buffering
}
