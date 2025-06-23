// Windows Platform Implementation for Quantum Terminal
#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <stdio.h>
#include <stdlib.h>
#include "../quantum_terminal.h"

#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

// Global variables
HWND g_hWnd = NULL;
HDC g_hDC = NULL;
HGLRC g_hRC = NULL;
BOOL g_active = TRUE;
BOOL g_keys[256];

// Mouse state
int g_mouseX = 0;
int g_mouseY = 0;
int g_mouseDown = 0;

// Terminal state (imported from cocoa_terminal.m logic)
extern void init_quantum_field(void);
extern void update_particles(float dt);
extern void render_scene(void);
extern void handle_key_input(int key, int action);
extern void handle_mouse_input(int x, int y, int button, int action);

// Window procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_CREATE:
            return 0;
            
        case WM_CLOSE:
            PostQuitMessage(0);
            return 0;
            
        case WM_SIZE:
            {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                if (height == 0) height = 1;
                
                glViewport(0, 0, width, height);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                gluPerspective(60.0, (GLfloat)width/(GLfloat)height, 0.1, 1000.0);
                glMatrixMode(GL_MODELVIEW);
            }
            return 0;
            
        case WM_KEYDOWN:
            g_keys[wParam] = TRUE;
            handle_key_input(wParam, 1);
            return 0;
            
        case WM_KEYUP:
            g_keys[wParam] = FALSE;
            handle_key_input(wParam, 0);
            return 0;
            
        case WM_MOUSEMOVE:
            g_mouseX = GET_X_LPARAM(lParam);
            g_mouseY = GET_Y_LPARAM(lParam);
            handle_mouse_input(g_mouseX, g_mouseY, -1, 0);
            return 0;
            
        case WM_LBUTTONDOWN:
            g_mouseDown = 1;
            handle_mouse_input(g_mouseX, g_mouseY, 0, 1);
            return 0;
            
        case WM_LBUTTONUP:
            g_mouseDown = 0;
            handle_mouse_input(g_mouseX, g_mouseY, 0, 0);
            return 0;
            
        case WM_RBUTTONDOWN:
            handle_mouse_input(g_mouseX, g_mouseY, 1, 1);
            return 0;
            
        case WM_MOUSEWHEEL:
            {
                int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                handle_mouse_input(g_mouseX, g_mouseY, 2, delta);
            }
            return 0;
            
        case WM_CHAR:
            // Handle character input
            if (wParam >= 32 && wParam < 127) {
                handle_key_input(wParam, 2); // Special flag for char input
            }
            return 0;
    }
    
    return DefWindowProc(hWnd, message, wParam, lParam);
}

// Initialize OpenGL
BOOL InitGL(void) {
    glShadeModel(GL_SMOOTH);
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glEnable(GL_POINT_SMOOTH);
    glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
    
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    
    return TRUE;
}

// Create OpenGL window
BOOL CreateGLWindow(char* title, int width, int height, int bits) {
    GLuint PixelFormat;
    WNDCLASS wc;
    DWORD dwExStyle;
    DWORD dwStyle;
    RECT WindowRect;
    
    WindowRect.left = 0;
    WindowRect.right = width;
    WindowRect.top = 0;
    WindowRect.bottom = height;
    
    HINSTANCE hInstance = GetModuleHandle(NULL);
    
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = (WNDPROC)WndProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "QuantumTerminal";
    
    if (!RegisterClass(&wc)) {
        MessageBox(NULL, "Failed to register window class", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    dwStyle = WS_OVERLAPPEDWINDOW;
    
    AdjustWindowRectEx(&WindowRect, dwStyle, FALSE, dwExStyle);
    
    if (!(g_hWnd = CreateWindowEx(dwExStyle,
                                  "QuantumTerminal",
                                  title,
                                  dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  WindowRect.right - WindowRect.left,
                                  WindowRect.bottom - WindowRect.top,
                                  NULL,
                                  NULL,
                                  hInstance,
                                  NULL))) {
        MessageBox(NULL, "Window creation failed", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,
        bits,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        16,
        0,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    
    if (!(g_hDC = GetDC(g_hWnd))) {
        MessageBox(NULL, "Can't create device context", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    if (!(PixelFormat = ChoosePixelFormat(g_hDC, &pfd))) {
        MessageBox(NULL, "Can't find suitable pixel format", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    if (!SetPixelFormat(g_hDC, PixelFormat, &pfd)) {
        MessageBox(NULL, "Can't set pixel format", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    if (!(g_hRC = wglCreateContext(g_hDC))) {
        MessageBox(NULL, "Can't create GL context", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    if (!wglMakeCurrent(g_hDC, g_hRC)) {
        MessageBox(NULL, "Can't activate GL context", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    ShowWindow(g_hWnd, SW_SHOW);
    SetForegroundWindow(g_hWnd);
    SetFocus(g_hWnd);
    
    if (!InitGL()) {
        MessageBox(NULL, "GL initialization failed", "Error", MB_OK|MB_ICONERROR);
        return FALSE;
    }
    
    return TRUE;
}

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    MSG msg;
    BOOL done = FALSE;
    LARGE_INTEGER frequency, lastTime, currentTime;
    
    // Create window
    if (!CreateGLWindow("Quantum Terminal", 800, 600, 32)) {
        return 0;
    }
    
    // Initialize quantum field
    init_quantum_field();
    
    // Get timer frequency
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&lastTime);
    
    // Main loop
    while (!done) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                done = TRUE;
            } else {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        } else {
            // Calculate delta time
            QueryPerformanceCounter(&currentTime);
            float dt = (float)(currentTime.QuadPart - lastTime.QuadPart) / frequency.QuadPart;
            lastTime = currentTime;
            
            // Update and render
            update_particles(dt);
            render_scene();
            SwapBuffers(g_hDC);
        }
    }
    
    // Cleanup
    if (g_hRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(g_hRC);
    }
    
    if (g_hDC) {
        ReleaseDC(g_hWnd, g_hDC);
    }
    
    if (g_hWnd) {
        DestroyWindow(g_hWnd);
    }
    
    UnregisterClass("QuantumTerminal", hInstance);
    
    return (int)msg.wParam;
}