# Cross compiling

General guide for cross compiling.

## Debian ARM64

A painless guide for cross compilation (or to test NCE) from a x86_64 system without polluting your main.

- Install QEMU: `sudo pkg install qemu`
- Download Debian 13: `wget https://cdimage.debian.org/debian-cd/current/arm64/iso-cd/debian-13.0.0-arm64-netinst.iso`
- Create a system disk: `qemu-img create -f qcow2 debian-13-arm64-ci.qcow2 30G`
- Run the VM: `qemu-system-aarch64 -M virt -m 2G -cpu max -bios /usr/local/share/qemu/edk2-aarch64-code.fd -drive if=none,file=debian-13.0.0-arm64-netinst.iso,format=raw,id=cdrom -device scsi-cd,drive=cdrom -drive if=none,file=debian-13-arm64-ci.qcow2,id=hd0,format=qcow2 -device virtio-blk-device,drive=hd0 -device virtio-gpu-pci -device usb-ehci -device usb-kbd -device intel-hda -device hda-output -nic user,model=virtio-net-pci`

## Gentoo

Gentoo's cross-compilation setup is relatively easy, provided you're already familiar with portage. A [cross toolchain file](../CMakeModules/toolchains/GentooCross.cmake) is provided. Throughout this section, replace `aarch64` with whatever target architecture you desire.

### Crossdev

First, emerge crossdev via `sudo emerge -a sys-devel/crossdev`.

Now, set up the environment depending on the target architecture; e.g.

```sh
sudo crossdev aarch64
```

### QEMU

If you don't have a host Gentoo system of your target architecture, you should install a QEMU user setup for testing. To do so, enable the relevant USE flags for `app-emulation/qemu`:

```txt
app-emulation/qemu static-user qemu_user_targets_aarch64
```

To use cross-emerged shared libraries, you will also need to tell qemu where the sysroot is. You can do this with an alias:

```sh
alias qemu-aarch64="qemu-aarch64 -L /usr/aarch64-unknown-linux-gnu"
```

### Dependencies

Dependencies are the same [as normal Gentoo](./Deps.md#Commands); simply replace the `emerge` command with `emerge-<target>-unknown-linux-gnu` (e.g. `emerge-aarch64-unknown-linux-gnu`). However, there are a few caveats:

#### Enabling GURU

Since Crossdev sysroots are effectively isolated from the system w.r.t Portage, you must manually enable GURU in your sysroot. Run the following as root:

```sh
mkdir -p /usr/aarch64-unknown-linux-gnu/etc/portage/repos.conf
cat << EOF > /usr/aarch64-unknown-linux-gnu/etc/portage/repos.conf/guru.conf
[guru]
location = /var/db/repos/guru
auto-sync = no
priority = 1
EOF
```

#### Package Errata

Crossdev is not perfect, and you may face some challenges with package that are not properly keyworded or have issues on specific architectures. These behaviors are, unfortunately, not well documented, and certain build systems such as Meson--and certain troublesome packages like GTK--are generally unfriendly towards cross-compilation.

Thus, it may be desirable to emerge a minimal set of dependencies and allow Eden's build system to handle the rest for you. At a minimum, you *only* need standard system libraries (Crossdev does this for you) and Qt:

```sh
sudo emerge-aarch64-unknown-linux-gnu dev-qt/qtbase:6 dev-qt/qtcharts:6
```

From here, CPMUtil will take care of everything else. For extra insurance, you may want to set `-DCPMUTIL_FORCE_BUNDLED=ON` in your configure command.

### Building

From here, building is relatively standard. The [cross toolchain file](../CMakeModules/toolchains/GentooCross.cmake) contains a few additional configurations, but generally all you need to do is set `CROSS_TARGET` and `CMAKE_TOOLCHAIN_FILE`. Disabling OpenGL is strongly recommended as well.

```sh
cmake -S . -B build/aarch64 -DCMAKE_TOOLCHAIN_FILE=CMakeModules/toolchains/GentooCross.cmake -GNinja -DCROSS_TARGET=aarch64 -DENABLE_OPENGL=OFF
```

With that done, you can build as normal:

```sh
cmake --build build/aarch64
```

And finally, run the compiled executable with QEMU!

```sh
qemu-aarch64 build/aarch64/bin/eden
```
