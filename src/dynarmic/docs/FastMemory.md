# Fast memory (Fastmem)

The main way of accessing memory in JITed programs is via an invoked function, say "Read()" and "Write()". On our translator, such functions usually take a sizable amounts of code space (push + call + pop). Trash the i-cache (due to an indirect call) and overall make code emission more bloated.

The solution? Delegate invalid accesses to a dedicated arena, similar to a swap. The main idea behind such mechanism is to allow the OS to transmit page faults from invalid accesses into the JIT translator directly, bypassing address space calls, while this sacrifices i-cache coherency, it allows for smaller code-size and "faster" throguhput.

Many kernels however, do not support fast signal dispatching (Solaris, OpenBSD, FreeBSD). Only Linux and Windows support relatively "fast" signal dispatching. Hence this feature is better suited for them only.

![Host to guest translation](./HostToGuest.svg)

![Fastmem translation](./Fastmem.svg)

In x86_64 for example, when a page fault occurs, the CPU will transmit via control registers and the stack (see `IRETQ`) the appropriate arguments for a page fault handler, the OS then will transform that into something that can be sent into userspace.

Most modern OSes implement kernel-page-table-isolation, which means a set of system calls will invoke a context switch (not often used syscalls), whereas others are handled by the same process address space (the smaller kernel portion, often used syscalls) without needing a context switch. This effect can be negated on systems with PCID (up to 4096 unique IDs).

Signal dispatching takes a performance hit from reloading `%cr3` - but Linux does something more clever to avoid reloads:  VDSO will take care of the entire thing in the same address space. Making dispatching as costly as an indirect call - without the hazards of increased code size.

The main downside from this is the constant i-cache trashing and pipeline hazards introduced by the VDSO signal handlers. However on most benchmarks fastmem does perform faster than without (Linux only). This also abuses the fact of continous address space emulation by using an arena - which can then be potentially transparently mapped into a hugepage, reducing TLB walk times.
