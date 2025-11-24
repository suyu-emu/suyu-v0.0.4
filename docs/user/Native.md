# User Handbook - Native Application Development

Debugging on physical hardware can get tedious and time consuming. Users are empowered with the debugging capabilities of the emulator to ensure their applications run as-is on the system. To the greatest extent possible atleast.

## Debugging

**Standard key prefix**: Allows to redirect the key manager to a file other than `prod.keys` (for example `other` would redirect to `other.keys`). This is useful for testing multiple keysets. Default is `prod`.

**Changing serial**: Very basic way to set debug values for the serial (and battery number). Developers do not need to write the full serial as it will be writen in-place (that is, it will be filled with the default serial and then overwrite the serial from the beginning).
- Battery serial: `YUZU0EMULATOR14022024`
- Board serial: `YUZ10000000001`
If the user were to set their board serial as `ABC`, then it will be written in-place and the resulting serial would be `ABC10000000001`. There are no underlying checks to ensure correctness of serials other than a hard limit of 16-characters for both.
