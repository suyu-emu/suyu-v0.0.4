# RenderDoc

Renderdoc is a free, cross platform, multi-graphics API debugger. It is an invaluable tool for diagnosing issues with graphics applications, and includes support for Vulkan. Get it at [renderdoc.org](https://renderdoc.org).

RenderDoc can capture Eden's Vulkan output when its Vulkan layer is loaded before Eden creates the Vulkan device. Before using renderdoc to diagnose issues, it is always good to make sure there are no validation errors. Any errors means the behavior of the application is undefined. That said, renderdoc can help debug validation errors if you do have them.

## Usage on Windows 

You can either use RenderDoc UI to launch eden, or you can make eden attach it internally:

On Windows PowerShell:
```powershell
$env:ENABLE_VULKAN_RENDERDOC_CAPTURE='1'
.\eden.exe
```
When RenderDoc is attached, Eden logs the default Windows capture folder:
```text
%LOCALAPPDATA%\Temp\RenderDoc
```

Press RenderDoc's capture hotkey, usually `F12`, to capture a frame. To stop using RenderDoc, close Eden and launch it again without `ENABLE_VULKAN_RENDERDOC_CAPTURE`.

## Eden Hotkey

Eden also has a separate `Toggle Renderdoc Capture` hotkey behind the debug setting `renderdoc_hotkey`.
That hotkey does not load or unload RenderDoc. It only toggles Eden's own manual capture through RenderDoc's API:

- first press: starts a capture
- second press: ends that capture

## Simple checklist for debugging black screens using Renderdoc

When debugging a black screen, there are many ways the application could have setup Vulkan wrong.
Here is a short checklist of items to look at to make sure are appropriate:

- Draw call counts are correct (aka not zero, or if rendering many triangles, not 3)
- Vertex buffers are bound
- vertex attributes are correct - Make sure the size & offset of each attribute matches what should it should be
- Any bound push constants and descriptors have the right data - including:
  - Matrices have correct values - double check the model, view, & projection matrices are uploaded correctly
- Pipeline state is correct
  - viewport range is correct - x,y are 0,0; width & height are screen dimensions, minDepth is 0, maxDepth is 1, NDCDepthRange is 0,1
  - Fill mode matches expected - usually solid
  - Culling mode makes sense - commonly back or none
  - The winding direction is correct - typically CCW (counter clockwise)
  - Scissor region is correct - usually same as viewport's x,y,width, &height
- Blend state is correct
- Depth state is correct - typically enabled with Function set to Less than or Equal
- Swapchain images are bound when rendering to the swapchain
- Image being rendered to is the same as the one being presented when rendering to the swapchain

Alternatively, a [RenderDoc Extension](https://github.com/baldurk/renderdoc-contrib/tree/main/baldurk/whereismydraw) ([Archive](https://web.archive.org/web/20250000000000*/https://github.com/baldurk/renderdoc-contrib/tree/main/baldurk/whereismydraw)) exists which automates doing a lot of these manual steps.
