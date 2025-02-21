#include "ShaderUtils.h"
#include <stdexcept>
#include <iostream>

#include <d3dcompiler.h>
#include <wrl.h>
#include <stdexcept>
#include <fstream>
#include <vector>


using namespace Microsoft::WRL;

ComPtr<ID3DBlob> LoadCompiledShader(const std::wstring& filename)
{
    std::ifstream shaderFile;
    shaderFile.open(filename, std::ios::binary | std::ios::ate);

    if (!shaderFile)
    {
        throw std::runtime_error("Failed to open shader file: " + std::string(filename.begin(), filename.end()));
    }

    std::streamsize size = shaderFile.tellg();
    shaderFile.seekg(0, std::ios::beg);

    ComPtr<ID3DBlob> shaderBlob;
    HRESULT hr = D3DCreateBlob(size, &shaderBlob);
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create shader blob");
    }

    if (!shaderFile.read(reinterpret_cast<char*>(shaderBlob->GetBufferPointer()), size))
    {
        throw std::runtime_error("Failed to read shader file: " + std::string(filename.begin(), filename.end()));
    }

    return shaderBlob;
}

ComPtr<ID3DBlob> CompileComputeShader(const std::wstring& shaderPath)
{
    ComPtr<ID3DBlob> computeShader;
    ComPtr<ID3DBlob> errorBlob;

#if defined(_DEBUG)
    // Enable better shader debugging with the graphics debugging tools.
    UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_STRICTNESS;
#else
    UINT compileFlags = 0;
#endif

    HRESULT hr = D3DCompileFromFile(
        shaderPath.c_str(),    // Path to the shader file
        nullptr,               // Optional macros
        D3D_COMPILE_STANDARD_FILE_INCLUDE,               // Optional include handler
        "CSMain",              // Entry point function name
        "cs_5_0",              // Target shader model
        compileFlags,                     // Compile options
        0,                     // Effect compile options
        &computeShader,        // Compiled shader
        &errorBlob             // Error messages
    );

    
    if (FAILED(hr))
    {
        if (errorBlob)
        {
            std::cerr << static_cast<char*>(errorBlob->GetBufferPointer()) << std::endl;
        }
        throw std::runtime_error("Failed to compile compute shader");
    }

    return computeShader;
}


