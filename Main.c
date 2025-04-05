#include <windows.h>
#include <gl/gl.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


HGLRC hRC = NULL; // OpenGL Rendering Context
HDC hDC = NULL;   // Device Context
HWND hWnd = NULL;  // Window Handle


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

// Initialize OpenGL settings
void InitGL(HWND hwnd)
{
    hDC = GetDC(hwnd);
    SetupPixelFormat(hDC);

    hRC = wglCreateContext(hDC); // Create OpenGL Rendering Context
    wglMakeCurrent(hDC, hRC);    // Make it current


    glShadeModel(GL_SMOOTH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear the screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnable(GL_DEPTH_TEST); // Enable depth testing for 3D
    glDepthFunc(GL_LEQUAL);   
}


// Basic rendering function
void RenderScene(GLvoid)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
    glLoadIdentity(); // Reset the model-view matrix
    glTranslatef(-1.5f,0.0f,-6.0f); 
                    
    glBegin(GL_TRIANGLES);     // Drawing Using Triangles
    glVertex3f( 0.0f, 1.0f, 0.0f); // Top
    glVertex3f(-1.0f,-1.0f, 0.0f); // Bottom Left
    glVertex3f( 1.0f,-1.0f, 0.0f); // Bottom Right
    glEnd(); // Finished Drawing The Triangle

    SwapBuffers(hDC); // Swap buffers for double buffering
}
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // Register the window class
    const char *className = "OpenGLWindowClass";
    WNDCLASS wc = {0};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = className;
    RegisterClass(&wc);

    hWnd = CreateWindowEx(0, className, "OpenGL Render", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, NULL, NULL, hInstance, NULL);

    InitGL(hWnd);

    // Show the window
    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    RenderScene();


    // Main message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }


    // No more virussing
    hWnd = NULL;
    return (int) msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
        //case WM_SIZE:
        /*case WM_SYSCOMMAND:
            if (wParam == SC_MINIMIZE) {
                // Handle window minimizing if needed
            }
            break;*/
        case WM_CLOSE:
            PostQuitMessage(0);
            break;
        case WM_DESTROY:

            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(hRC);
            ReleaseDC(hwnd, hDC);
            hDC = NULL;
            hRC = NULL;
            break;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
