# User Handbook - Command Line

There are two main applications, an SDL2 based app (`eden-cli`) and a Qt based app (`eden`); both accept command line arguments.

## eden
- `./eden <path>`: Running with a single argument and nothing else, will make the emulator look for the given file and load it, this behaviour is similar to `eden-cli`; allows dragging and dropping games into the application.
- `-g <path>`: Alternate way to specify what to load, overrides. However let it be noted that arguments that use `-` will be treated as options/ignored, if your game, for some reason, starts with `-`, in order to safely handle it you may need to specify it as an argument.
- `-f`: Use fullscreen.
- `-u <number>`: Select the index of the user to load as.
- `-qlaunch`: Launch QLaunch.
- `-setup`: Launch setup applet.

## eden-cli
- `--debug/-d`: Enter debug mode, allow gdb stub at port `1234`
- `--config/-c`: Specify alternate configuration file.
- `--fullscreen/-f`: Set fullscreen.
- `--help/-h`: Display help.
- `--game/-g`: Specify the game to run.
- `--multiplayer/-m`: Specify multiplayer options.
- `--program/-p`: Specify the program arguments to pass (optional).
- `--user/-u`: Specify the user index.
- `--version/-v`: Display version and quit.
