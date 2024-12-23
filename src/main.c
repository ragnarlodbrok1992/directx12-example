#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

#define FRAME_COUNT 2

static IDXGISwapChain3* swapChain;
static ID3D12Device* device;
static ID3D12Resource* renderTargets[FRAME_COUNT];
static ID3D12CommandAllocator* commandAllocators[FRAME_COUNT];
static ID3D12CommandQueue* commandQueue;
static ID3D12DescriptorHeap* rtvHeap;
static ID3D12PipelineState* pipelineState;
static ID3D12GraphicsCommandList* commandList;
static ID3D12Fence* fence;
static HANDLE fenceEvent;
static UINT64 fenceValue[FRAME_COUNT];
static UINT rtvDescriptorSize;
static UINT frameIndex;
static D3D12_VIEWPORT viewport;
static D3D12_RECT scissorRect;
static ID3D12RootSignature* rootSignature;
static ID3D12Resource* vertexBuffer;
static D3D12_VERTEX_BUFFER_VIEW vertexBufferView;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

typedef struct {
    float position[3];
    float color[4];
} Vertex;

void WaitForPreviousFrame() {
    const UINT64 currentFenceValue = fenceValue[frameIndex];
    fenceValue[frameIndex]++;
    commandQueue->lpVtbl->Signal(commandQueue, fence, currentFenceValue);
    if (fence->lpVtbl->GetCompletedValue(fence) < currentFenceValue) {
        fence->lpVtbl->SetEventOnCompletion(fence, currentFenceValue, fenceEvent);
        WaitForSingleObject(fenceEvent, INFINITE);
    }
    frameIndex = swapChain->lpVtbl->GetCurrentBackBufferIndex(swapChain);
}

void LoadPipeline(HWND hwnd) {
    IDXGIFactory4* factory;
    CreateDXGIFactory1(&IID_IDXGIFactory4, (void**)&factory);
    IDXGIAdapter1* adapter = NULL;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->lpVtbl->EnumAdapters1(factory, adapterIndex, &adapter); adapterIndex++) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->lpVtbl->GetDesc1(adapter, &desc);
        if (!(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
            if (D3D12CreateDevice((IUnknown*)adapter, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void**)&device) == S_OK) {
                break;
            }
        }
        adapter->lpVtbl->Release(adapter);
        adapter = NULL;
    }
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    device->lpVtbl->CreateCommandQueue(device, &queueDesc, &IID_ID3D12CommandQueue, (void**)&commandQueue);
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = FRAME_COUNT;
    swapChainDesc.Width = 1280;
    swapChainDesc.Height = 720;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    IDXGISwapChain1* tempSwapChain;
    factory->lpVtbl->CreateSwapChainForHwnd(factory, (IUnknown*)commandQueue, hwnd, &swapChainDesc, NULL, NULL, &tempSwapChain);
    tempSwapChain->lpVtbl->QueryInterface(tempSwapChain, &IID_IDXGISwapChain3, (void**)&swapChain);
    tempSwapChain->lpVtbl->Release(tempSwapChain);
    factory->lpVtbl->MakeWindowAssociation(factory, hwnd, DXGI_MWA_NO_ALT_ENTER);
    factory->lpVtbl->Release(factory);
    frameIndex = swapChain->lpVtbl->GetCurrentBackBufferIndex(swapChain);
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.NumDescriptors = FRAME_COUNT;
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    device->lpVtbl->CreateDescriptorHeap(device, &rtvHeapDesc, &IID_ID3D12DescriptorHeap, (void**)&rtvHeap);
    rtvDescriptorSize = device->lpVtbl->GetDescriptorHandleIncrementSize(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (UINT i = 0; i < FRAME_COUNT; i++) {
        swapChain->lpVtbl->GetBuffer(swapChain, i, &IID_ID3D12Resource, (void**)&renderTargets[i]);
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtvHeap);
        rtvHandle.ptr += i * rtvDescriptorSize;
        device->lpVtbl->CreateRenderTargetView(device, renderTargets[i], NULL, rtvHandle);
    }
    for (UINT i = 0; i < FRAME_COUNT; i++) {
        device->lpVtbl->CreateCommandAllocator(device, D3D12_COMMAND_LIST_TYPE_DIRECT, &IID_ID3D12CommandAllocator, (void**)&commandAllocators[i]);
    }
}

void LoadAssets() {
    D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
    rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    ID3DBlob* signature;
    ID3DBlob* error;
    D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error);
    device->lpVtbl->CreateRootSignature(device, 0, signature->lpVtbl->GetBufferPointer(signature), signature->lpVtbl->GetBufferSize(signature), &IID_ID3D12RootSignature, (void**)&rootSignature);
    signature->lpVtbl->Release(signature);
    D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    ID3DBlob* vertexShader;
    ID3DBlob* pixelShader;
    D3DCompile("struct VSOut{float4 pos:SV_POSITION;float4 color:COLOR;};"
               "VSOut VS(float3 pos:POSITION,float4 color:COLOR){VSOut o;o.pos=float4(pos,1);o.color=color;return o;}"
               "float4 PS(VSOut input):SV_TARGET{return input.color;}",
               strlen("struct VSOut{float4 pos:SV_POSITION;float4 color:COLOR;};VSOut VS(float3 pos:POSITION,float4 color:COLOR){VSOut o;o.pos=float4(pos,1);o.color=color;return o;}float4 PS(VSOut input):SV_TARGET{return input.color;}"),
               NULL, NULL, "VS", "vs_5_0", 0, 0, &vertexShader, NULL);
    D3DCompile("struct VSOut{float4 pos:SV_POSITION;float4 color:COLOR;};"
               "VSOut VS(float3 pos:POSITION,float4 color:COLOR){VSOut o;o.pos=float4(pos,1);o.color=color;return o;}"
               "float4 PS(VSOut input):SV_TARGET{return input.color;}",
               strlen("struct VSOut{float4 pos:SV_POSITION;float4 color:COLOR;};VSOut VS(float3 pos:POSITION,float4 color:COLOR){VSOut o;o.pos=float4(pos,1);o.color=color;return o;}float4 PS(VSOut input):SV_TARGET{return input.color;}"),
               NULL, NULL, "PS", "ps_5_0", 0, 0, &pixelShader, NULL);
    D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.InputLayout = (D3D12_INPUT_LAYOUT_DESC){inputElementDescs, 2};
    psoDesc.pRootSignature = rootSignature;
    psoDesc.VS.pShaderBytecode = vertexShader->lpVtbl->GetBufferPointer(vertexShader);
    psoDesc.VS.BytecodeLength = vertexShader->lpVtbl->GetBufferSize(vertexShader);
    psoDesc.PS.pShaderBytecode = pixelShader->lpVtbl->GetBufferPointer(pixelShader);
    psoDesc.PS.BytecodeLength = pixelShader->lpVtbl->GetBufferSize(pixelShader);
    psoDesc.RasterizerState = (D3D12_RASTERIZER_DESC){D3D12_FILL_MODE_SOLID, D3D12_CULL_MODE_BACK, FALSE, 0,0.0f,0.0f,TRUE,FALSE,FALSE,FALSE,0,D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF};
    psoDesc.BlendState = (D3D12_BLEND_DESC){FALSE,FALSE,
        {{TRUE,FALSE,
          D3D12_BLEND_ONE,D3D12_BLEND_ZERO,D3D12_BLEND_OP_ADD,
          D3D12_BLEND_ONE,D3D12_BLEND_ZERO,D3D12_BLEND_OP_ADD,
          D3D12_LOGIC_OP_NOOP,D3D12_COLOR_WRITE_ENABLE_ALL}}};
    psoDesc.DepthStencilState.DepthEnable = FALSE;
    psoDesc.DepthStencilState.StencilEnable = FALSE;
    psoDesc.SampleMask = UINT_MAX;
    psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    psoDesc.NumRenderTargets = 1;
    psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
    psoDesc.SampleDesc.Count = 1;
    device->lpVtbl->CreateGraphicsPipelineState(device, &psoDesc, &IID_ID3D12PipelineState, (void**)&pipelineState);
    vertexShader->lpVtbl->Release(vertexShader);
    pixelShader->lpVtbl->Release(pixelShader);
    device->lpVtbl->CreateCommandList(device, 0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocators[frameIndex], pipelineState, &IID_ID3D12GraphicsCommandList, (void**)&commandList);
    Vertex triangleVertices[] = {
        {{ 0.0f,  0.25f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 0.25f,-0.25f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
        {{-0.25f,-0.25f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
    };
    const UINT vertexBufferSize = sizeof(triangleVertices);
    D3D12_HEAP_PROPERTIES heapProps = {D3D12_HEAP_TYPE_UPLOAD};
    D3D12_RESOURCE_DESC resourceDesc = {D3D12_RESOURCE_DIMENSION_BUFFER,0,vertexBufferSize,1,1,1,DXGI_FORMAT_UNKNOWN,{1,0},D3D12_TEXTURE_LAYOUT_ROW_MAJOR, D3D12_RESOURCE_FLAG_NONE};
    device->lpVtbl->CreateCommittedResource(device, &heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, &IID_ID3D12Resource, (void**)&vertexBuffer);
    void* dataBegin;
    D3D12_RANGE readRange = {0,0};
    vertexBuffer->lpVtbl->Map(vertexBuffer, 0, &readRange, &dataBegin);
    memcpy(dataBegin, triangleVertices, sizeof(triangleVertices));
    vertexBuffer->lpVtbl->Unmap(vertexBuffer, 0, NULL);
    vertexBufferView.BufferLocation = vertexBuffer->lpVtbl->GetGPUVirtualAddress(vertexBuffer);
    vertexBufferView.StrideInBytes = sizeof(Vertex);
    vertexBufferView.SizeInBytes = vertexBufferSize;
    commandList->lpVtbl->Close(commandList);
    device->lpVtbl->CreateFence(device, 0, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence, (void**)&fence);
    for (UINT i = 0; i < FRAME_COUNT; i++) {
        fenceValue[i] = 0;
    }
    fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    viewport = (D3D12_VIEWPORT){0.0f, 0.0f, 1280.0f, 720.0f, 0.0f, 1.0f};
    scissorRect = (D3D12_RECT){0, 0, 1280, 720};
}

void PopulateCommandList() {
    commandAllocators[frameIndex]->lpVtbl->Reset(commandAllocators[frameIndex]);
    commandList->lpVtbl->Reset(commandList, commandAllocators[frameIndex], pipelineState);
    commandList->lpVtbl->SetGraphicsRootSignature(commandList, rootSignature);
    commandList->lpVtbl->RSSetViewports(commandList, 1, &viewport);
    commandList->lpVtbl->RSSetScissorRects(commandList, 1, &scissorRect);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = renderTargets[frameIndex];
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commandList->lpVtbl->ResourceBarrier(commandList, 1, &barrier);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->lpVtbl->GetCPUDescriptorHandleForHeapStart(rtvHeap);
    rtvHandle.ptr += frameIndex * rtvDescriptorSize;
    float clearColor[] = {0.0f, 0.2f, 0.4f, 1.0f};
    commandList->lpVtbl->OMSetRenderTargets(commandList, 1, &rtvHandle, FALSE, NULL);
    commandList->lpVtbl->ClearRenderTargetView(commandList, rtvHandle, clearColor, 0, NULL);
    commandList->lpVtbl->IASetPrimitiveTopology(commandList, D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->lpVtbl->IASetVertexBuffers(commandList, 0, 1, &vertexBufferView);
    commandList->lpVtbl->DrawInstanced(commandList, 3, 1, 0, 0);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    commandList->lpVtbl->ResourceBarrier(commandList, 1, &barrier);
    commandList->lpVtbl->Close(commandList);
}

void Render() {
    PopulateCommandList();
    ID3D12CommandList* ppCommandLists[] = { (ID3D12CommandList*)commandList };
    commandQueue->lpVtbl->ExecuteCommandLists(commandQueue, 1, ppCommandLists);
    swapChain->lpVtbl->Present(swapChain, 1, 0);
    WaitForPreviousFrame();
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    WNDCLASSEX wc = {sizeof(WNDCLASSEX), CS_HREDRAW|CS_VREDRAW, WndProc, 0, 0, hInstance, 0, 0, 0, 0, "DX12WindowClass", 0};
    RegisterClassEx(&wc);
    HWND hwnd = CreateWindow("DX12WindowClass", "Hello World Triangle", WS_OVERLAPPEDWINDOW, 100, 100, 1280, 720, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    LoadPipeline(hwnd);
    LoadAssets();
    MSG msg = {};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            Render();
        }
    }
    WaitForPreviousFrame();
    CloseHandle(fenceEvent);
    for (UINT i = 0; i < FRAME_COUNT; i++) {
        if (renderTargets[i]) renderTargets[i]->lpVtbl->Release(renderTargets[i]);
        if (commandAllocators[i]) commandAllocators[i]->lpVtbl->Release(commandAllocators[i]);
    }
    if (vertexBuffer) vertexBuffer->lpVtbl->Release(vertexBuffer);
    if (pipelineState) pipelineState->lpVtbl->Release(pipelineState);
    if (commandList) commandList->lpVtbl->Release(commandList);
    if (rtvHeap) rtvHeap->lpVtbl->Release(rtvHeap);
    if (rootSignature) rootSignature->lpVtbl->Release(rootSignature);
    if (swapChain) swapChain->lpVtbl->Release(swapChain);
    if (commandQueue) commandQueue->lpVtbl->Release(commandQueue);
    if (device) device->lpVtbl->Release(device);
    if (fence) fence->lpVtbl->Release(fence);
    return (int) msg.wParam;
}
