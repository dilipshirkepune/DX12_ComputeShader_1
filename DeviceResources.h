#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>

using namespace Microsoft::WRL;

void CreateDeviceAndCommandObjects(
    ComPtr<ID3D12Device>& device,
    ComPtr<ID3D12CommandQueue>& commandQueue,
    ComPtr<ID3D12CommandAllocator>& commandAllocator,
    ComPtr<ID3D12GraphicsCommandList> & commandList);
