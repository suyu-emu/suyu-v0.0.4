# User configuration

## Configuration directories

Eden will store configuration in the following directories:

- **Windows**: `%AppData%\Roaming`.
- **Android**: Data is stored internally.
- **Linux, macOS, FreeBSD, Solaris, OpenBSD**: `$XDG_DATA_HOME`, `$XDG_CACHE_HOME`, `$XDG_CONFIG_HOME`.

If a `user` directory is present in the current working directory, that will override all global configuration directories and the emulator will use that instead.
