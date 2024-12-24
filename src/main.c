// I need to write this myself
// since making and providing a SIMPLE (not Microsoft one) example
// to render HelloWorld triangle in DirectX3D is something beyond
// capabilities of multi bilion dollar company and
// LLMs
// And when LLMs doesn't handle this very well - I get that
// But example provided by MS is so OOP bloated it is painful to
// actually read and work with. Shame.

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define FRAME_COUNT 2

// DirectX3D objects


LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hWnd, message, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  WNDCLASSEX wc = {
    sizeof(WNDCLASSEX),
    CS_HREDRAW|CS_VREDRAW,
    WndProc,
    0,
    0,
    hInstance,
    0,
    0,
    0,
    0,
    "DX12WindowClass",
    0
  };

  RegisterClassEx(&wc);

  HWND hwnd = CreateWindow(
      "DX12WindowClass",
      "Hello World Triangle",
      WS_OVERLAPPEDWINDOW,
      100,
      100,
      1280,
      720,
      NULL,
      NULL,
      hInstance,
      NULL
      );
  ShowWindow(hwnd, nCmdShow);

  // Main loop
  MSG msg = {};

  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }
  return (int) msg.wParam;
}


