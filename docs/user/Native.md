# User Handbook - Native Application Development

Debugging on physical hardware can get tedious and time consuming. Users are empowered with the debugging capabilities of the emulator to ensure their applications run as-is on the system. To the greatest extent possible atleast.

## Debugging

**Standard key prefix**: Allows to redirect the key manager to a file other than `prod.keys` (for example `other` would redirect to `other.keys`). This is useful for testing multiple keysets. Default is `prod`.

**Changing serial**: Very basic way to set debug values for the serial (and battery number). Developers do not need to write the full serial as only the first digits (excluding the last) will be accoutned for. Region settings will affect the generated serial. The serial corresponds to a non-OLED/Lite console.
