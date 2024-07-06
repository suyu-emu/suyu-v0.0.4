<!--
SPDX-FileCopyrightText: 2018 yuzu Emulator Project - 2024 torzu Emulator Project
SPDX-License-Identifier: GPL-2.0-or-later
-->

<h1 align="center">
  <br>
  <a href="http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu"><img src="https://raw.githubusercontent.com/litucks/torzu/master/dist/yuzu.bmp" alt="torzu" width="200"></a>
  <br>
  <b>torzu</b>
  <br>
</h1>

<h4 align="center"><b>torzu</b> is a fork of yuzu, an open-source Nintendo Switch emulator.
<br>
It is written in C++ with portability in mind and runs on Linux and Windows
</h4>

## !!! PENDING GITHUB MIRROR TAKEDOWN !!!

I have received a takedown notice from GitHub!

```
Hi litucks,

GitHub Trust & Safety is contacting you because we've received a DMCA takedown notice regarding the following content:

https://github.com/litucks/torzu

You can see the DMCA takedown notice that we received here, which includes the complainant's requested changes to your repository:

https://enterprise.githubsupport.com/attachments/token/IRkbmN2s4Pu7U7fwBNZ2hXYoP/?name=2024-07-05-nintendo.rtf

We want to make clear that it is never our desire or goal to take down open source projects, but instead to help developers address DMCA concerns with their projects. To that end, you have some options:

1. If you choose to make changes in response to this takedown notice, you'll have 1 business day to make the requested changes. Once you've made the changes, please reply to this message and let us know. If you don't tell us that you've made changes within the next 1 business day, we'll need to disable the entire repository according to our GitHub DMCA Takedown Policy - https://docs.github.com/en/github/site-policy/dmca-takedown-policy

If you need to remove specific content from your repository, simply making the repository private or deleting it via a commit won't resolve the alleged infringement. Instead, you must follow these instructions to remove the content from your repository's history, even if you don't think it's sensitive - https://docs.github.com/en/free-pro-team@latest/github/authenticating-to-github/removing-sensitive-data-from-a-repository

2. If you believe your content on GitHub was mistakenly identified by a DMCA takedown request, you have the right to contest the takedown by submitting a counter notice, as described in our DMCA Takedown Policy - https://docs.github.com/en/github/site-policy/dmca-takedown-policy

We want to help ensure you can make informed decisions when responding to this takedown request. To help, we have partnered with leading legal providers who can help you evaluate the takedown request and determine how to respond at no cost to you. If you are interested in receiving legal support through this program (https://github.blog/2021-07-27-github-developer-rights-fellowship-stanford-law-school/), reply to this message to let us know that.

It is important that you reply to this message within 1 business day regardless of which option you choose. If you do not, the repository will be disabled.

Please let us know if you have any questions or need any help removing the content.

Regards,
GitHub Trust & Safety
```

The next mirror repository is going to be at: https://codeberg.org/litucks/torzu (repo privatized until then) unless anything comes in the way. Otherwise you can use the button in the "About" menu of the emulator to get to the current mirror repository or simply check out the [main repository](http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu).

## Limited public development

I feel like working publicly on this has taken away the fun. You may not understand, but it's quite stressful to have the public eye on a project.
Keep in mind, this is just a hobby project. I feel like I always have to keep updating so I don't disappoint you. It's just not a good situation for a hobby project to be in.
Turns out: running an open source project takes a lot more time than I have.
And then stupid and unnecessary issues like Windows Defender flagging the emulator as malware ruin the rest. I am grateful for all your bug reports, help and support, but all that has distracted me from taking the project into the direction I would've liked.

Today I've accidentally locked myself out of the Tor site out of pure stupidity and one of the things I didn't make a backup of for was the key required to get the same Tor site set back up.

For these reasons, I have decided to limit public development.
It is not all over though. My plan is to:
 - continue the blog
 - move the blog into a seperate repository
 - keep this repository updated just enough so it stays compilable on Linux and Windows
 - keep the externals updated
 - push some bigger updates from my private upstream whenever I feel like (no promises)
 - NOT publish releases. If someone feels like publishing builds they should feel free to do that
 - NOT offer support in any way
   - feel free to open issues on the main repository though if you feel like an issue REALLY needs my attention
 - accept pull requests as long as they are of reasonable quality

Again, thanks to everyone who has supported my efforts so far in any way (even by creating bug reports), I really appreciate it.

## Compatibility

The emulator is capable of running most commercial games at full speed, provided you meet the [necessary hardware requirements](http://web.archive.org/web/20240130133811/https://yuzu-emu.org/help/quickstart/#hardware-requirements).

It runs most Nintendo Switch games released until the date of the Yuzu takedown.

## Goals

The first and foremost goal is long-term maintenance. Even if I stop commiting new features I will always do my best to keep the emulator functional and third party dependencies updated. This also means most of the changes made will eventually be bug fixes.
Essentially, the main goal is that you can still use this emulator on modern systems in 20 years.
It is very important to me that this project is going to be a good base to fork once grass has grown over the whole legal dilemma and people are willing to do real work on this emulator non-anonymously.

A secondary goal is the improvement of usability on low-end systems. This includes both improving the performance of the emulator as well as making games more playable below 100% speed whenever possible (the sync CPU to render speed limit option already helps with that in few cases).

Android support is low priority but would be a nice bonus.

## Development

Most of the development happens on [Dark Git](http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/). It's also where [our central repository](http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu) is hosted.

To clone this git repository, you can use these commands given tor is installed and running:

    git -c http.proxy=socks5h://127.0.0.1:9050 clone http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu.git
    cd torzu
    git submodule update --init --depth 1 --recursive

Alternatively, you can clone from the [GitHub mirror repository](https://github.com/litucks/torzu):

    git clone https://github.com/litucks/torzu.git --depth 1 --recursive

Note that above repository may be taken down any time. Do not rely on its existence in production. In case the GitHub mirror goes down, another mirror will be most likely be set up on Bitbucket.

This project incorporates several commits from the [Suyu](https://suyu.dev) and [Sudachi](https://github.com/sudachi-emu/sudachi) forks (but cleaned up due to the typically mediocre code/commit quality from both projects) as well as changes listed in **Changes**.

## Building

* __Linux__: [Linux Build](http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu/wiki/Building-for-Linux)
* __Windows__: [Windows Build](http://vub63vv26q6v27xzv2dtcd25xumubshogm67yrpaz2rculqxs7jlfqad.onion/torzu-emu/torzu/wiki/Building-for-Windows)

## License

yuzu is licensed under the GPLv3 (or any later version). Refer to the [LICENSE.txt](https://github.com/yuzu-emu/yuzu/blob/master/LICENSE.txt) file.
