# User Handbook - Architectures and Platforms

Notes and caveats for different architectures and platforms.

# Architectures

Eden is primarily designed to run on amd64 (x86_64--Intel/AMD 64-bit) and aarch64 (arm64--ARM 64-bit) CPUs. Each architecture tends to have their own quirks and fun stuff; this page serves as a reference for these quirks.

## amd64

AMD64, aka x86_64, is the most tested and supported architecture for desktop targets. Android is entirely unsupported.

### Caveats

AMD64 systems are almost always limited by the CPU. For example, a Zen 5/RX 6600 system will often hit max CPU usage before the GPU ever reaches 70% usage, with minimal exceptions (that tend to pop up only at >200fps). JIT is slow!

Computers on Linux will almost always run Eden strictly better than an equivalent machine on Windows. This is largely due to the way the Linux kernel handles memory management (and the lack of Microsoft spyware).

Intel Macs are believed to be supported, but no CI is provided for them. Performance will likely be awful on all but the highest-end iMacs and Pro-level Macs, and the MoltenVK requirement generally means Vulkan compatibility will suffer.

## aarch64

ARM64, aka aarch64, is the only supported architecture for Android, with limited experimental support available on Linux, Windows, and macOS.

### Caveats

NCE (Native Code Execution) is currently only available on Android and (experimentally) Linux. Support for macOS is in the works, but Windows is extremely unlikely to ever happen (if you want it--submit patches!). Generally, if NCE is available, you should pretty much always use it due to the massive performance hit JIT has.

When NCE is enabled, do note that the GPU will almost always be the limiting factor. This is especially the case for Android, as well as desktops that lack dedicated GPUs; Adreno, Mali, PowerVR, etc. GPUs are generally significantly weaker relative to their respective CPUs.

Windows/arm64 is *very* experimental and is unlikely to work at all. Support and testing is in the works.

## riscv64

RISC-V, aka riscv64, is sparsely tested, but preliminary tests from developers have reported at least partial support on Milk-V's Fedora/riscv64 Linux distribution. Performance, Vulkan support, compatibility, and build system caveats are largely unknown for the time being.

### Caveats

Windows/riscv64 doesn't exist, and may never (until corporate greed no longer consumes Microsoft).

Android/riscv64 is interesting. While support for it may be added if and when RISC-V phones/handhelds ever go mainstream, arm64 devices will always be preferred due to NCE.

Only Fedora/riscv64 has been tested, but in theory, every riscv64 distribution that has *at least* the standard build tools, Qt, FFmpeg, and SDL2 should work.

## Other

Other architectures, such as SPARC, MIPS, PowerPC, Loong, and all 32-bit architectures are completely unsupported, as there is no JIT backend or emitter thereof. If you want support for it--submit patches!

IA-64 (Itanium) support is completely unknown. Existing amd64 packages will not run on IA-64 (assuming you can even find a supported Windows/Linux distribution)

# Platforms

The vast majority of Eden's testing is done on Windows, Linux, and Android. However, first-class support is also provided for:

- HaikuOS
- FreeBSD
- OpenBSD
- NetBSD
- OpenIndiana (Solaris)
- macOS

## Linux

While all modern Linux distributions are supported (Fedora >40, Ubuntu >24.04, Debian >12, Arch, Gentoo, etc.), the vast majority of testing and development for Linux is on Arch and Gentoo. Most major build system changes are tested on Gentoo first and foremost, so if builds fail on any modern distribution no matter what you do, it's likely a bug and should be reported.

Intel and Nvidia GPU support is limited. AMD (RADV) drivers receive first-class testing and are known to provide the most stable Eden experience possible.

Wayland is not recommended. Testing has shown significantly worse performance on most Wayland compositors compared to X11, alongside mysterious bugs and compatibility errors. For now, set `QT_QPA_PLATFORM=xcb` when running Eden, or pass `-platform xcb` to the launch arguments.

## Windows

Windows 10 and 11 are supported. Support for Windows 8.x is unknown, and Windows 7 support is unlikely to ever be added.

In order to run Eden, you will probably need to install the [Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170).

Neither AMD nor Nvidia drivers work nearly as well as Linux's RADV drivers. Compatibility is still largely the same, but performance and some hard-to-run games may suffer compared to Linux.

## Android

A cooler is always recommended. Phone SoCs tend to get very hot, especially those manufactured with the Samsung process or those lacking in power.

Adreno 6xx and 7xx GPUs with Turnip drivers will always have the best compatibility. "Stock" (system) drivers will have better performance on Adreno, but compatibility will suffer. Better support for stock drivers (including Adreno 8xx) is in the works.

Android 16 is always recommended, as it brought major improvements to Vulkan requirements and compatibility, *plus* significant performance gains. Some users reported an over 50% performance gain on some Pixel phones after updating.

Mali, PowerVR, Xclipse, and other GPU vendors generally lack in performance and compatibility. Notably:
- No PowerVR GPUs *except* the DXT-48-1536 are known to work with Eden at all.
- No Xclipse GPUs *except* the very latest (e.g. Xclipse 950) are known to work with Eden at all.
- Mali has especially bad performance, though the Mali-G715 (Tensor G4) and Immortalis-G925 are known to generally run surprisingly well, especially on Android 16.
- The status of all other GPU vendors is unknown. As long as they support Vulkan, they theoretically can run Eden.
- Note that these GPUs generally don't play well with driver injection. If you choose to inject custom drivers via a rooted system (Panfrost, RADV, etc), you may see good results.

Qualcomm Snapdragon SoCs are generally the most well supported.
- Google Tensor chips have pretty terrible performance, but even the G1 has been proven to be able to run some games well on the Pixel 6 Pro.
  * The Tensor G4 is the best-supported at the time. How the G5 currently fares is unknown, but on paper, it should do about as well as a Snapdragon 8 Gen 2 with stock drivers.
- Samsung Exynos chips made before 2022 are not supported.
- MediaTek Dimensity chips are extremely weak and most before mid-2023 don't work at all.
  * This means that most budget phones won't work, as they tend to use old MediaTek SoCs.
  * Generally, if your phone doesn't cost *at least* as much as a Switch itself, it will not *emulate* the Switch very well.
- Snapdragon 865 and other old-ish SoCs may benefit from the Legacy build. These will reduce performance but *should* drastically improve compatibility.
- If you're not sure how powerful your SoC is, check [NanoReview](https://nanoreview.net/en/soc-compare) - e.g. [Tensor G5](https://archive.is/ylC4Z).
  * A good base to compare to is the Snapdragon 865--e.g. [Tensor vs SD865](https://archive.is/M1P58)
  * Some benchmarks may be misleading due to thermal throttling OR RAM requirements.
    - For example, a Pixel 6a (Tensor G1) performs about 1/3 as well as an 865 due to its lack of RAM and poor thermals.
  * Remember--always use a cooler if you can, and you MUST have *at least* 8GB of RAM!
- If you're not sure what SoC you have, check [GSMArena](https://www.gsmarena.com) - e.g. [Pixel 9 Pro](https://archive.ph/91VhA)

Custom ROMs are recommended, *as long as* you know what you're doing.
- For most devices, [LineageOS](https://lineageos.org/) is preferred.
- [CalyxOS](https://calyxos.org/) is available as well.
- For Google Pixel devices ONLY... and [soon another OEM](https://archive.ph/cPpMd)... [GrapheneOS](https://grapheneos.org/) is highly recommended.
  * As of October 5, 2025, the Pixel 10 line is unsupported, however, [it will be](https://archive.is/viAUl) in the very near future!
  * Keep checking the [FAQ page](https://grapheneos.org/faq#supported-devices) for news.
- Custom ROMs will likely be exclusively recommended in the future due to Google's upcoming [draconian](https://archive.is/hGIjZ), [anti-privacy, anti-user](https://archive.is/mc1CJ) verification requirements.

Eden is currently unavailable on F-Droid or the Play Store. Check back occasionally.

## macOS

macOS is relatively stable, with only the occasional crash and bug. Compatibility may suffer due to the MoltenVK layer, however.

Do note that building the GUI version with Qt versions higher than 6.7.3 will cause mysterious bugs, Vulkan errors, and crashes, alongside the cool feature of freezing the entire system UI randomly; we recommend you build with 6.7.3 (via aqtinstall) or earlier as the CI does.

## *BSD, Solaris

BSD and Solaris distributions tend to lag behind Linux in terms of Vulkan and other library compatibility. For example, OpenIndiana (Solaris) does not properly package Qt, meaning the recommended method of usage is to use `eden-cli` only for now. Solaris also generally works better with OpenGL.

AMD GPU support on these platforms is limited or nonexistent.

## HaikuOS

HaikuOS supports (see below) Vulkan 1.3 and has Mesa 24.0. Because OpenGL ES is used instead of the desktop flavour of OpenGL the OpenGL backend is actually worse than the Vulkan one in terms of stability and system support. OpenGL is highly not recommended due to it being: out of tree builds of Mesa and generally unstable ones at that. Users are advised to use Vulkan whenever possible.

- Additionally system drivers for NVIDIA and Intel iGPUs exist and provide a native Vulkan ICD with the `Xcb` interface as opposed to the native `BView`
- In order to obtain Vulkan 1.3 support with native `BView` support; Swiftshader can be compiled from source [see this thread](https://discuss.haiku-os.org/t/swiftshader-vulkan-software-renderer-on-haiku/11526/6).

## VMs

Eden "can" run in a VM, but only with the software renderer, *unless* you create a hardware-accelerated KVM with GPU passthrough. If you *really* want to do this and don't have a spare GPU lying around, RX 570 and 580 GPUs are extremely cheap on the black market and are powerful enough to run most commercial games at 60 FPS.

Some users and developers have had success using a pure OpenGL-accelerated KVM on Linux with a Windows VM, but this is ridiculously tedious to set up. You're probably better off dual-booting.