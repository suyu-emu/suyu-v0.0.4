ai is fine


## Imported Claude Cowork project instructions

/autopilot still not getting through to title screens of games, also ensure the copilot model used is gpt 5.4, or is on auto mode, preferably the former. loading screens dont automatically end

ALSO, VERIFY EVERYTHING FROM THE OG PROMPT:.

fix game export/recompilation to actually recompile

the loading screen sometimes has half of itself covered by menu

some buttons in the bottom bar of the emulator window arent clickable or dont make sense to me


hello, games still do not finish/sucsessfully loadng, im not sure how much decryption logic has been restored, test both smash and tomodachi, and also make sure all improvements from eden are migrated to this codebases, based off of its commits, prs and also its release changelog: https://git.eden-emu.dev/eden-emu/eden/releases (eden is a fork of suyu).  generally fix more stuff like online rooms and nintendo account stuff (should be alt more automated like a 1-click sign in that lets suyu show ur digital library which u still need to add roms to seperately). 

test vigourously and fix, dont stop until games get past loading and into title screens, make lgos more detailed and expanded. expand mcp capabilities to test games and features more


smash did load after you did MCP or other commands (not on its own tho), but the intro cutscene was slow, everything else was decent speed, I couldnt join a public room though, because i connected, it showed me an empty chat but the game didnt load into that multiplayer session. tomodachi doesnt load at all yet, apparenly ryujnix is the only emu that can run it perfectly, reference it: https://git.ryujinx.app/projects/Ryubing.


apparently latest version of eden supports all the latest stuff (firmware, keyes etc) etc as well, so migrate/port and reference more code etc


dont stop until every feature in the emulator is tested and works completely.


additionally, make sure it builds to all platforms needed, and that it can also build to a libretro core.


search the web for more code and information/context

also remove all mention of suyuEclipse in manuals and other bits, replace with just suyu.

test all features and stubs.

also there is something libretro-related already there, but that is wip support for lbretro cores as part of suyu, u can implement if u like.

also the icons/artwork when in launcher/menu are compressed/low res for some reason.

also, loading screen is unrefined looking, progress bar is mid, the text underneath doesnt really log what the emulator is doing, just says "starting emulator session", theres no nice border around the game artwork, and that artwork needs rounded corners, the other shapes in the scene dont match.

make sure everything ive asked is done, make todos and do each one within this session, remind yourself
