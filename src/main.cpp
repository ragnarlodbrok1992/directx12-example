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
#include <DirectXMath.h>

#include <string>
#include <wrl.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

// Non defined keys definitions
#define VK_Q 0x51

// DirectX3D types
struct Vertex {
  DirectX::XMFLOAT3 positon;
  DirectX::XMFLOAT4 color;
};

// DirectX3D variables
#define FRAME_COUNT 2

// DirectX3D objects
static UINT width = 1280;
static UINT height = 720;
static std::wstring name = L"Box Delivery - engine.";

// Pipeline objects
D3D12_VIEWPORT m_viewport;
D3D12_RECT     m_scissorRect;
Microsoft::WRL::ComPtr<IDXGISwapChain3> m_swapChain;
Microsoft::WRL::ComPtr<ID3D12Device> m_device;
Microsoft::WRL::ComPtr<ID3D12Resource> m_renderTargets[FRAME_COUNT];
Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_commandAllocator;
Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_commandQueue;
Microsoft::WRL::ComPtr<ID3D12RootSignature> m_rootSignature;
Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
Microsoft::WRL::ComPtr<ID3D12PipelineState> m_pipelineState;
Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
UINT m_rtvDescriptorSize;

// Application resources
Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

// Synchronization objects
UINT m_frameIndex;
HANDLE m_fenceEvent;
Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
UINT64 m_fenceValue;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
  // Destroying application
  switch (message) {
    case WM_DESTROY: {
        PostQuitMessage(0);
        return 0;
      }
      break;
    case WM_KEYDOWN: {
        if (wParam == VK_Q) { // VK_Q
          PostQuitMessage(0);
          return 0;
        }
      }
      break;
    default:
      return DefWindowProc(hWnd, message, wParam, lParam);
  }
  // Just to not have warning here
  return DefWindowProc(hWnd, message, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  // Create and register class
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


