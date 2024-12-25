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
// #include <d3d12.h>
#include <dxgi1_6.h>
// #include <d3dcompiler.h>

#include "directx/d3d12.h"
#include "directx/d3dx12.h"
#include <DirectXMath.h>

#include <string>
#include <cstdio>
#include <stdexcept>
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
#define _DEBUG
static bool m_useWarpDevice = false; // WARP stands for Windows Advanced Rasterization Platform - software renderer, basically

// DirectX3D objects
static UINT m_width = 1280;
static UINT m_height = 720;
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

// Helper functions for C-->C++ interoperability by Microsoft
inline void ThrowIfFailed(HRESULT hr) {
  if (FAILED(hr)) {
    throw std::runtime_error("Failed HRESULT");
  }
}

// Helper function - getting hardware adapter
void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter) {
  *ppAdapter = nullptr;
  for (UINT adapterIndex = 0; ; ++adapterIndex) {
      IDXGIAdapter1* pAdapter = nullptr;
      if (DXGI_ERROR_NOT_FOUND == pFactory->EnumAdapters1(adapterIndex, &pAdapter)) {
          // No more adapters to enumerate.
          break;
      } 

      // Check to see if the adapter supports Direct3D 12, but don't create the
      // actual device yet.
      if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
          *ppAdapter = pAdapter;
          return;
      }
      pAdapter->Release();
  }
}

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

  // Initialization - pipeline
  // Enabling debug layer - debug messages into parent process console
#if defined(_DEBUG)
    AttachConsole(ATTACH_PARENT_PROCESS); // TODO: check if attaching to console fails
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "w", stdin);

    printf("Debug enabled!\n"); // TODO: create more elaborate debug message with __FILE__ and __LINE__ if it's possible (maybe macro?)
#endif

  // Enabling the D3D12 debug layer
#if defined(_DEBUG)
    {
      Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
      if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
      {
        debugController->EnableDebugLayer();
      }
    }
#endif

  // Creating factory
  Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
  ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

  // Creating device
  if (m_useWarpDevice) {
    // TODO: put code for software renderer here - right now we work on our GPU hardware, so no code here
  } else {
    Microsoft::WRL::ComPtr<IDXGIAdapter1> hardwareAdapter;
    GetHardwareAdapter(factory.Get(), &hardwareAdapter);

    ThrowIfFailed(D3D12CreateDevice(
      hardwareAdapter.Get(),
      D3D_FEATURE_LEVEL_11_0,
      IID_PPV_ARGS(&m_device)));
  }
  // Initialization - describing and creating the command queue
  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;
  queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;

  ThrowIfFailed(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

  // Initialization - describing and creating the swap chain
  DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
  swapChainDesc.BufferCount          = FRAME_COUNT;
  swapChainDesc.BufferDesc.Width     = m_width;
  swapChainDesc.BufferDesc.Height    = m_height;
  swapChainDesc.BufferDesc.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
  swapChainDesc.BufferUsage          = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swapChainDesc.SwapEffect           = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swapChainDesc.OutputWindow         = hwnd;
  swapChainDesc.SampleDesc.Count     = 1;
  swapChainDesc.Windowed             = TRUE;

  Microsoft::WRL::ComPtr<IDXGISwapChain> swapChain;
  ThrowIfFailed(factory->CreateSwapChain(
        m_commandQueue.Get(),
        &swapChainDesc,
        &swapChain
    ));

  ThrowIfFailed(swapChain.As(&m_swapChain));

  // Initialization - setting up fullscreen
  ThrowIfFailed(factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));
  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();

  // Initialization - create descriptor heaps
  {
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors             = FRAME_COUNT;
    rtvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(m_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));

    m_rtvDescriptorSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  }

  // Initialization - frame resources
  {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());   
  }

  // Loading assets - Hello World Triangle for now

  // Main loop
  MSG msg = {};

  while (msg.message != WM_QUIT) {
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  // All went well message for those pesky segfaults without notification on WinOS
  printf("All went well - goodbye...\n"); // TODO: This message interfere with normal console flow - fix that
  return (int) msg.wParam;
}


