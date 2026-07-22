# Development guidelines

## License Headers

All commits must have proper license header accreditation.

You can easily add all necessary license headers by running:

```sh
git fetch origin master:master
.ci/license-header.sh -u -c
git push
```

Alternatively, you may omit `-c` and do an amend commit:

```sh
git fetch origin master:master
.ci/license-header.sh
git commit --amend -a --no-edit
```

If the work is licensed/vendored from other people or projects, you may omit the license headers. Additionally, if you wish to retain authorship over a piece of code, you may attribute it to yourself; however, the code may be changed at any given point and brought under the attribution of Eden.

For more information on the license header script, run `.ci/license-header.sh -h`.

## Pull Requests

Pull requests are only to be merged by core developers when properly tested and discussions conclude on Discord or other communication channels. Labels are recommended but not required. However, all PRs MUST be namespaced and optionally typed:

```txt
[cmake] refactor: CPM over submodules
[desktop] feat: implement firmware install from ZIP
[hle] stub fw20 functions
[core] test: raise maximum CPU cores to 6
[cmake, core] Unbreak FreeBSD Building Process
```

- The level of namespacing is generally left to the committer's choice.
- However, we never recommend going more than two levels *except* in `hle`, in which case you may go as many as four levels depending on the specificity of your changes.
- Ocassionally, up to two additional namespaces may be provided for more clarity.
  - Changes that affect the entire project (sans CMake changes) should be namespaced as `meta`.
- Maintainers are permitted to change namespaces at will.
- Commits within PRs are not required to be namespaced, but it is highly recommended.

## Adding new settings

When adding new settings, use `tr("Setting:")` if the setting is meant to be a field, otherwise use `tr("Setting")` if the setting is meant to be a Yes/No or checkmark type of setting, see [this short style guide](https://learn.microsoft.com/en-us/style-guide/punctuation/colons#in-ui).

- The majority of software must work with the default option selected for such setting. Unless the setting significantly degrades performance.
- Debug settings must never be turned on by default.
- Provide reasonable bounds (for example, a setting controlling the amount of VRAM should never be 0).
- The description of the setting must be short and concise, if the setting "does a lot of things" consider splitting the setting into multiple if possible.
- Try to avoid excessive/redundant explainations "recommended for most users and games" can just be "(recommended)".
- Try to not write "slow/fast" options unless it clearly degrades/increases performance for a given case, as most options may modify behaviour that result in different metrics accross different systems. If for example the option is an "accuracy" option, writing "High" is sufficient to imply "Slow". No need to write "High (Slow)".

Some examples:

- "[...] negatively affecting image quality", "[...] degrading image quality": Same wording but with less filler.
- "[...] this may cause some glitches or crashes in some games", "[...] this may cause soft-crashes": Crashes implies there may be glitches (as crashes are technically a form of a fatal glitch). The entire sentence is structured as "may cause [...] on some games", which is redundant, because "may cause [...] in games" has the same semantic meaning ("may" is a chance that it will occur on "some" given set).
- "FIFO Relaxed is similar to FIFO [...]", "FIFO Relaxed [...]": The name already implies similarity.
- "[...] but may also reduce performance in some cases", "[...] but may degrade performance": Again, "some cases" and "may" implies there is a probability.
- "[...] it can [...] in some cases", "[...] it can [...]": Implied probability.

Before adding a new setting, consider:

- Does the piece of code that the setting pertains to, make a significant difference if it's on/off?
- Can it be auto-detected?

# IDE setup

## VSCode

Copy this to `.vscode/settings.json`, get CMake tools and it should be ready to build:

```json
{
    "editor.tabSize": 4,
    "files.watcherExclude": {
        "**/target": true
    },
    "files.associations": {
      "*.inc": "cpp"
    },
    "git.enableCommitSigning": true,
    "git.alwaysSignOff": true
}
```

You may additionally need the `Qt Extension Pack` extension if building Qt.

# Build speedup

If you have an HDD, use ramdisk (build in RAM), approximatedly you need 4GB for a full build with debug symbols:

```sh
mkdir /tmp/ramdisk
chmod 777 /tmp/ramdisk
# about 8GB needed
mount -t tmpfs -o size=4G myramdisk /tmp/ramdisk
cmake -B /tmp/ramdisk
cmake --build /tmp/ramdisk -- -j32
umount /tmp/ramdisk
```

# Assets and large files

A general rule of thumb, before uploading files:

- PNG files: Use [optipng](https://web.archive.org/web/20240325055059/https://optipng.sourceforge.net/).
- SVG files: Use [svgo](https://github.com/svg/svgo).

May not be used but worth mentioning nonethless:

- OGG files: Use [OptiVorbis](https://github.com/OptiVorbis/OptiVorbis).
- Video files: Use ffmpeg, preferably re-encode as AV1.
