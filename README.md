# Introduction

ShaderCrashingAssert is a small library for programmers who use C++, Windows, and Direct3D 12. It provides an assert-like macro for HLSL shader language that causes memory page fault. Together with GPU crash analysis tools like **[Radeon GPU Detective](https://gpuopen.com/radeon-gpu-detective/)** for AMD cards it can provide help in shader debugging.

Author: Adam Sawicki - http://asawicki.info<br>
Version: 0.0.1, 2023-08-19<br>
License: MIT

# User guide

This is a single-header library, so you only need to copy file "ShaderCrashingAssert.h" into your project. It is truly header-only library, not "STB-style", which means all the functions are defined as inline and there is no need to extract the implementation into a .cpp file with some macro.

## Integration in C++ code

1. Include the library in your C++ code:
   
   ```
   #include "ShaderCrashingAssert.h"
   ```
   
   Direct3D 12 `<d3d12.h>` must be included earlier.

2. Create and initialize the main context object. The object should be created after `ID3D12Device` and destroyed before the device is destroyed.

   ```
   ID3D12Device* device = ...

   SHADER_CRASHING_ASSERT_CONTEXT_DESC ctxDesc = {};
   ctxDesc.pDevice = device;
   ShaderCrashingAssertContext* ctx = new ShaderCrashingAssertContext();
   HRESULT hr = ctx->Init(&ctxDesc);
   if(FAILED(hr)) ... // Handle error

   // YOUR APPLICATION WORKING HERE

   delete ctx;
   device->Release();
   ```

3. Prepare a UAV descriptor. This library needs a special descriptor to work. Context object provides a CPU handle to that descriptor in a non-shader-visible descriptor heap, which you need to copy into your shader-visible descriptor heap that you use for rendering.

   ```
   device->CopyDescriptorsSimple(
       1, // NumDescriptors
       myDescriptorHeap->GetCPUHandle(), // DestDescriptorRangeStart
       ctx->GetUAVCPUDescriptorHandle(), // SrcDescriptorRangeStart
       D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); // DescriptorHeapsType
   ```

4. In your root signature, declare a UAV where this descriptor will be passed. It can be located at any register slot and space of your choice (for example: slot u6 in space7).

5. Before issuing a draw call or a compute dispatch that uses the assert macro, you need to set this descriptor at the correct root parameter index:

   ```
   cmdList->SetGraphicsRootDescriptorTable(rootParameterIndex, myDescriptorHeap->GetGPUHandle());
   ```

*If these steps required for resource binding in D3D12 look too compilcated for you, this article can help understand them: [Direct3D 12: Long Way to Access Data](https://asawicki.info/news_1754_direct3d_12_long_way_to_access_data).*

## Integration in HLSL shader code

1. Include the library:
   
   ```
   #include "ShaderCrashingAssert.h"
   ```

   Yes, this is not a mistake. The file automatically uses CPU/C++ or GPU/HLSL part depending on predefined macros.

2. Declare the UAV resource needed by the library using provided macro. You need to specify register number and space matching those declared in the root signature.

   ```
   SHADER_CRASHING_ASSERT_RESOURCE : register(u6, space7);
   ```

3. Finally, in the HLSL function where you want to check some critical condition, use the macro `SHADER_CRASHING_ASSERT(expr)`. Argument should be boolean with true value when everything is OK. When false, a memory page fault is triggered.

   ```
   float3 color = ...
   SHADER_CRASHING_ASSERT(!any(isnan(color)));
   ```

## Usage

A GPU memory page fault generally results in an undefined behavior. The application may observe an error like `DXGI_ERROR_DEVICE_REMOVED`, `DXGI_ERROR_DEVICE_HUNG`, `DXGI_ERROR_DEVICE_RESET` returned from one of the D3D12/DXGI functions in the same rendering frame, in some future frame, or it may continue normally.

However, with **[Radeon GPU Detective](https://gpuopen.com/radeon-gpu-detective/)** active (Radeon Developer Panel launched and set to "Crash Analysis" mode), such crash seems to be captured more reliably. Application still observes D3D12 error, but RGD can then show information about the render pass and draw call or dispatch that triggered it. To make sure it was the assert from this library and not some other GPU failure, look for resources named "ShaderCrashingAssert" in the RGD output. For more information, check [RGD tutorial](https://gpuopen.com/learn/rgd_1_0_tutorial/) or documentation of the tool: [Quickstart Guide](https://radeon-gpu-detective.readthedocs.io/en/latest/quickstart.html) and [Help Manual](https://radeon-gpu-detective.readthedocs.io/en/latest/help_manual.html).

**Please remember that this whole library is a hack and may not be fully reliable. On some systems, GPUs, with some applications, crash may not happen despite the assert is triggered. Please always test asserting unconditionally before using this library for debugging.**

# Technical considerations

## How does it work?

The library creates a small UAV buffer, a raw UAV descriptor for it, declares a `RWByteAddressBuffer` resource in shader code, and performs a `Store()` to it in the main assert macro. The buffer and its heap is released soon after creation, so the descriptor points to an incorrect address, which triggers the page fault.

The need to use a UAV resouce is an inconvenience, but it is necessary because shaders don't have a free-form pointers to be able to just reach out to some address.

D3D Debug Layer doesn't report this logic as an error because it doesn't track the content of the descriptors. GPU-based validation may be able to find it - I didn't check.

## Perspectives for returning extra data

It would be nice to be able to return additional data when an assert is hit. For example, some `SHADER_CRASHING_ASSERT2(uint val)` could be defined that crashes whenever argument is non-zero and returns that value somehow.

1. One possible idea is to encode this value in the offending virtual address (VA) of the crash. Unfortunately, I couldn't make it working.

   1. Possibly least significant bits of the address could be controlled by performing the UAV `Store()` to a specific offsets in the buffer instead of zero. Unfortunately, it doesn't work - when tested, RGD still returns an address like 0x2008f9000. The address is likely aligned down to 0x1000 = 4 KB.

   2. Possibly, higher bits could be used if we know that the beginning of the buffer is aligned to some large number. Theoretically, a `ID3D12Heap` can be created with specific alignment, notably `D3D12_DEFAULT_MSAA_RESOURCE_PLACEMENT_ALIGNMENT`, equal to 4194304 = 4 MB, which is required when placing MSAA textures inside. Unfortunately, it doesn't work. Even when using this alignment, also with `D3D12_HEAP_FLAG_NONE` instead of `D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS`, RGD still shows an address like 0x2008f9000, so the heap is still aligned only to 4K.

2. Other potential solution is to simply create several buffers and choose between them when triggering the assert. They could be then distinguished by their string names or differnet sizes in the RGD report. If you are interested in having this feature, please let me know by creating an Issue ticket in this repository.

# Final words

Thanks to Jasper Bekkers from Traverse Research for the inspiration!
