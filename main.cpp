#include "DeviceResources.h"
#include "ShaderUtils.h"
#include "PipelineState.h"
#include <vector>
#include <numeric>
#include <iostream>
#include <algorithm>

int main()
{
    ComPtr<ID3D12Device> device;
    ComPtr<ID3D12CommandQueue> commandQueue;
    ComPtr<ID3D12CommandAllocator> commandAllocator;
    ComPtr<ID3D12GraphicsCommandList> commandList;

    CreateDeviceAndCommandObjects(device, commandQueue, commandAllocator, commandList);

    // Load the compiled shader
    ComPtr<ID3DBlob> computeShader8x8x1;
    ComPtr<ID3DBlob> computeShader16x16x1;
    ComPtr<ID3DBlob> computeShader32x32x1;
    try
    {
        computeShader8x8x1 = LoadCompiledShader(L"ComputeShader8x8x1.cso");
        computeShader16x16x1 = LoadCompiledShader(L"ComputeShader16x16x1.cso");
        computeShader32x32x1 = LoadCompiledShader(L"ComputeShader32x32x1.cso");
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return -1;
    }    

    // Define texture dimensions
    std::vector<std::pair<UINT, UINT>> textureSizes = 
    {
        {64, 64},
        {128, 128},
        {256, 256},
        {512, 512},
        {1024, 1024}
    };

    ComPtr<ID3D12RootSignature> rootSignature;
    ComPtr<ID3D12PipelineState> pipelineState;

    // Define thread group sizes
    std::vector<UINT> threadGroupSizes = { 8, 16, 32 };

    // Measure performance over multiple runs for each texture size and thread group size
    for (const auto& size : textureSizes)
    {
        UINT width = size.first;
        UINT height = size.second;

        std::cout << "Texture Size: " << width << "x" << height << std::endl;
        for (UINT threadGroupSize : threadGroupSizes)
        {
            std::cout << "Thread Group Size: " << threadGroupSize << "x" << threadGroupSize << std::endl;
            if (threadGroupSize == 8)
            {
                pipelineState = CreateComputePipelineState(device.Get(), computeShader8x8x1, rootSignature);
            }
            else if (threadGroupSize == 16)
            {
                pipelineState = CreateComputePipelineState(device.Get(), computeShader16x16x1, rootSignature);
            }
            else if (threadGroupSize == 32)
            {
                pipelineState = CreateComputePipelineState(device.Get(), computeShader32x32x1, rootSignature);
            }
            else // default 16x16x1
            {
                pipelineState = CreateComputePipelineState(device.Get(), computeShader16x16x1, rootSignature);
            }
            {
            }
            const int numRuns = 10;
            std::vector<UINT> maxValues;
            for (int i = 0; i < numRuns; ++i)
            {
                UINT maxValue = ReadBackR8UNormValues(device.Get(), commandQueue.Get(), commandList.Get(), commandAllocator.Get(), pipelineState.Get(), rootSignature.Get(), width, height, threadGroupSize);
                maxValues.push_back(maxValue);
            }

            // Calculate the final maximum value
            UINT finalMaxValue = *std::max_element(maxValues.begin(), maxValues.end());
            std::cout << "Final Max Value: " << finalMaxValue << std::endl;
            std::cout << "----------------------------------------------------" << std::endl;
        }
    }

    return 0;
}