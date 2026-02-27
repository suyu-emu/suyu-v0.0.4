# AI Policy

Use at your peril.

AI is a *tool*, not a replacement or catch-all solution. It is generally okay at a few *very specific* use cases:

- Automation of tedious changes where you have already made the pattern clear and done the necessary groundwork.
- Conversion of code from one paradigm to another.

For everything else, AI is subpar at best, and actively harmful at worst. In general, you are **heavily** encouraged to not use AI at all.

## Why?

AI is notorious for hallucinating facts out of thin air and sometimes outright lying to users. Additionally, code written by LLMs is often needlessly verbose and horrifically inefficient (not to mention the rather ridiculous level of over-commenting). The end result is often one of three things:

- Completely nonfunctional code
- Code that works, but is extraordinarily verbose or not nearly as efficient as it can be
- Code that works well and is written well, but solves a different problem than was intended, or solves the same problem but in a completely incorrect way that will break other things horribly.

Human-written code will, without exception, always be of infinitely higher quality when properly researched and implemented by someone familiar with *both* the surrounding code and the programming language in use. LLMs may produce a "good enough" result, but this result is often subpar.

**All code is held under a STRICT STANDARD OF EXCELLENCE**. AI code is no different, and since it often produces subpar or outright terrible code, it will often fail to meet this excellence standard.

On a lesser-known note, LLM outputs often contain unicode symbols such as emojis or the arrow symbol. Please don't put Unicode symbols in your code. It messes with many an IDE, and the three people viewing your code on Lynx will be very unhappy.

**Learn to code**. It's worth it, we promise!

## Acceptable Use

As stated previously, AI is good in a few *very specific* cases. In these cases, it's usually fine to use AI, as long as you **explicitly provide notice that it was used**.

- Anything directly outside of the realm of the code written in your PR or patch is none of our business.
  - This primarily covers research.
  - However, we *still* strongly discourage this for the reasons mentioned above.
- Assistance with cleanups, and minor nitpicks/optimizations.
  - This is still discouraged, but it's okay to occasionally use LLMs to catch any minor mistakes you made in your code.
- Debugging
  - In general, LLMs are not good at debugging, but if you solve a bug you're facing with help from an AI, and said fix **works properly**, then that's fine.

## Unacceptable Use

Well, everything else. But here are a few examples:

- Commit messages
  - LLMs are absolutely horrible at this. They are needlessly verbose, almost never catch the actual intent of the commit, and will almost always hallucinate false information about said changes.
  - See the addendum for an example
- Solving problems
  - Slapping a few files and a "please fix bug XYZ" into an LLM is a recipe for disaster that will pretty much never work.
- Fully AI-generated code
  - This shouldn't need explaining. Do not do this under any circumstance, especially if you don't actually understand what's going on.
- Writing code based on pseudo-instructions
  - If you don't know how to write code, don't. If you've figured out the root cause (preferably without feeding random files into an LLM) and actively know what's going on, provide information to other developers or friends of yours who have knowledge of the language and/or the codebase.

## Addendum: Commit Messages

The patchset for pull request [#3422](https://git.eden-emu.dev/eden-emu/eden/pulls/3422) was fed into several LLMs to generate a commit message. All of them sucked, and not a single one caught on to what the commit actually did. For example:

```txt
profile_manager: Refactor user deletion to use index instead of UUID

This commit improves the user profile deletion flow in the following ways:

1. **UI layer (ConfigureProfileManager)**:
   - Replaces UUID-based deletion with index-based deletion
   - Passes the selected row index from the tree view to the confirmation dialog
   - Emits deleteUser(int index) signal from ConfigureProfileManagerDeleteDialog
   - Removes lambda capture and std::function callback in favor of direct signal/slot connection

2. **Delete dialog**:
   - Changes SetInfo() signature to accept index instead of accept callback
   - Stores index internally and emits it via deleteUser signal on acceptance

3. **Core ProfileManager**:
   - Refactors RemoveUser(UUID) → RemoveProfileAtIndex(index)
   - Removes duplicated stable_partition + save logic from RemoveUser()
   - Centralizes profile removal and save-file writing in RemoveProfileAtIndex()

4. **Minor follow-up**:
   - Adds profile_manager.ResetUserSaveFile() call in PopulateUserList() to ensure consistent state when refreshing the user list

These changes eliminate redundant code, reduce the risk of UUID → index mismatch bugs, and make the deletion path more straightforward and maintainable.

Fixes: potential inconsistency when user order changes between UI and backend
```

This is:

- Needlessly verbose (nobody cares about most of these details)
- Doesn't address the actual purpose of the PR (fixing a double-deletion bug in the profile manager)
- Has unicode arrows
- Uses overly corporate and, well, robotic language

As another example:

```txt
profile(manager/ui): switch to index-based deletion and unify removal logic

- Make ConfigureProfileManagerDeleteDialog emit a deleteUser(int) signal instead of taking a callback. Store the target index in the dialog (m_index) and emit it on acceptance.
- Change ConfigureProfileManager to pass an index to SetInfo and to accept an index in DeleteUser. Wire the dialog signal to the new DeleteUser slot.
- Update headers: add the signal and m_index, add TODO to move dialog to a .ui file, and update slot/signature declarations.
- Add ProfileManager::RemoveProfileAtIndex(std::size_t) and refactor RemoveUser(UUID) to call RemoveProfileAtIndex to avoid duplicated removal logic. Ensure the removal path marks saves as needed and writes the user save file.
- Ensure the profile list updates immediately after deletes by calling profile_manager.ResetUserSaveFile() when populating the user list (qlaunch fix).
- Misc: update SPDX copyright year and fix build breakages caused by the API changes.

This consolidates profile removal behavior, fixes potential race conditions in the profile dialog, and removes duplicated removal code.
```

This has all of the same problems as the other one. Needlessly verbose, doesn't address *what* it actually fixes ("consolidates profile removal behavior"... okay, why? What does it fix?), etc. It even has the bonus of totally hallucinating the addition of a method!

On a more "philosophical" note, LLMs tend to be geared towards *corporate language*, as that's what they're trained on. This is why AI-generated commit messages feel like "word salad", and typically pad out the commit message to make it *look* like a lot of things were changed (trust me, it's like that in the corporate world). They typically also drift towards unneeded buzzwords and useless implementation details.

**Don't use AI for commit messages**.
