# The suyu Emulator

[![Android Building](https://github.com/suyu-emu/SuyuEclipse/actions/workflows/android-build.yml/badge.svg)](https://github.com/suyu-emu/SuyuEclipse/actions/workflows/android-build.yml) [![Codespell](https://app.codacy.com/project/badge/Grade/5c5e01fb109f4ad38ff54111046da4bf)](https://app.codacy.com?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) [![Deploy Website to GitHub Pages](https://github.com/suyu-emu/website/actions/workflows/deploy.yml/badge.svg)](https://github.com/suyu-emu/website/actions/workflows/deploy.yml)

**Notes**: We do not support or condone piracy in any form. In order to use suyu, you'll need pre-decrypted games from your real Switch or Switch 2 system that you either can dump or have registered on your Nintendo Account which you have legally obtained and paid for. We do not intend to make money or profit from this project. We're in need of developers. Please join go to this Org's Github Discussions Page or DM a developer if you want to contribute as the project repository is private, although we will make a mirror on another platform so users have access to the source code, as is required under the GPL 3.0 License.  Our only websites are suyu.dev and suyu-emu.github.io so please be cautious when using other sites offering downloads for builds relating to suyu and other emulators. Bus_Error/Crimson Hawk and Co are no longer affiliated with the project.

<hr />

<h1 align="center">
  <br>
  <a href="https://suyu.dev"><img src="suyu__Logo-Pill.svg" alt="suyu" height="128"></a>
  <br>
  <b>suyu</b>
  <br>
</h1>

<h4 align="center"><b>suyu</b> was the most well known continuation of the world's most popular, open-source Nintendo Switch emulator, yuzu, but is now something more.
<br>
It is written in C++ with portability in mind, and we provide builds for Windows, Linux, Android and our own operating system: SuyuOS. MacOS is in development and iOS is being considered.

</h4>

<p align="center">
  <a href="github.com/suyu-emu/discussions">Discussions</a> |
  <a href="https://www.reddit.com/r/suyu/">Reddit</a> |
  <a href="#status">Status</a> |
  <a href="#development">Development</a> |
  <a href="#downloads">Downloads</a> |
  <a href="#building">Building</a> |
  <a href="#support">Support</a> |
  <a href="#license">License</a> |
</p>

## Hardware Requirements
[Click here to see the Hardware Requirements](https://web.archive.org/web/20250401081146/https://git.suyu.dev/suyu/suyu/wiki/Hardware-Requirements)

## Status


**Note**: We try to update this README whenever we can, but some links might be broken, and some information may be outdated or irrelevant.

## Development

This project is completely free and open source, and anyone can contribute to help improve suyu.

Most of the development happens on Github. For development discussion, please join us in our [Discussions Page](https://github.com/orgs/suyu-emu/discussions), you can also contact a developer or use our old [Chat](https://chat.suyu.dev) until the domain expires (you must already have a suyu Chat account to do so, as new accounts can no longer be created) and [Subreddit](reddit.com/r/suyu/).

If you want to contribute, please take a look at the [Contributor's Guide](https://web.archive.org/web/20241220084820/https://git.suyu.dev/suyu/suyu/wiki/Contributing) and [Developer Information](https://web.archive.org/web/20241217191056/https://git.suyu.dev/suyu/suyu/wiki/Developer-Information).
You can also contact any of the developers on Github to learn more about the current state of suyu.

## Downloads

*DUE TO THE THREATS OF A DMCA, DOWNLOADS TO PREVIOUS VERSIONS OF SUYU ARE RESTRICTED*

If any website or person is claiming to have a build for suyu, take that with a grain of salt and let us know.

For Multiplayer, we recommend using the "Yuzu Online" patch, install instructions can be found on Reddit and their Discord, although any online patches/hacks for Yuzu or any of it's forks should work fine with suyu.

## Building Guide

* __Windows__: [Windows Build](https://web.archive.org/web/20241220055052/https://git.suyu.dev/suyu/suyu/wiki/Building-for-Windows)
* __Linux__: [Linux Build](https://web.archive.org/web/20241220055052/https://git.suyu.dev/suyu/suyu/wiki/Building-for-Linux)
* __Android__: [Android Build](https://web.archive.org/web/20241220055052/https://git.suyu.dev/suyu/suyu/wiki/Building-for-Android)
* __MacOS__: [MacOS Build](https://web.archive.org/web/20241220055052/https://git.suyu.dev/suyu/suyu/wiki/Building-for-MacOS)
###### We currently do not provide builds for iOS, however if you would like, you could try the experimental [Sudachi Emulator](https://www.reddit.com/r/sudachiemulator/) and it's bigger parent project: [Folium](https://apps.apple.com/us/app/folium/id6498623389).


## Support

If you have any questions, don't hesitate to ask us in our [Chat](https://chat.suyu.dev) or [Subreddit](https://www.reddit.com/r/suyu/), but you'd have better luck making an issue here on github or contacting a developer. We don't bite!


## Agentic Workflows 📡

This repository includes experimental **agentic GitHub Actions** powered by the
[gh‑aw](https://github.github.com/gh-aw/) framework. These workflows can
automatically diagnose and fix common CI failures using a Copilot-based agent.

To make the agentic workflows work locally (for testing or development) you
will need to install the GitHub CLI extension:

```bash
# one‑time setup on your machine
gh extension install github/gh-aw
```

See the quick‑start guide at the official docs:
https://github.github.com/gh-aw/setup/quick-start/

The workflows themselves are located under `.github/workflows/` and include the
`auto-fix-workflows.lock.yml` file which is generated by the `gh-aw` toolchain.

---

## License

suyu is licensed under the free and open-source GPL-3.0-or-later license.

## Legal Notice

suyu is a GPLv3 program, which allows fully free redistribution of its source code and releases liability of it's authors for how this software is used as stated in Section 15 and 16.

The suyu Emulator program does not circumvent Nintendo's technological protection measures (TPMs) as the user is required to provide both the Nintendo Switch software & the encryption keys for these games, and the suyu Emulator uses a mode of the Advanced Encryption Standard (AES), an open encryption standard established by the US NIST, along with the encryption keys that the user themselves must lawfully acquire, to decrypt the software.
As the standard is public and available to use by all, it does not constitute as the Digital Market Copyright Act's (DMCA) definition of "circumventing a technological measure" as defined in Section 1201(a)(3).

The suyu Emulator also falls under the exemptions stated in Section 1201(f) of the DMCA as this software was created for the purposes of reverse engineering the Nintendo Switch software (known as Horizon OS) to create interoperability with Nintendo Switch games and software with the Windows, macOS, and GNU/Linux operating systems. 

Any aggressive DMCA claims or takedown notices against projects that explicitly disclaim piracy support, require user-provided keys, and limit functionality to interoperability (such as suyu) could constitute overreach or misuse of the DMCA. 

As derived from §512(f), if Nintendo (or an affiliated entity) knowingly materially misrepresents that a project like suyu is infringing (or circumvents TPMs) when it does not, especially if they fail to consider fair use, interoperability exemptions under §1201(f), or the fact that the emulator requires user-provided keys and does not itself contain proprietary Nintendo code, they can be made liable for any Damages against suyu.
