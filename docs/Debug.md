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

- **gdb**: `handle SIGSEGV nostop pass`.
- **lldb**: `pro hand -p true -s false -n false SIGSEGV`.

## Debugging (guest code)

### gdb

You must have GDB installed for aarch64 to debug the target. Install it through your package manager, e.g.:

- On Arch:
  - `sudo pacman -Syu aarch64-linux-gnu-gdb`
- On Gentoo:
  - `sudo emerge --ask crossdev`
  - `sudo crossdev -t aarch64-unknown-linux-gnu --ex-gdb`

Run `./build/bin/eden-cli -c <path to your config file (see logs where you run eden normally to see where it is)> -d -g <path to game>`, or `Enable GDB Stub` at General > Debug, then hook up an aarch64-gdb:

- `target remote localhost:6543`

Type `c` (for continue) and then if it crashes just do a `bt` (backtrace) and `layout asm`

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

# RenderDoc (Graphic Debugging Tool)

Guidelines for graphical debugging using RenderDoc: **[RenderDoc usage](./RenderDoc.md)**