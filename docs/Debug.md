# Debug Guidelines

## Issue reports

When reporting issues or finding bugs, we often need backtraces, debug logs, or both in order to track down the issue.

### Graphics Debugging

If your bug is related to a graphical issue--e.g. mismatched colors, vertex explosions, flickering, etc.--then you are required to include graphical debugging logs in your issue reports.

Graphics Debugging is found in General -> Debug on desktop, and Advanced Settings -> Debug on Android. Android users are all set; however, desktop users may need to install the Vulkan Validation Layers:
- Windows: Install the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
- Linux, BSD, etc: Install `vulkan-validation-layers`, `vulkan-layers`, or similar from your package manager. It should be located in e.g. `/usr/lib64/libVkLayer_khronos_validation.so`

Once Graphics Debugging is enabled, run the problematic game again and continue. Note that the game may run extremely slow on weak hardware.

### Debug Logs

Debug logs can be found in General -> Debug -> Open Log Location on desktop, and Share Debug Logs on Android. This MUST be included in all bug reports, except for certain UI bugs--but we still highly recommend them even for UI bugs.

## Debugging (host code)

Ignoring SIGSEGV when debugging in host:

- **gdb**: `handle all nostop pass`.
- **lldb**: `pro hand -p true -s false -n false SIGSEGV`.

## Debugging (guest code)

### gdb

Run `./build/bin/eden-cli -c <path to your config file (see logs where you run eden normally to see where it is)> -d -g <path to game>`

Then hook up an aarch64-gdb (use `yay aarch64-gdb` or `sudo pkg in arch64-gdb` to install)
Then type `target remote localhost:1234` and type `c` (for continue) - and then if it crashes just do a `bt` (backtrace) and `layout asm`.

### gdb cheatsheet

- `mo <cmd>`: Monitor commands, `get info`, `get fastmem` and `get mappings` are available. Type `mo help` for more info.
- `detach`: Detach from remote (i.e restarting the emulator).
- `c`: Continue
- `p <expr>`: Print variable, `p/x <expr>` for hexadecimal.
- `r`: Run
- `bt`: Print backtrace
- `info threads`: Print all active threads
- `thread <number>`: Switch to the given thread (see `info threads`)
- `layout asm`: Display in assembly mode (TUI)
- `si`: Step assembly instruction
- `s` or `step`: Step over LINE OF CODE (not assembly)
- `display <expr>`: Display variable each step.
- `n`: Next (skips over call frame of a function)
- `frame <number>`: Switches to the given frame (from `bt`)
- `br <expr>`: Set breakpoint at `<expr>`.
- `delete`: Deletes all breakpoints.
- `catch throw`: Breakpoint at throw. Can also use `br __cxa_throw`
- `br _mesa_error`: Break on mesa errors (set environment variable `MESA_DEBUG=1` beforehand), see [MESA_DEBUG](https://mesa-docs.readthedocs.io/en/latest/debugging.html).

Expressions can be `variable_names` or `1234` (numbers) or `*var` (dereference of a pointer) or `*(1 + var)` (computed expression).

For more information type `info gdb` and read [the man page](https://man7.org/linux/man-pages/man1/gdb.1.html).

## Simple checklist for debugging black screens using Renderdoc

Renderdoc is a free, cross platform, multi-graphics API debugger. It is an invaluable tool for diagnosing issues with graphics applications, and includes support for Vulkan. Get it [here](https://renderdoc.org).

Before using renderdoc to diagnose issues, it is always good to make sure there are no validation errors. Any errors means the behavior of the application is undefined. That said, renderdoc can help debug validation errors if you do have them.

When debugging a black screen, there are many ways the application could have setup Vulkan wrong.
Here is a short checklist of items to look at to make sure are appropriate:
* Draw call counts are correct (aka not zero, or if rendering many triangles, not 3)
* Vertex buffers are bound
* vertex attributes are correct - Make sure the size & offset of each attribute matches what should it should be
* Any bound push constants and descriptors have the right data - including:
  * Matrices have correct values - double check the model, view, & projection matrices are uploaded correctly
* Pipeline state is correct
  * viewport range is correct - x,y are 0,0; width & height are screen dimensions, minDepth is 0, maxDepth is 1, NDCDepthRange is 0,1
  * Fill mode matches expected - usually solid
  * Culling mode makes sense - commonly back or none
  * The winding direction is correct - typically CCW (counter clockwise)
  * Scissor region is correct - usually same as viewport's x,y,width, &height
* Blend state is correct
* Depth state is correct - typically enabled with Function set to Less than or Equal
* Swapchain images are bound when rendering to the swapchain
* Image being rendered to is the same as the one being presented when rendering to the swapchain

Alternatively, a [RenderDoc Extension](https://github.com/baldurk/renderdoc-contrib/tree/main/baldurk/whereismydraw) ([Archive](https://web.archive.org/web/20250000000000*/https://github.com/baldurk/renderdoc-contrib/tree/main/baldurk/whereismydraw)) exists which automates doing a lot of these manual steps.
