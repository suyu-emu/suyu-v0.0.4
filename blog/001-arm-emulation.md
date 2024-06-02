# Blog Entry 1: ARM CPU emulation

I've been studying the ARM recompiler used by Yuzu (Dynarmic) for quite a while now, and have learnt a lot on the way there.

This is going to be a rather technical post, but I hope it might help people like me understand what goes on under the hood.

## Native code execution

Let me start by giving a greatly simplified explaination of what happens on actual ARM hardware (like the Nintendo Switch) when an application is started:

1. Initial address space is set up
2. Stack pointer and finally program counter and are set
3. CPU goes into a loop where instructions are fetched, decoded, then executed

The first step includes loading the executable into memory and allocating stack space. The stack is basically a piece of memory that is used to store local variables and hold information about what address to return execution to when a routine ("function") finishes execution.
The second step here sets (at least) 2 registers, the stack pointer which tells the CPU where the stack is and the program counter which tells the CPU where to execute code from. It would point to a specific routine inside the executable.
The third step is to just let the CPU execute the application. In reality this step is much more complex but this explaination will do the job here.

Yuzu is able to run ARM64 code natively on ARM64 CPUs through "NCE", sidestepping the need for recompilation.

## Code recompilation

Now, what would this look like when emulating an ARM CPU with Dynarmic? In fact, the first 2 steps are exactly the same, except that the register values are stored in memory at first.
But then it gets interesting since the instructions at the program counter can't be "just executed". Instead, they have to go through a transformation from the architecture that is being emulated (like ARM64) to the architecture of the host CPU (like x64).

Let's take a look at this function:
```c
unsigned long add(unsigned long a, unsigned long b) {
    return a + b;
}
```

When compiled to ARM64, it looks like this:
```arm64
add	x0, x0, x1
ret
```

This could very easily be translated into x64 like this:
```x64
add rdi,rsi
mov rax,rdi
ret
```

And further optimized down like this:
```x64
lea rax,[rdi+rsi*1]
ret
```

### Register mapping

#### Static register map

As you can see, I have directly mapped ARM64 registers to x64 registers:

 - `x0` -> `rax`
 - `x1` -> `rdi`
 - `x2` -> `rsi`

But if you know the differences between ARM64 and x64 well enough, you've probably already notices that this won't always work!
The reason is that ARM64 has *way more* general purpose registers than x64! This unfortunately throws up a number of issues.

#### Dynamic register map

The closest we get to a good solution is to **dynamically allocate** the registers on the host machine.
This means that instead of creating a fixed mapping between registers, we do what C compilers do when there are more local variables in a function than registers available: Just store the least frequently used registers in memory instead and load them into a register (discarding *its* value into memory) as needed.

This sounds pretty cool, doesn't it? It's not that easy.
Dynarmic operates in **blocks of code**. That means that instead of recompiling the whole application right from the start, only recently executed code is recompiled and kept in memory. There are multiple reasons for this:

1. Startup time (recompiling everything can take a long time)
2. Memory usage (a limit for how much recompiled code to keep in memory can be defined)
3. Dynamic code generation (the guest application could overwrite and replace its own code at any time)

It is what makes Dynarmic a **dynamic** recompiler.

This comes with a pitfall in combination with dynamic register allocation though: Each block must be able to pick up execution from where the previous block stopped. This means that all registers must be stored in a predefined (fixed) location so that a block can be entered from different places.
For example, if block A expects the `x27` register to be in memory, but block B expects the same register to be in an actual register things would go horribly wrong.

This is typically solved by either storing all or a fixed set of registers at specific locations in memory and the rest of the registers in actual registers at the end of a block, and loading the registers back from memory into actual registers as needed at the start of another block.
This brings quite an unfortunate performance penalty but there is no real way around this.

More details on that later.

### Intermediate representation

So far, I've made it seem like the code transformation happens *directly* from the guest to the host architecture. This isn't actually true: There is something called an "intermediate representation" (short "IR") inbetween.
Pretty much all recompilers including Dynarmic have these 3 main components:

1. Frontend ("lifts" guest instructions into IR)
2. Middleend (runs transformations like optimization, more on that later)
3. Backend ("lowers" IR into host instructions)

This system has the major advantage that frontend and backend can be replaced individually and makes many different combinations possible.
Dynarmic has 2 frontends:

 - ARM32
 - ARM64

And 3 backend:

 - ARM64
 - RISCV64
 - x64

Specifically the RISCV64 backend does not implement all IR instructions so it only works with the ARM32 backend.
In theory there could be many more frontends and backends though, LLVM is great example for such a system but there are no frontends available that lift instructions to its IR, likely because its IR is too high level (too far away from machine instructions). Instead it has frontends for actual programming languages like C, C++ and Rust and many many backends, even some for GPUs.

Typically, one guest instruction turns into one or more IR instructions and a couple of IR instructions turn into one or more host instructions.

Let's take a look at the example from the beginning again:
```arm64
add	x0, x0, x1
ret
```

And convert it as a whole block into IR:
```c
%0 = load_general_register 0
%1 = load_general_register 1
%2 = integer_add %0 %1
store_general_register 0 %2
%3 = load_general_register 30
return branch_to(%3)
```

This is not what any actual IR looks like but you can see the entire process of loading the register values into IR registers, performing the addition and storing the result back into a register. Finally, the block ends by performing a branch to the address in the `x30` register (which is what the `ret` instruction actually does).

#### Dynamic register allocation

You might've noticed that instead of "overwriting" the IR register `0`, a new IR register `2` is created. This is because IR registers are actually constant values and only a concept needed to generate human readable IR code. In reality, the `integer_add` instruction arguments here simply "use" the results of the `load_general_register` instructions.

The backend itself is then responsible for making sure the value returned by the used instruction is still available when the instruction user needs it. This can be achieved by either:

1. Just keeping the value in an available host register if possible
2. Executing the instruction later, when it's actually needed
3. Moving the value into memory until it's used

And this is actually how dynamic register allocation works under the hood! This concept of an intermediate representation makes it incredibly easy.

#### Optimization

One important optimization is known as "constant folding". This optimization pass figures out values at compile time. Take this piece of IR for example:
```
%0 = integer_add 3 4
%1 = integer_sub %0 1
store_general_register 7 %1
```

Through constant folding, this IR can be optimized down to just:
```
store_general_register 7 6
```

Another optimization pass known as "dead code elimination" is able to remove code that can be removed without changing the outcome. For example this piece of IR:
```
%0 = integer_add 3 4
%1 = integer_sub %0 1
store_general_register 7 %0
```

Would be optimized into just:
```
%0 = integer_add 3 4
store_general_register 7 %0
```

By this pass.

There are a couple more optimization passes in Dynarmic that aren't as easy to explain, but I guess you get the point.
Optimization passes are usually chained together to generate the most optimal code.

### Other issues

What happens if the backend architecture does not support an instruction the frontend architure does *at all*?
One example for this is the **exclusive monitor**. It's a multithreading-related feature on ARM CPUs that can not be accurately replicated on x86_64 CPUs at all, and even the way Dynarmic implements it is extremely slow compared to an actual ARM CPU.

Sometimes, hard to emulate stuff like that can be emulated with less accurately to improve performance. In Torzu, such optimizations are enabled by setting the CPU accuracy to "unsafe". Some games may work fine with this, others may crash the entire emulator. Use at your own risk.

### Possible performance improvements in Dynarmic

#### Code caching

The idea is to cache generated IR on disk (and OS filesystem cache) to avoid reoptimizing the same blocks over and over again. This is not particularly useful for hot blocks (blocks with code that is executed very often in a loop) though since such blocks would most likely always stay in memory anyways.
It would improve startup time though and generally works very similarly to the disk shader cache.

#### Branch caching

One other idea I've had to avoid calling into the recompiler while the game is running works very similar to **profile guided optimization**. Basically, all branches that can't be resolved at compile time could fill kind of a "branch cache" so that next time that branch is recompiled the path to previous branch locations could be shorter.

#### More optimization passes

LLVM has a big collection of optimization passes that may be worthwhile checking out.

## Conclusion

While Dynarmic is already very fast there are still possible improvements to be made.
I'd love to get started with those right away, but the Dynarmic code base is quite complex and hard to learn and I don't want to dedicate *all* my time to Dynarmic.
Who knows, maybe the mentioned optimizations are completely worthless? I'll keep you updated whenever something interesting happens!

If you see any mistakes in this post, please feel free to create an issue. I have only proofread this post myself since I am a one-man team.
