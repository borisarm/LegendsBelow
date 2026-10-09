//
// Main.cpp
//

#include "pch.h"

import LB.Engine;

using LB::Engine::GameEngine;

#ifdef __clang__
#pragma clang diagnostic ignored "-Wcovered-switch-default"
#pragma clang diagnostic ignored "-Wswitch-enum"
#endif

#pragma warning(disable : 4061)

extern "C"
{
    // Used to enable the "Agility SDK" components
    __declspec(dllexport) extern const UINT D3D12SDKVersion = D3D12_SDK_VERSION;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\D3D12\\";
}

namespace
{
    std::unique_ptr<GameEngine> g_engine;
}

LPCWSTR g_szAppName = L"LegendsBelow";

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

namespace
{
    // Translate a WM_*BUTTONDOWN/UP message into an engine mouse event
    void ForwardMouseButton(GameEngine* engine, UINT message, LPARAM lParam, bool down)
    {
        if (!engine)
            return;

        using LB::Systems::Input::MouseButton;
        MouseButton button = MouseButton::Left;
        if (message == WM_RBUTTONDOWN || message == WM_RBUTTONUP)
            button = MouseButton::Right;
        else if (message == WM_MBUTTONDOWN || message == WM_MBUTTONUP)
            button = MouseButton::Middle;

        engine->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        engine->OnMouseButton(button, down);
    }
}

// Entry point
int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine, _In_ int nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!DirectX::XMVerifyCPUSupport())
        return 1;

    // Initialize COM for WIC usage
    if (FAILED(CoInitializeEx(nullptr, COINITBASE_MULTITHREADED)))
        return 1;

    // Initialize the GameRuntime
    HRESULT hr = XGameRuntimeInitialize();
    if (FAILED(hr))
    {
        if (hr == E_GAMERUNTIME_DLL_NOT_FOUND || hr == E_GAMERUNTIME_VERSION_MISMATCH || hr == HRESULT_FROM_WIN32(ERROR_SERVICE_DOES_NOT_EXIST))
        {
            std::ignore = MessageBoxW(nullptr, L"Game Runtime is not installed on this system or needs updating.", g_szAppName, MB_ICONERROR | MB_OK);
        }
        return 1;

    }

    g_engine = std::make_unique<GameEngine>();

    // Register class and create window
    {
        // Register class
        WNDCLASSEXW wcex = {};
        wcex.cbSize = sizeof(WNDCLASSEXW);
        wcex.style = CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc = WndProc;
        wcex.hInstance = hInstance;
        wcex.hIcon = LoadIconW(hInstance, L"IDI_ICON");
        wcex.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        wcex.lpszClassName = L"LegendsBelowWindowClass";
        wcex.hIconSm = LoadIconW(wcex.hInstance, L"IDI_ICON");
        if (!RegisterClassExW(&wcex))
            return 1;

        // Create window
        int w, h;
        g_engine->GetDefaultWindowSize(w, h);

        RECT rc = { 0, 0, static_cast<LONG>(w), static_cast<LONG>(h) };

        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        HWND hwnd = CreateWindowExW(0, L"LegendsBelowWindowClass", g_szAppName, WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
            nullptr, nullptr, hInstance,
            g_engine.get());
        // TODO: Change to CreateWindowExW(WS_EX_TOPMOST, L"LegendsBelowWindowClass", g_szAppName, WS_POPUP,
        // to default to fullscreen.

        if (!hwnd)
            return 1;

        ShowWindow(hwnd, nCmdShow);
        // TODO: Change nCmdShow to SW_SHOWMAXIMIZED to default to fullscreen.

        GetClientRect(hwnd, &rc);

        if (!g_engine->Initialize(hwnd, rc.right - rc.left, rc.bottom - rc.top))
        {
            std::ignore = MessageBoxW(hwnd, L"Failed to initialize Direct3D 12. A GPU supporting Shader Model 6.0 is required.", g_szAppName, MB_ICONERROR | MB_OK);
            // Destroy the window first: it holds a pointer to the engine
            DestroyWindow(hwnd);
            g_engine.reset();
            XGameRuntimeUninitialize();
            return 1;
        }
    }

    // Main message loop
    MSG msg = {};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_engine->Tick();
        }
    }


    g_engine.reset();

    XGameRuntimeUninitialize();

    return static_cast<int>(msg.wParam);
}

// Windows procedure
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static bool s_in_sizemove = false;
    static bool s_in_suspend = false;
    static bool s_minimized = false;
    static bool s_fullscreen = false;
    // TODO: Set s_fullscreen to true if defaulting to fullscreen.

    auto engine = reinterpret_cast<GameEngine*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));

    switch (message)
    {
    case WM_CREATE:
        if (lParam)
        {
            auto params = reinterpret_cast<LPCREATESTRUCTW>(lParam);
            SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(params->lpCreateParams));
        }
        break;

    case WM_PAINT:
        if (s_in_sizemove && engine)
        {
            engine->Tick();
        }
        else
        {
            PAINTSTRUCT ps;
            std::ignore = BeginPaint(hWnd, &ps);
            EndPaint(hWnd, &ps);
        }
        break;

    case WM_DISPLAYCHANGE:
        if (engine)
        {
            engine->OnDisplayChange();
        }
        break;

    case WM_MOVE:
        if (engine)
        {
            engine->OnWindowMoved();
        }
        break;

    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
        {
            if (!s_minimized)
            {
                s_minimized = true;
                if (!s_in_suspend && engine)
                    engine->OnSuspending();
                s_in_suspend = true;
            }
        }
        else if (s_minimized)
        {
            s_minimized = false;
            if (s_in_suspend && engine)
                engine->OnResuming();
            s_in_suspend = false;
        }
        else if (!s_in_sizemove && engine)
        {
            engine->OnWindowSizeChanged(LOWORD(lParam), HIWORD(lParam));
        }
        break;

    case WM_ENTERSIZEMOVE:
        s_in_sizemove = true;
        break;

    case WM_EXITSIZEMOVE:
        s_in_sizemove = false;
        if (engine)
        {
            RECT rc;
            GetClientRect(hWnd, &rc);

            engine->OnWindowSizeChanged(rc.right - rc.left, rc.bottom - rc.top);
        }
        break;

    case WM_GETMINMAXINFO:
        if (lParam)
        {
            auto info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 320;
            info->ptMinTrackSize.y = 200;
        }
        break;

    case WM_ACTIVATEAPP:
        if (engine)
        {
            if (wParam)
            {
                engine->OnActivated();
            }
            else
            {
                engine->OnDeactivated();
            }
        }
        break;

    case WM_POWERBROADCAST:
        switch (wParam)
        {
        case PBT_APMQUERYSUSPEND:
            if (!s_in_suspend && engine)
                engine->OnSuspending();
            s_in_suspend = true;
            return TRUE;

        case PBT_APMRESUMESUSPEND:
            if (!s_minimized)
            {
                if (s_in_suspend && engine)
                    engine->OnResuming();
                s_in_suspend = false;
            }
            return TRUE;

        default:
            break;
        }
        break;

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    case WM_SYSKEYDOWN:
        if (wParam == VK_RETURN && (lParam & 0x60000000) == 0x20000000)
        {
            // Implements the classic ALT+ENTER fullscreen toggle
            if (s_fullscreen)
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, 0);

                int width = 800;
                int height = 600;
                if (engine)
                    engine->GetDefaultWindowSize(width, height);

                ShowWindow(hWnd, SW_SHOWNORMAL);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
            else
            {
                SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP);
                SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST);

                SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                ShowWindow(hWnd, SW_SHOWMAXIMIZED);
            }

            s_fullscreen = !s_fullscreen;
        }
        else if (engine)
        {
            // Alt combinations and F10; DefWindowProc still handles Alt+F4
            engine->OnKeyDown(static_cast<int32_t>(wParam));
        }
        break;

    // Input: forwarded to the engine. Key codes are Windows virtual-key codes, which
    // LB::Systems::Input::Keys mirrors.
    case WM_KEYDOWN:
        if (engine)
            engine->OnKeyDown(static_cast<int32_t>(wParam));
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (engine)
            engine->OnKeyUp(static_cast<int32_t>(wParam));
        break;

    case WM_MOUSEMOVE:
        if (engine)
            engine->OnMouseMove(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;

    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
        // Capture so the matching button-up arrives even if released outside the window
        SetCapture(hWnd);
        ForwardMouseButton(engine, message, lParam, true);
        break;

    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
        ForwardMouseButton(engine, message, lParam, false);
        if ((wParam & (MK_LBUTTON | MK_RBUTTON | MK_MBUTTON)) == 0)
            ReleaseCapture();
        break;

    case WM_MOUSEWHEEL:
        if (engine)
            engine->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wParam));
        break;

    case WM_MENUCHAR:
        // A menu is active and the user presses a key that does not correspond
        // to any mnemonic or accelerator key. Ignore so we don't produce an error beep.
        return MAKELRESULT(0, MNC_CLOSE);

    default:
        break;
    }

    return DefWindowProc(hWnd, message, wParam, lParam);
}
