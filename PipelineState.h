#pragma once

#include <d3d12.h>
#include <wrl.h>

using namespace Microsoft::WRL;

ComPtr<ID3D12PipelineState> CreateComputePipelineState(ID3D12Device* device, ComPtr<ID3DBlob> computeShader, ComPtr<ID3D12RootSignature>& rootSignature);

UINT ReadBackR8UNormValues(ID3D12Device* device, ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator, ID3D12PipelineState* pipelineState, ID3D12RootSignature* rootSignature, UINT width, UINT height, UINT threadGroupSize);