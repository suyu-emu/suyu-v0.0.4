# Coding guidelines

These are mostly "suggestions", if you feel like your code is readable, comprehensible to others; and most importantly doesn't result in unreadable spaghetti you're fine to go.

But for new developers you may find that following these guidelines will make everything x10 easier.

## Naming conventions

Simply put, types/classes are named as `PascalCase`, same for methods and functions like `AddElement`. Variables are named `like_this_snake_case` and constants are `IN_SCREAMING_CASE`.

Except for Qt MOC where `functionName` is preferred.

Template typenames prefer short names like `T`, `I`, `U`, if a longer name is required either `Iterator` or `perform_action` are fine as well.

Macros must always be in `SCREAMING_CASE`. Do not use short letter macros as systems like Solaris will conflict with them; a good rule of thumb is >5 characters per macro - i.e `THIS_MACRO_IS_GOOD`, `AND_ALSO_THIS_ONE`.

Try not using hungarian notation, if you're able.

## Formatting

Do not put if/while/etc braces after lines:
```c++
// no dont do this
if (thing)
{
    some(); // ...
}

// do this
if (thing) {
    some(); // ...
}

// or this
if (thing)
    some(); // ...

// this is also ok
if (thing) some();
```

Brace rules are lax, if you can get the point across, do it:

```c++
// this is fine
do {
    if (thing) {
        return 0;
    }
} while (other);

// this is also ok --- albeit a bit more dense
do if (thing) return 0; while (other);

// ok as well
do {
    if (thing) return 0;
} while (other);
```

There is no 80-column limit but preferably be mindful of other developer's readability (like don't just put everything onto one line).

```c++
// someone is going to be mad due to this
SDL_AudioSpec obtained;
device_name.empty() ? device = SDL_OpenAudioDevice(nullptr, capture, &spec, &obtained, false) : device = SDL_OpenAudioDevice(device_name.c_str(), capture, &spec, &obtained, false);

// maybe consider this
SDL_AudioSpec obtained;
if (device_name.empty()) {
    device = SDL_OpenAudioDevice(nullptr, capture, &spec, &obtained, false);
} else {
    device = SDL_OpenAudioDevice(device_name.c_str(), capture, &spec, &obtained, false);
}

// or this is fine as well
SDL_AudioSpec obtained;
device = SDL_OpenAudioDevice(device_name.empty() ? nullptr : device_name.c_str(), capture, &spec, &obtained, false);
```
