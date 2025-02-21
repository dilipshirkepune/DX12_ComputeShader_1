#include "DeviceResources.h"
#include <stdexcept>

void CreateDeviceAndCommandObjects(
    ComPtr<ID3D12Device>& device,
    ComPtr<ID3D12CommandQueue>& commandQueue,
    ComPtr<ID3D12CommandAllocator>& commandAllocator,
    ComPtr<ID3D12GraphicsCommandList>& commandList)
{

    UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // Create the DXGI factory
    ComPtr<IDXGIFactory4> factory;
    HRESULT hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create DXGI factory");
    }

    // Create the device
    ComPtr<IDXGIAdapter1> hardwareAdapter;
    for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(adapterIndex, &hardwareAdapter); ++adapterIndex)
    {
        DXGI_ADAPTER_DESC1 desc;
        hardwareAdapter->GetDesc1(&desc);

        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        {
            // Don't select the Basic Render Driver adapter.
            continue;
        }

        hr = D3D12CreateDevice(hardwareAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
        if (SUCCEEDED(hr))
        {
            break;
        }
    }

    if (!device)
    {
        throw std::runtime_error("Failed to create D3D12 device");
    }

    // Create the command queue
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create command queue");
    }

    // Create the command allocator
    hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create command allocator");
    }

    // Create the command list
    hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator.Get(), nullptr, IID_PPV_ARGS(&commandList));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create command list");
    }

    // Close the command list as it needs to be reset before being used
    commandList->Close();
}