# Driver bugs

Non-exhaustive list of known drivers bugs.

See also: [Dolphin emulator](hhttps://github.com/dolphin-emu/dolphin/blob/cdbea8867df3d0a6fc375e78726dae95612fb1fd/Source/Core/VideoCommon/DriverDetails.h#L84) own list of driver bugs (which are also included here).

## Vulkan

| Vendor/GPU | OS | Drivers | Version | Bug |
|---|---|---|---|---|
| AMD | All | Proprietary | ? | `VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT` on GCN4 and lower is broken. |
| AMD | All | Proprietary | ? | GCN4 and earlier have broken `VK_EXT_sampler_filter_minmax`. |
| AMD | All | Proprietary | ? | If offset + stride * count is greater than the size, then the last attribute will have the wrong value for vertex buffers. |
| AMD | All | Proprietary | ? | On GCN, 2D mipmapped texture arrays (with width == height) where there are more than 6 layers causes broken corrupt mipmaps. |
| AMD | All | RADV, MESA | ? | Using LLVM emitter causes corrupt FP16, proprietary drivers unaffected. |
| AMD | All | Proprietary | ? | Incorrect implementation of VK_EXT_depth_clamp_control causes incorrect depth values to be written to the depth buffer. |
| AMD | macOS | Proprietary | ? | `gl_HelperInvocation` is actually `!gl_HelperInvocation`. |
| NVIDIA | All | Proprietary | ? | Drivers for ampere and newer have broken float16 math. |
| NVIDIA | All | Proprietary | >=510.0.0 | Versions >= 510 do not support MSAA->MSAA image blits. |
| NVIDIA | All | Proprietary | ? | Shader stencil export not supported. |
| NVIDIA | All | Proprietary | ? | Doesn't properly support conditional barriers. |
| NVIDIA | All | Proprietary | ? | GPUs pre-turing doesn't properly work with push descriptors. |
| NVIDIA | All | All | ? | Calling `vkCmdClearAttachments` with a partial rect, or specifying a render area in a render pass with the load op set to clear can cause the GPU to lock up, or raise a bounds violation. This only occurs on MSAA framebuffers, and it seems when there are multiple clears in a single command buffer. Worked around by back to the slow path (drawing quads) when MSAA is enabled. |
| Intel | All | Proprietary | <27.20.100.0 | Intel Windows versions before 27.20.100.0 has broken `VK_EXT_vertex_input_dynamic_state`. |
| Intel | All | Proprietary | ? | Intel proprietary drivers do not support MSAA->MSAA image blits. |
| Intel | All | Proprietary | 0.405.0<br>until<br>0.405.286 | Intel proprietary drivers 0.405.0 until 0.405.286 have broken compute. |
| Intel | macOS | Proprietary | ? | Using dynamic sampler indexing locks up the GPU. |
| Intel | macOS | Proprietary | ? | Using subgroupMax in a shader that can discard results in garbage data. |
| Intel | All | Mesa | ? | Broken lines in geometry shaders when writing to `gl_ClipDistance` in the vertex shader. |
| Qualcomm | All | Proprietary | ? | Using too many samplers (on A8XX is `65536`) can cause heap exhaustion, recommended to use 75% of total available samplers. |
| Qualcomm | All | Proprietary | ? | Qualcomm Adreno GPUs doesn't handle scaled vertex attributes `VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME`. |
| Qualcomm | All | Proprietary | ? | 64-bit integer extensions (`VK_KHR_SHADER_ATOMIC_INT64_EXTENSION_NAME`) cause nondescriptive crashes and stability issues. |
| Qualcomm | All | All | ? | Driver requires higher-than-reported binding limits (32). |
| Qualcomm | All | All | ? | On Adreno, enabling rasterizer discard somehow corrupts the viewport state; a workaround is forcing it to be updated on next use. |
| Qualcomm | All | Proprietary | ? | Concurrent fence waits are unsupported. |
| Qualcomm | All | Proprietary | ? | `vkCmdCopyImageToBuffer` allocates a staging image when used to copy from an image with optimal tiling; which results in overall slower performance. |
| Qualcomm | All | Proprietary | ? | 32-bit depth clears are broken in the Adreno Vulkan driver, and have no effect. |
| Qualcomm | All | Proprietary | ? | It should be safe to release the resources before actually resetting the VkCommandPool. However, devices running R drivers there was a few months period where the driver had a bug which it incorrectly was accessing objects on the command buffer while it was being reset. If these objects were already destroyed (which is a valid thing to do) it would crash. |
| Qualcomm | All | Proprietary | ? | On Pixel and Pixel2XL's with Adreno 530 and 540s, setting width and height to 10s reliably triggers what appears to be a driver race condition. |
| Qualcomm | All | Proprietary | ? | Non-descriptive broken OpPhi SPIRV lowering, originally using OpPhi to choose the result is crashing on Adreno 4xx. Switched to storing the result in a temp variable as glslang does. |
| Qualcomm | All | Proprietary | ? | See crbug.com/1241134. The bug appears on Adreno 5xx devices with OS PQ3A. It does not repro on the earlier PPR1 version since the extend blend func extension was not present on the older driver. |
| Mali | Android | Proprietary | ? | Non-descriptive green screen on various locations. |
| Mali | Android | Proprietary | ? | Cached memory is significantly slower for readbacks than coherent memory in the Mali Vulkan driver, causing high CPU usage in the `__pi___inval_cache_range` kernel function. |
| Mali | Android | Proprietary | ? | On some old ARM driver versions, dynamic state for stencil write mask doesn't work correctly in the presence of discard or alpha to coverage, if the static state provided when creating the pipeline has a value of 0 (`alphaToCoverageEnable` and `rasterizerDiscardEnable`). |
| Mali | Android | Proprietary | ? | Failing to submit because of a device loss still needs to wait for the fence to signal before deleting. However, there is an ARM bug (b/359822580) where the driver early outs on the fence wait if in a device lost state and thus we can't wait on it. Instead,  we just wait on the queue to finish. |
| Mali | Android | Proprietary | ? | With Galaxy S7 we see lots of rendering issues when we suballocate VkImages. |
| Mali | Android | Proprietary | ? | With Galaxy S7 and S9 we see lots of rendering issues with image filters dropping out when using only primary command buffers. We also see issues on the P30 running Android 28. |
| Mali | Android | Proprietary | ? | `RGBA_F32` mipmaps appear to be broken on some Mali devices. |
| Mali | Android | Proprietary | ? | Matrix IR lowering for matrix swizzle, scalar multiplication and unary `(+m)`/`(-m)` present extraneous unexplained bugs with more than 32 matrix temporals. |
| Apple | All | MoltenVK | ? | Driver breaks when using more than 16 vertex attributes/bindings. |
| Apple | macOS | Proprietary | >4 | Some driver and Apple Silicon GPU combinations have problems with fragment discard when early depth test is enabled. Discarded fragments may appear corrupted. |
| Apple | iOS | MoltenVK | ? | Null descriptors cause non-descriptive issues. |
| Apple | iOS | MoltenVK | ? | Push descriptors cause non-descriptive issues. |
| Imagination | Android | Proprietary | ? | Some vulkan implementations don't like the 'clear' loadop renderpass if you try to use a framebuffer with a different load/store op than that which it was created with, despite the spec saying they should be compatible. |
| Intel<br>Qualcomm<br>AMD<br>Apple | All | All<br>MoltenVK (Apple) | ? | Reversed viewport depth range does not work as intended on some Vulkan drivers. The Vulkan spec allows the `minDepth`/`maxDepth` fields in the viewport to be reversed, however the implementation is broken on some drivers. |

AMD: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_AMD_PROPRIETARY`
- OSS: `VK_DRIVER_ID_AMD_OPEN_SOURCE`
- MESA: `VK_DRIVER_ID_MESA_RADV`

NVIDIA: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_NVIDIA_PROPRIETARY`.
- MESA: `VK_DRIVER_ID_MESA_NVK`.

Intel: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_INTEL_PROPRIETARY_WINDOWS`.
- MESA: `VK_DRIVER_ID_INTEL_OPEN_SOURCE_MESA`.

Qualcomm: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_QUALCOMM_PROPRIETARY`.
- MESA: `VK_DRIVER_ID_MESA_TURNIP`.

Mali: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_ARM_PROPRIETARY`.
- MESA: `VK_DRIVER_ID_MESA_PANVK`.

Samsung: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_SAMSUNG_PROPRIETARY`.

Apple: List of driver IDs:
- MoltenVK/MVK: `VK_DRIVER_ID_MOLTENVK`.
- KosmicKrisp: `VK_DRIVER_ID_MESA_KOSMICKRISP`.
- HoneyKrisp: `VK_DRIVER_ID_MESA_HONEYKRISP`.

PowerVR: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_IMAGINATION_PROPRIETARY`.
- MESA: `VK_DRIVER_ID_IMAGINATION_OPEN_SOURCE_MESA`.

Software Rasterizers: List of driver IDs:
- SwiftShader: `VK_DRIVER_ID_GOOGLE_SWIFTSHADER`.
- LLVMPipe: `VK_DRIVER_ID_MESA_LLVMPIPE`.

Broadcom: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_BROADCOM_PROPRIETARY`.
- MESA: `VK_DRIVER_ID_MESA_V3DV`.

Verisilicon: List of driver IDs:
- Proprietary: `VK_DRIVER_ID_VERISILICON_PROPRIETARY`.

Other: List of driver IDs:
- SC: `VK_DRIVER_ID_VULKAN_SC_EMULATION_ON_VULKAN`.
- GGP (Stadia): `VK_DRIVER_ID_GGP_PROPRIETARY`.
- CoreAVI (Cars): `VK_DRIVER_ID_COREAVI_PROPRIETARY`.
- Juice (Remote GPU): `VK_DRIVER_ID_JUICE_PROPRIETARY`.
- Venus (Virtio GPU): `VK_DRIVER_ID_MESA_VENUS`.
- Dozen (Vulkan on DirectX): `VK_DRIVER_ID_MESA_DOZEN`.

## OpenGL

| Vendor/GPU | OS | Drivers | Version | Bug |
|---|---|---|---|---|
| All | Android | All | ? | In OpenGL, multi-threaded shader pre-compilation sometimes crashes. |
| All | All | Mesa | ? | https://github.com/KhronosGroup/glslang/pull/2646 |
