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
#include "directx/d3d12.h"
#include "directx/d3dx12.h"
#include "directx/d3dcommon.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#include <DirectXMath.h>

#include <string>
#include <cstdio>
#include <stdexcept>
#include <wrl.h>

// Engine stuff
std::wstring m_assetsPath = L"assets/";

// Non defined keys definitions
#define VK_Q 0x51

// DirectX3D types
struct Vertex {
  DirectX::XMFLOAT3 position;
  DirectX::XMFLOAT4 color;
};

// DirectX3D variables
#define FRAME_COUNT 2
#define _DEBUG
static bool m_useWarpDevice = false; // WARP stands for Windows Advanced Rasterization Platform - software renderer, basically

// DirectX3D objects
static UINT m_width = 1280;
static UINT m_height = 720;
static float m_aspectRatio = static_cast<float>(m_width) / static_cast<float>(m_height);
static std::wstring name = L"Box Delivery - engine.";
static bool main_loop = true;

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

// Shaders code
const char* shadersCode = R"(
  struct PSInput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
  };

  PSInput VSMain(float4 position : POSITION, float4 color : COLOR) {
    PSInput result;

    result.position = position;
    result.color = color;

    return result;
  }

  float4 PSMain(PSInput input) : SV_TARGET {
    return input.color;
  }
)";

// Helper function - copying error messages (from shaders for example) to clipboard
void CopyToClipboard(HWND hWnd, const std::string& text) {
  if (!OpenClipboard(hWnd)) return;

  EmptyClipboard();

  // Allocate global memory for the text
  size_t sizeInBytes = (text.size() + 1) * sizeof(char);
  HGLOBAL hGlobal = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
  if (hGlobal) {
    void* pGlobal = GlobalLock(hGlobal);
    memcpy(pGlobal, text.c_str(), sizeInBytes);
    GlobalUnlock(hGlobal);

    // Transferring data to clipboard
    SetClipboardData(CF_TEXT, hGlobal);
  }

  CloseClipboard();
}

// Helper functions for C-->C++ interoperability by Microsoft
inline void ThrowIfFailed(HRESULT hr) {
  if (FAILED(hr)) {
    throw std::runtime_error("Failed HRESULT");
  }
}

// Helper function - assets // TODO: create a assets manager
std::wstring GetAssetFullPath(LPCWSTR assetName) {
  return m_assetsPath + assetName;
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

// Helper function - waiting for previous frame
void WaitForPreviousFrame() {
  // Waiting for the frame to complete before continuing is not the best practice (what if it never finishes?)

  // Signal and icrement the fence value
  const UINT64 fence = m_fenceValue;
  ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), fence));
  m_fenceValue++;

  // Wait until the previous frame if finished
  if (m_fence->GetCompletedValue() < fence) {
    ThrowIfFailed(m_fence->SetEventOnCompletion(fence, m_fenceEvent));
    WaitForSingleObject(m_fenceEvent, INFINITE);
  }

  m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
}

// Helper function - PopulateCommandList
void PopulateCommandList() {
  ThrowIfFailed(m_commandAllocator->Reset());

  ThrowIfFailed(m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

  m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
  m_commandList->RSSetViewports(1, &m_viewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);

  auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
      D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  m_commandList->ResourceBarrier(1, &barrier);

  CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(), m_frameIndex, m_rtvDescriptorSize);
  m_commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

  // Rendering goes here - commands for DirectX.
  const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
  m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  m_commandList->IASetVertexBuffers(0, 1, &m_vertexBufferView);
  m_commandList->DrawInstanced(3, 1, 0, 0);

  // Moving backbuffer to the front.
  barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndex].Get(),
      D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  m_commandList->ResourceBarrier(1, &barrier);

  ThrowIfFailed(m_commandList->Close());
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
          // PostQuitMessage(0);
          main_loop = false;
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
#if 0 // This does not work in debugger for example, so we won't have fun with it
    AttachConsole(ATTACH_PARENT_PROCESS); // TODO: check if attaching to console fails
    FILE* fp;
    freopen_s(&fp, "CONOUT$", "w", stdout);
    freopen_s(&fp, "CONOUT$", "w", stderr);
    freopen_s(&fp, "CONIN$", "w", stdin);

    printf("Debug enabled!\n"); // TODO: create more elaborate debug message with __FILE__ and __LINE__ if it's possible (maybe macro?)
#endif
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

    // RTV for each frame
    for (UINT n = 0; n < FRAME_COUNT; n++) {
      ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
      m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
      rtvHandle.Offset(1, m_rtvDescriptorSize);
    }
  }

  ThrowIfFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)));

  // Loading assets - Hello World Triangle for now

  // Creating empy root signature
  {
    CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init(0, nullptr, 0, nullptr,
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    Microsoft::WRL::ComPtr<ID3DBlob> signature;
    Microsoft::WRL::ComPtr<ID3DBlob> error;
    ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
    ThrowIfFailed(m_device->CreateRootSignature(0,
          signature->GetBufferPointer(),
          signature->GetBufferSize(),
          IID_PPV_ARGS(&m_rootSignature)));
  }

  // Create the pipline state - includes compiling and loading shaders
  {
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShader;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShader;
    Microsoft::WRL::ComPtr<ID3DBlob> vertexShaderErrorBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> pixelShaderErrorBlob;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    UINT compileFlags = 0;
#endif

    try {
    ThrowIfFailed(D3DCompile(shadersCode, strlen(shadersCode),
          nullptr, nullptr, nullptr,
          "VSMain", "vs_5_0", compileFlags, 0,
          &vertexShader, &vertexShaderErrorBlob));
    } catch (const std::runtime_error& e) {
      MessageBoxA(
        nullptr,
        e.what(),
        "Exception caught!",
        MB_OK | MB_ICONERROR
      );
      // Getting information from pixelShaderErrorBlob
      const char* errorMessage = static_cast<const char*>(vertexShaderErrorBlob->GetBufferPointer());
      MessageBoxA(nullptr, errorMessage, "Error blob message:", MB_OK);
      CopyToClipboard(hwnd, errorMessage);
    }


    try {
    ThrowIfFailed(D3DCompile(shadersCode, strlen(shadersCode),
          nullptr, nullptr, nullptr,
          "PSMain", "ps_5_0", compileFlags, 0, // @Hint: ps_5_0 selects what kind of shader is being compiled
          &pixelShader, &pixelShaderErrorBlob));
    // TODO: add compilation errors check for pixel shader
    } catch (const std::runtime_error& e) {
      MessageBoxA(
        nullptr,
        e.what(),
        "Exception caught!",
        MB_OK | MB_ICONERROR
      );
      // Getting information from pixelShaderErrorBlob
      const char* errorMessage = static_cast<const char*>(pixelShaderErrorBlob->GetBufferPointer());
      MessageBoxA(nullptr, errorMessage, "Error blob message:", MB_OK);
      CopyToClipboard(hwnd, errorMessage);
    }

    // Define the vertex input layout
    D3D12_INPUT_ELEMENT_DESC inputElementDesc[] = {
      {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };

    // Describe and create the graphics pipeline state object (PSO).
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = {inputElementDesc, _countof(inputElementDesc)};
    psoDesc.pRootSignature = m_rootSignature.Get();
    psoDesc.VS = {reinterpret_cast<UINT8*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize()};
    psoDesc.PS = {reinterpret_cast<UINT8*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize()};
    psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pipelineState)));
  }

  // Create the command list
  ThrowIfFailed(m_device->CreateCommandList(0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(),
        m_pipelineState.Get(),
        IID_PPV_ARGS(&m_commandList)));

  // Command lists are created in the recording state, but there is nothing to record yet. The main loop expects it to be close, so close it now
  ThrowIfFailed(m_commandList->Close());

  // Create the vertex buffer
  {
    // Define the geometry for a triangle
    Vertex triangleVertices[] = {
        {{0.0f, 0.25f    * m_aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{0.25f, -0.25f  * m_aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };

    const UINT vertexBufferSize = sizeof(triangleVertices);

    // Note: using upload heaps to transfer static data like vert buffers is not recommended (then why is it in tutorial then?)
    // Please read up on Default Heap usage.
    // It is used for code simplicity (xD) and because there are few verts to transfer.
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    auto desc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
    ThrowIfFailed(m_device->CreateCommittedResource(
          &heapProps,
          D3D12_HEAP_FLAG_NONE,
          &desc,
          D3D12_RESOURCE_STATE_GENERIC_READ,
          nullptr,
          IID_PPV_ARGS(&m_vertexBuffer)));

    // Copy the triangle data to the vertex buffer
    UINT8* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0); // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
    m_vertexBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view
    m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
    m_vertexBufferView.StrideInBytes = sizeof(Vertex);
    m_vertexBufferView.SizeInBytes = vertexBufferSize;
  }

  // Create synchronization objects and wait until assets have been uploaded to the GPU.
  {
    ThrowIfFailed(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
    m_fenceValue = 1;

    // Create an event handle to use for frame synchronization
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (m_fenceEvent == nullptr) {
      ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
    }

    // Wait for the command list to execute
    // We are reusing the same command list in our main loop but for now
    // we just wait for setup to complete before continuing
    WaitForPreviousFrame();
  }

  // Main loop
  MSG msg = {};

  while (main_loop) {
    // Windows messages thing
    if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    // Rendering goes here
    PopulateCommandList();

    ID3D12CommandList* ppCommandLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

    // Present the frame.
    ThrowIfFailed(m_swapChain->Present(1, 0));

    WaitForPreviousFrame();
  }

  // On destroy - after main loop
  WaitForPreviousFrame();

  CloseHandle(m_fenceEvent);

  // All went well message for those pesky segfaults without notification on WinOS
#if 0
  printf("All went well - goodbye...\n"); // TODO: This message interfere with normal console flow - fix that
#endif

  return (int) msg.wParam;
}


