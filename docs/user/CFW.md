# User Handbook - Custom Firmware (CFW)

At the moment of writing, we do not support CFW such as Atmosphere, due to:

- Lacking the required LLE emulation capabilities to properly emulate the full firmware.
- Lack of implementation on some of the key internals.
- Nobody has bothered to do it (PRs always welcome!)

We do however, maintain HLE compatibility with the former mentioned CFW, applications that require Atmosphere to run will run fine in the emulator without any adjustments.

If they don't run - then that's a bug!

## Atmosphere

Fusee Galee, the bootloader and other low-level mechanisms are not emulated at the moment.

Having OFW is recommended, but may not be required (untested).

Extract the contents of Atmosphere into `sdmc`. Then to launch simply use `-hlaunch` instead (orthogonal to `-qlaunch`).
