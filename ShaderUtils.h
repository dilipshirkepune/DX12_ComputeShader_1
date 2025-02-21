#pragma once

#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl.h>
#include <string>

using namespace Microsoft::WRL;

ComPtr<ID3DBlob> CompileComputeShader(const std::wstring & shaderPath);
ComPtr<ID3DBlob> LoadCompiledShader(const std::wstring& filename);
