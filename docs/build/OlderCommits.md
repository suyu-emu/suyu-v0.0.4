# Building Older Commits

Bisecting and debugging older versions of Eden can be difficult, as many of our submodules have been deleted or removed. However, work has been done to make this process as simple as possible for users.

## Script

Copy the following script and store it in `fix.sh`:

```sh
#!/bin/sh -e

git -C externals/discord-rpc checkout 0d8b2d6a37c6e47d62b37caa14708bf747c883bb
git add externals/discord-rpc

git -C externals/dynarmic checkout 05b7ba50588d1004e23ef91f1bda8be234be68f4
git add externals/dynarmic

git -C externals/mbedtls checkout ce4f81f4a926a0e0dcadd0128e016baba416e8ea
git add externals/mbedtls

git -C externals/oboe checkout e4f06f2143eb0173bf4a2bd15aae5e8cc3179405
git add externals/oboe

git -C externals/sirit checkout b870b062998244231a4f08004d3b25151732c5c5
git add externals/sirit
```

Then, run `chmod +x fix.sh`

## Submodules

To check out submodules successfully, use this order of operations:

```sh
git submodule update --init --recursive --depth 1 --jobs 8 --progress
./fix.sh
git submodule update --init --recursive --depth 1 --jobs 8 --progress
```

And you should be good to go! If you check out a different commit that changes submodule commits, run the above command list again.
