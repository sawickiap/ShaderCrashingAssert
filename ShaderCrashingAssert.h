/*
ShaderCrashingAssert: Small library for D3D12. Provides assert-like macro for HLSL that crashes the GPU.

Author:  Adam Sawicki - http://asawicki.info - adam__DELETE__@asawicki.info
Version: 0.0.1, 2023-08-19
License: MIT

Documentation: see README.md in the repository: https://github.com/sawickiap/ShaderCrashingAssert

# Version history

Version 0.0.1, 2023-08-19

    First version.

# License

Copyright (c) 2023 Adam Sawicki

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef SHADER_CRASHING_ASSERT_H_
#define SHADER_CRASHING_ASSERT_H_

////////////////////////////////////////////////////////////////////////////////
// C++ section
#ifdef __cplusplus

#ifndef __d3d12_h__
    #error Please #include <d3d12.h> before including "ShaderCrashingAssert.h"
#endif

struct SHADER_CRASHING_ASSERT_CONTEXT_DESC
{
    ID3D12Device* pDevice;
};

class ShaderCrashingAssertContext
{
public:
    HRESULT Init(const SHADER_CRASHING_ASSERT_CONTEXT_DESC* pDesc)
    {
        constexpr UINT64 size = 32;
        HRESULT hr = S_OK;

        // Create heap
        ID3D12Heap* heap = nullptr;
        {
            D3D12_HEAP_DESC heapDesc = {};
            heapDesc.SizeInBytes = size;
            heapDesc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            heapDesc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
            hr = pDesc->pDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&heap));
        }
        if (SUCCEEDED(hr))
        {
            heap->SetName(L"ShaderCrashingAssert Heap");
        }

        // Create buffer
        ID3D12Resource* buf = nullptr;
        if (SUCCEEDED(hr))
        {
            D3D12_RESOURCE_DESC resDesc = {};
            resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            resDesc.Width = size;
            resDesc.Height = 1;
            resDesc.DepthOrArraySize = 1;
            resDesc.MipLevels = 1;
            resDesc.SampleDesc.Count = 1;
            resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            hr = pDesc->pDevice->CreatePlacedResource(heap, 0, &resDesc, D3D12_RESOURCE_STATE_COMMON, nullptr,
                IID_PPV_ARGS(&buf));
        }
        if (SUCCEEDED(hr))
        {
            buf->SetName(L"ShaderCrashingAssert Buffer");
        }

        // Create descriptor heap
        if (SUCCEEDED(hr))
        {
            D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
            descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            descHeapDesc.NumDescriptors = 1;
            hr = pDesc->pDevice->CreateDescriptorHeap(&descHeapDesc, IID_PPV_ARGS(&m_pDescriptorHeap));
        }
        if (SUCCEEDED(hr))
        {
            m_pDescriptorHeap->SetName(L"ShaderCrashingAssert Descriptor Heap");
        }

        // Setup the descriptor
        if (SUCCEEDED(hr))
        {
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
            uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
            uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
            uavDesc.Buffer.NumElements = size / sizeof(DWORD);
            pDesc->pDevice->CreateUnorderedAccessView(buf, NULL, &uavDesc,
                m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
        }

        // Release buffer and heap
        if (buf)
        {
            buf->Release();
        }
        if (heap)
        {
            heap->Release();
        }

        return hr;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GetUAVCPUDescriptorHandle()
    {
        return m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
    }

private:
    ID3D12DescriptorHeap* m_pDescriptorHeap = nullptr;
};

////////////////////////////////////////////////////////////////////////////////
// HLSL section
#else // #ifdef __cplusplus

#define SHADER_CRASHING_ASSERT_RESOURCE \
    globallycoherent RWByteAddressBuffer ShaderCrashingAssertResource1

#define SHADER_CRASHING_ASSERT(expr) \
    do { \
        [branch] if (!(expr)) \
        { \
            ShaderCrashingAssertResource1.Store( \
                0, \
                0x23898f4a); \
        } \
    } while(false)
// That specific numerical value doesn't matter.

#endif // #ifdef __cplusplus

#endif // #ifndef SHADER_CRASHING_ASSERT_H_
