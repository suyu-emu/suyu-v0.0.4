# Blog Entry 2: The puzzle pieces

I've been excited about this for a long time! Users typically think of applications as one big piece of software.
In almost all cases this is not true.

In this post, I'd like to break Yuzu down into pieces for you. If you take a look at Torzu's `src/` directory, you'll see that there are several subdirectories. These are the main components, which are compiled as separate libraries (and then linked together). If you take a look at the `externals/` directory, you'll see a large list of other projects that Yuzu depends on.

I will focus on the `core` library here, as it contains most of the interesting stuff.

## ARM code execution

The `core/arm' directory is one of the most important: it is responsible for executing ARM instructions. It originally contained only code for dynamic integration, but was extended last year to include Native Code Execution (NCE).
Interestingly, it doesn't have an ARM interpreter, which usually helps a lot when debugging JIT-related issues in other emulators. Although Dynarmic already has some debug-friendly options, it might be worthwhile to add an interpreter at some point.

## Crypto

Probably the most controversial part of Yuzu so far as it contains those "access-control circumvention measures". Not much to say about it, it is the core part of key derivation, ROM decryption, etc.

## Filesystem

There are file systems everywhere: Inside game ROMs, on the SD card, the internal flash, etc. `core/file_sys` is responsible for organizing them and their contents, as well as applying patches and overlays.

## Frontend

Despite the name, there is no actual frontend code here. It's just support code that can be used by different frontends.

There are currently 2 frontends that come with Yuzu:

 - `yuzu`: Simply being called Yuzu, this is what you're interacting with most of the time. It is written in Qt Widgets.
 - `yuzu-cmd`: An SDL2 based frontend useful for light debugging. There's rarely a reason to use this.

## High-level emulation

Finally! This is where things get interesting.

Processes on an operating system can't do anything useful on their own. Typically, they can only access a small, predefined piece of RAM, and that's it. This is where the operating system APIs come in. Whenever a user process wants to access a file on a file system, query user input, or do just about anything else, it must use the operating system API to ask the operating system to perform that action on behalf of the process.

On modern operating systems, all of these things happen inside the operating system kernel. These kernels are called "monolithic kernels", but the operating system on the Nintendo Switch ("Horizon") doesn't work this way; it's called a "microkernel". Basically, instead of having the kernel handle hardware access, there are dedicated user processes called "services" that have direct access to the hardware they are responsible for. All the kernel does is manage memory, schedule tasks, and pass API calls to these services.

This is what a process accessing a driver would look like on a monolithic kernel like Linux or Windows:

```
[Userspace ]    [Kernel            ]
User process -> Kernel API -> Driver
```

And this is what the same thing would look like on a microkernel like Horizon:

```
[Userspace-]    [Kernel------------------]    [Userspace------]
User process -> Kernel API -> Services API -> Service -> Driver
```

As you can see, the path is much longer and therefore less efficient. Also note that each switch between userspace and kernel is quite slow, and that this path also has to be traversed backwards after the requested action is completed. However, this also means that if there is an exploit in a service, the rest of the system remains inaccessible to the attacker, and bugs can't bring down the kernel (which would kill the entire system).

Yes, Nintendo really did prioritize good security so that the Switch wouldn't be so easily hacked over performance on already weak hardware. Thanks, Nintendo.

Anyway, since Yuzu is a high-level emulator, it has to implement the kernel API and all those services itself. This is done in the `core/hle` directory. If you'd like to learn more, check out these Switchbrew pages:

 - https://switchbrew.org/wiki/SVC: Kernel API
 - https://switchbrew.org/wiki/Services_API: Services API + Service list

## Internal network

This part of the code handles the lowest level of networking, as well as redirection to a multiplayer server (Did you know? Yuzu multiplayer servers work kind of like a VPN!). Nothing special or noteworthy here at all.

## Loader

This is where ROM files are loaded:

 - Mapping of executable code and data to memory
 - Filesystem setup

## Memory

Nope, no memory management here. Not sure why this directory is called "memory" at all, since all it contains is the code responsible for applying cheats. Yes, cheats end up only interacting with memory, but it should have just been called "cheat".

Of note is the cheat VM, which is a nice, tiny cheat interpreter. Pretty cool!

## Tools

Here you'll find some debugging tools, including a "memory freezer" that allows you to silently prevent the game from changing data at certain memory addresses.

Quoting from `freezer.h`:

> This class allows the user to prevent an application from writing new values to certain memory
> locations. This has a variety of uses when attempting to reverse a game.
> 
> One example could be a cheat to prevent Mario from taking damage in SMO. One could freeze the
> memory address that the game uses to store Mario's health so when he takes damage (and the game
> tries to write the new health value to memory), the value won't change.

And then there's also RenderDoc integration for graphics debugging.

## Conclusion

There are many more components I could have listed here, especially outside the `core' directory, but I feel these are the most important ones, and I don't want this article to go on forever.

Thank you for reading and making it to the end. I welcome you to create issues for any mistgakes you find here!
