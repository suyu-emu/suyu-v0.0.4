# Coding guidelines

These are mostly "suggestions", if you feel like your code is readable, comprehensible to others; and most importantly doesn't result in unreadable spaghetti you're fine to go.

But for new developers you may find that following these guidelines will make everything x10 easier.

## Naming conventions

Simply put, types/classes are named as `PascalCase`, same for methods and functions like `AddElement`. Variables are named `like_this_snake_case` and constants are `IN_SCREAMING_CASE`.

Except for Qt MOC where `functionName` is preferred.

Template typenames prefer short names like `T`, `I`, `U`, if a longer name is required either `Iterator` or `perform_action` are fine as well. Do not use names like `SS` as systems like solaris define it for registers, in general do not use any of the following for short names:
- `SS`, `DS`, `GS`, `FS`: Segment registers, defined by Solaris `<ucontext.h>`
- `EAX`, `EBX`, `ECX`, `EDX`, `ESI`, `EDI`, `ESP`, `EBP`, `EIP`: Registers, defined by Solaris.
- `X`: Defined by some utility headers, avoid.
- `_`: Defined by gettext, avoid.
- `N`, `M`, `S`: Preferably don't use this for types, use it for numeric constants.
- `TR`: Used by some weird `<ucontext.h>` whom define the Task Register as a logical register to provide to the user... (Need to remember which OS in specific).

Macros must always be in `SCREAMING_CASE`. Do not use short letter macros as systems like Solaris will conflict with them; a good rule of thumb is >5 characters per macro - i.e `THIS_MACRO_IS_GOOD`, `AND_ALSO_THIS_ONE`.

Try not using hungarian notation, if you're able.

## Formatting

Formatting is extremelly lax, the general rule of thumb is: Don't add new lines just to increase line count. The less lines we have to look at, the better. This means also packing densely your code while not making it a clusterfuck. Strike a balance of "this is a short and comprehensible piece of code" and "my eyes are actually happy to see this!". Don't just drop the entire thing in a single line and call it "dense code", that's just spaghetti posing as code. In general, be mindful of what other devs need to look at.

Do not put if/while/etc braces after lines:
```c++
// no dont do this
// this is more lines of code for no good reason (why braces need their separate lines?)
// and those take space in someone's screen, cumulatively
if (thing)
{ //<--
    some(); // ...
} //<-- 2 lines of code for basically "opening" and "closing" an statment

// do this
if (thing) { //<-- [...] and with your brain you can deduce it's this piece of code
             //    that's being closed
    some(); // ...
} //<-- only one line, and it's clearer since you know its closing something [...]

// or this, albeit the extra line isn't needed (at your discretion of course)
if (thing)
    some(); // ...

// this is also ok, keeps things in one line and makes it extremely clear
if (thing) some();

// NOT ok, don't be "clever" and use the comma operator to stash a bunch of statments
// in a single line, doing this will definitely ruin someone's day - just do the thing below
// vvv
if (thing) some(), thing(), a2(a1(), y1(), j1()), do_complex_shit(wa(), wo(), ploo());
// ... and in general don't use the comma operator for "multiple statments", EXCEPT if you think
// that it makes the code more readable (the situation may be rare however)

// Wow so much clearer! Now I can actually see what each statment is meant to do!
if (thing) {
    some();
    thing();
    a2(a1(), y1(), j1());
    do_complex_shit(wa(), wo(), ploo());
}
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

A note about operators: Use them sparingly, yes, the language is lax on them, but some usages can be... tripping to say the least.
```c++
a, b, c; //<-- NOT OK multiple statments with comma operator is definitely a recipe for disaster
return c ? a : b; //<-- OK ternaries at end of return statments are clear and fine
return a, b; //<-- NOT OK return will take value of `b` but also evaluate `a`, just use a separate statment
void f(int a[]) //<-- OK? if you intend to use the pointer as an array, otherwise just mark it as *
```

And about templates, use them sparingly, don't just do meta-templating for the sake of it, do it when you actually need it. This isn't a competition to see who can make the most complicated and robust meta-templating system. Just use what works, and preferably stick to the standard libary instead of reinventing the wheel. Additionally:

```c++
// NOT OK This will create (T * N * C * P) versions of the same function. DO. NOT. DO. THIS.
template<typename T, size_t N, size_t C, size_t P> inline void what() const noexcept;

// OK use parameters like a normal person, don't be afraid to use them :)
template<typename T> inline void what(size_t n, size_t c, size_t p) const noexcept;
```
