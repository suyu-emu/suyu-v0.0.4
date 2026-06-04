# HOS Kernel

In brief, the HOS kernel is a microkernel, all services and programs run in userspace, the primary way to do communication between these is via `HIPC` (not covered here); otherwise most of the primitives reside in the forms of syscalls invoked via `svc #imm`. The kernel supports both 32-bit and 64-bit programs, and has the capacity to use 32, 36 and 39 bits of address space for spawned processes. Most of the networking stack is based off FreeBSD's network stack.

The emulator implements the majority of the syscalls pertaining to the HOS kernel itself. When we talk about the HOS Kernel (in the context of the emulator) we are strictly speaking about the mechanisms from which syscalls are handled (and it's subsequent side effects, such as the page table book-keeping). The emulator at it's current state is unable to load a custom low-level kernel and do supervisor-level emulation.

Most programs in NX eventually invoke an `svc`, which, depending on it's immediate value, will go on to be dispatched into one of the specific syscall handlers.

These can be seen in [svc.cpp](/src/core/hle/kernel/svc.cpp). All of these correspond to syscalls which userspace programs may perform.

In turn, these syscalls create the mechanisms that allows programs to use CMIF/TIPC as their primary IPC form to contact other services/processes running on the system, the details of which will not be covered here, but you can consult the relevant [SwitchBrew article: 'HIPC'](https://switchbrew.org/wiki/HIPC).

From the point of view of the programs, no special devices (such as PCIE, Realtek drivers, Bluetooth or USB) has to be handled by the emulator; this is because most of the fun occurs in specialized services such as `usb:u` or `pcie` services. Which aren't emulated (yet).

Due to the nature of syscalls, many of them interact with memory. The emulated kernel has an internal tree-like structure, borrowed from FreeBSD's intrusive red-black tree; this is used to track and find mappings added or removed. Thus most of the process space is emulated in this way.

The kernel keeps it's own separate pagetable, in a traditional sense, each process has it's own pagetable, this is true for HOS as well.

Every process keeps it's own tracking of the following structures:
- Name (13 characters)
- 64-bit ID
- A handle table
- Exclusive monitor
- Threads
- Held locks
- Thread local pages
- A page table for each process

The emulator willingly restricts itself to only use 4 threads (to emulate 4 cores), this is because most existing applications do not benefit greatly from the added core count, and in fact can be detrimental due to extra contention. This translates equitatively to about 4 `ArmInterface` slots for each process, these are then redirected to whatever is the last `pc` of the last thread running on the core is meant to be; proceed to run it, then when returning (due to halt or interruption), proceed to reschedule the thread.

The scheduler as-is isn't 100% faithful to the original (for example the original is cooperative and not preemptive), and has great timing variance (especially due to the fact the emulator can run in systems with wildly different timings).
