# User Handbook - Graphics

## Visual Enhancements

### Anti-aliasing

Enhancements aimed at removing jagged lines/sharp edges and/or masking artifacts.

- **No AA**: Default, provides no anti-aliasing.
- **FXAA**: Fast Anti-Aliasing, an implementation as described on [this blog post](https://web.archive.org/web/20110831051323/http://timothylottes.blogspot.com/2011/03/nvidia-fxaa.html). Generally fast but with some innocuos artifacts.
- **SMAA**: Subpixel Morphological Anti-Aliasing, an implementation as described on [this article](https://web.archive.org/web/20250000000000*/https://www.iryoku.com/smaa/).

### Filters

Various graphical filters exist - each of them aimed at a specific target/image quality preset.

- **Nearest**: Provides no filtering - useful for debugging.
  - **Pros**: Fast, works in any hardware.
  - **Cons**: Less image quality.
- **Bilinear**: Provides the hardware default filtering of the Tegra X1.
  - **Pros**: Fast with acceptable image quality.
- **Bicubic**: Provides a bicubic interpolation using a Catmull-Rom (or hardware-accelerated) implementation.
  - **Pros**: Better image quality with more rounded edges.
- **Zero-Tangent, B-Spline, Mitchell**: Provides bicubic interpolation using the respective matrix weights. They're normally not hardware accelerated unless the device supports the `VK_QCOM_filter_cubic_weights` extension. The matrix weights are those matching [the specification itself](https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#VkSamplerCubicWeightsCreateInfoQCOM).
  - **Pros/Cons**: Each of them is a variation of the Bicubic interpolation model with different weights, they offer different methods to fix some artifacts present in Catmull-Rom.
- **Spline-1**: Bicubic interpolation (similar to Mitchell) but with a faster texel fetch method. Generally less blurry than bicubic.
  - **Pros**: Faster than bicubic even without hardware accelerated bicubic.
- **Gaussian**: Whole-area blur, an applied gaussian blur is done to the entire frame.
  - **Pros**: Less edge artifacts.
  - **Cons**: Slow and sometimes blurry.
- **Lanczos**: An implementation using `a = 3` (49 texel fetches). Provides sharper edges but blurrier artifacts.
  - **Pros**: Less edge artifacts and less blurry than gaussian.
  - **Cons**: Slow.
- **ScaleForce**: Experimental texture upscale method, see [ScaleFish](https://github.com/BreadFish64/ScaleFish).
  - **Pros**: Relatively fast.
- **FSR**: Uses AMD FidelityFX Super Resolution to enhance image quality.
  - **Pros**: Great for upscaling, and offers sharper visual quality.
  - **Cons**: Somewhat slow, and may be offputtingly sharp.
- **Area**: Area interpolation (high kernel count).
  - **Pros**: Best for downscaling (internal resolution > display resolution).
  - **Cons**: Costly and slow.
- **MMPX**: Nearest-neighbour filter aimed at providing higher pixel-art quality.
  - **Pros**: Offers decent pixel-art upscaling.
  - **Cons**: Only works for pixel-art.

### External

While stock shaders offer a basic subset of options for most users, programs such as [ReShade](https://github.com/crosire/reshade) offer a more flexible experience. In addition to that users can also seek out modifications (mods) for enhancing visual experience (60 FPS mods, HDR, etc).

## Driver specifics

### Mesa environment variable hacks

The software requires a certain version of Vulkan and a certain version of OpenGL to work - otherwise it will refuse to load, this can be easily bypassed by setting an environment variable: `MESA_GL_VERSION_OVERRIDE=4.6 MESA_GLSL_VERSION_OVERRIDE=460` (OpenGL) and `MESA_VK_VERSION_OVERRIDE=1.3` (Vulkan), for more information see [Environment variables for Mesa](https://web.archive.org/web/20250000000000*/https://docs.mesa3d.org/envvars.html).

### NVIDIA OpenGL environment variables

Unstable multithreaded optimisations are offered by the stock proprietary NVIDIA driver on X11 platforms. Setting `__GL_THREADED_OPTIMIZATIONS` to `1` would enable such optimisations. This mainly benefits the OpenGL backend. For more information see [Environment Variables for X11 NVIDIA](https://web.archive.org/web/20250115162518/https://download.nvidia.com/XFree86/Linux-x86_64/435.17/README/openglenvvariables.html).

### swrast/LLVMpipe crashes under high load

The OpenGL backend would invoke behaviour that would result in swarst/LLVMpipe writing an invalid SSA IR (on old versions of Mesa), and then proceeding to crash. The solution is using a script found in [tools/llvmpipe-run.sh](../../tools/llvmpipe-run.sh).
