#include "PipelineState.h"
#include <stdexcept>
#include <iostream>
#include "d3dx12.h"
#include <vector>
#include <random>
#include <algorithm>
#include <fstream>
#define MAX_VALUE_FOR_RANDOM    100

ComPtr<ID3D12PipelineState> CreateComputePipelineState(ID3D12Device* device, ComPtr<ID3DBlob> computeShader, ComPtr<ID3D12RootSignature>& rootSignature)
{
    // Create the root signature
    CD3DX12_DESCRIPTOR_RANGE1 ranges[2];
    ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER1 rootParameters[2];
    rootParameters[0].InitAsDescriptorTable(1, &ranges[0]);
    rootParameters[1].InitAsDescriptorTable(1, &ranges[1]);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
    rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    HRESULT hr = D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &signature, &error);
    if (FAILED(hr))
    {
        if (error)
        {
            std::cerr << static_cast<char*>(error->GetBufferPointer()) << std::endl;
        }
        throw std::runtime_error("Failed to serialize root signature");
    }

    hr = device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create root signature");
    }

    // Create the compute pipeline state
    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
    psoDesc.pRootSignature = rootSignature.Get();
    psoDesc.CS = { reinterpret_cast<BYTE*>(computeShader->GetBufferPointer()), computeShader->GetBufferSize() };

    ComPtr<ID3D12PipelineState> pipelineState;
    hr = device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create compute pipeline state");
    }

    return pipelineState;
}

UINT ReadBackR8UNormValues(ID3D12Device* device, ID3D12CommandQueue* commandQueue, ID3D12GraphicsCommandList* commandList, ID3D12CommandAllocator* commandAllocator, ID3D12PipelineState* pipelineState, ID3D12RootSignature* rootSignature, UINT width, UINT height, UINT threadGroupSize)
{
    // Reset command allocator and list
    commandAllocator->Reset();
    commandList->Reset(commandAllocator, pipelineState);

    // Create input texture
    D3D12_RESOURCE_DESC textureDesc = {};
    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = width;
    textureDesc.Height = height;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_R8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    textureDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    CD3DX12_HEAP_PROPERTIES defaultHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
    ComPtr<ID3D12Resource> inputTexture;
    device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &textureDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&inputTexture));

    // Create upload buffer
    UINT64 uploadBufferSize;
    device->GetCopyableFootprints(&textureDesc, 0, 1, 0, nullptr, nullptr, nullptr, &uploadBufferSize);
    CD3DX12_HEAP_PROPERTIES uploadHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
    D3D12_RESOURCE_DESC uploadBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    ComPtr<ID3D12Resource> uploadBuffer;
    device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &uploadBufferDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));

    // Initialize texture with random data

    std::vector<uint8_t> textureBytes(width * height);
    std::generate(textureBytes.begin(), textureBytes.end(), []() { return static_cast<uint8_t>(rand() % MAX_VALUE_FOR_RANDOM); });

    // Write texture data to a text file
    std::ofstream outFile("textureData.txt");
    if (outFile.is_open())
    {
        for (size_t i = 0; i < textureBytes.size(); ++i)
        {
            outFile << static_cast<int>(textureBytes[i]) << " ";
            if ((i + 1) % width == 0)
            {
                outFile << "\n";
            }
        }
        outFile.close();
    }
    else
    {
        throw std::runtime_error("Failed to open textureData.txt for writing");
    }

    D3D12_SUBRESOURCE_DATA textureData = {};
    textureData.pData = textureBytes.data();
    textureData.RowPitch = width;
    textureData.SlicePitch = textureData.RowPitch * height;
    UpdateSubresources(commandList, inputTexture.Get(), uploadBuffer.Get(), 0, 0, 1, &textureData);

    // Transition texture to readable state
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(inputTexture.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
    commandList->ResourceBarrier(1, &barrier);

    // Create intermediate buffer
    D3D12_RESOURCE_DESC intermediateBufferDesc = CD3DX12_RESOURCE_DESC::Buffer((width / threadGroupSize) * (height / threadGroupSize) * sizeof(UINT), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    ComPtr<ID3D12Resource> intermediateBuffer;
    device->CreateCommittedResource(&defaultHeapProperties, D3D12_HEAP_FLAG_NONE, &intermediateBufferDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&intermediateBuffer));

    // Create readback buffer
    CD3DX12_HEAP_PROPERTIES readbackHeapProperties(D3D12_HEAP_TYPE_READBACK);
    D3D12_RESOURCE_DESC readbackBufferDesc = CD3DX12_RESOURCE_DESC::Buffer((width / threadGroupSize) * (height / threadGroupSize) * sizeof(UINT));
    ComPtr<ID3D12Resource> readbackBuffer;
    device->CreateCommittedResource(&readbackHeapProperties, D3D12_HEAP_FLAG_NONE, &readbackBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&readbackBuffer));

    // Create descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.NumDescriptors = 2;
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> descriptorHeap;
    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));

    // Create SRV for input texture
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8_UINT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(inputTexture.Get(), &srvDesc, descriptorHeap->GetCPUDescriptorHandleForHeapStart());

    // Create UAV for intermediate buffer
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.NumElements = (width / threadGroupSize) * (height / threadGroupSize);
    uavDesc.Buffer.StructureByteStride = sizeof(UINT);
    CD3DX12_CPU_DESCRIPTOR_HANDLE uavHandle(descriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    device->CreateUnorderedAccessView(intermediateBuffer.Get(), nullptr, &uavDesc, uavHandle);

    // Set pipeline state and root signature
    commandList->SetPipelineState(pipelineState);
    commandList->SetComputeRootSignature(rootSignature);
    ID3D12DescriptorHeap* heaps[] = { descriptorHeap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // Set SRV and UAV descriptor tables
    CD3DX12_GPU_DESCRIPTOR_HANDLE srvHandle(descriptorHeap->GetGPUDescriptorHandleForHeapStart());
    CD3DX12_GPU_DESCRIPTOR_HANDLE uavHandleGpu(descriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV));
    commandList->SetComputeRootDescriptorTable(0, srvHandle);
    commandList->SetComputeRootDescriptorTable(1, uavHandleGpu);

    // Create query heap for timestamp queries
    D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
    queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
    queryHeapDesc.Count = 2;
    ComPtr<ID3D12QueryHeap> queryHeap;
    device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&queryHeap));

    // Create readback buffer for timestamps
    D3D12_RESOURCE_DESC timestampBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(2 * sizeof(UINT64));
    ComPtr<ID3D12Resource> timestampBuffer;
    device->CreateCommittedResource(&readbackHeapProperties, D3D12_HEAP_FLAG_NONE, &timestampBufferDesc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&timestampBuffer));

    // Record start timestamp
    commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0);

    // Dispatch compute shader
    commandList->Dispatch((width + (threadGroupSize - 1)) / threadGroupSize, (height + (threadGroupSize-1)) / threadGroupSize, 1);

    // Record end timestamp
    commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 1);

    // Resolve query data
    commandList->ResolveQueryData(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, 0, 2, timestampBuffer.Get(), 0);


    // Transition intermediate buffer to copy source state
    CD3DX12_RESOURCE_BARRIER barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(intermediateBuffer.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
    commandList->ResourceBarrier(1, &barrier2);

    // Copy intermediate buffer to readback buffer
    commandList->CopyResource(readbackBuffer.Get(), intermediateBuffer.Get());

    // Close command list
    commandList->Close();

    // Execute command list
    ID3D12CommandList* commandLists[] = { commandList };
    commandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

    // Wait for GPU to finish
    ComPtr<ID3D12Fence> fence;
    device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
    HANDLE eventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    if (eventHandle == nullptr) throw std::runtime_error("Failed to create event handle");

    commandQueue->Signal(fence.Get(), 1);
    if (fence->GetCompletedValue() < 1)
    {
        fence->SetEventOnCompletion(1, eventHandle);
        WaitForSingleObject(eventHandle, INFINITE);
    }
    CloseHandle(eventHandle);

    // Map readback buffer and find maximum value
    void* mappedData;
    readbackBuffer->Map(0, nullptr, &mappedData);
    UINT* data = static_cast<UINT*>(mappedData);
    UINT maxValue = *std::max_element(data, data + (width / threadGroupSize) * (height / threadGroupSize));
    readbackBuffer->Unmap(0, nullptr);

    // Map timestamp buffer and calculate GPU time
    UINT64* timestamps;
    timestampBuffer->Map(0, nullptr, reinterpret_cast<void**>(&timestamps));
    UINT64 gpuTime = timestamps[1] - timestamps[0];
    timestampBuffer->Unmap(0, nullptr);

    // Get GPU timestamp frequency
    UINT64 frequency;
    commandQueue->GetTimestampFrequency(&frequency);

    // Calculate GPU time in milliseconds
    double gpuTimeMs = (gpuTime * 1000.0) / frequency;

    std::cout << "GPU Time: " << gpuTimeMs << " ms" << std::endl;
        
    return maxValue; ;
}