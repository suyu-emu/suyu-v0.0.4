# AI Policy

AI and LLM use is *strictly* prohibited within our codebase and surrounding community, including issues and comments. This includes using AI or LLMs to write docs/commit messages, debug issues, brainstorm ideas, research concepts, or search the codebase.

- [AI Policy](#ai-policy)
  - [Low-quality code](#low-quality-code)
  - [Licensing concerns](#licensing-concerns)
  - [Vibe-coding](#vibe-coding)
  - [Commit messages](#commit-messages)
  - [Miscellaneous concerns](#miscellaneous-concerns)
  - [Unacceptable Use Examples](#unacceptable-use-examples)
  - [Addendum: Commit Messages](#addendum-commit-messages)

## Low-quality code

AI is notorious for producing low-quality code; be it:

- nonfunctional,
- verbose/inefficient,
- breaking other parts of the codebase,
- or writing/architecting in a completely different style

All code, AI or not, is held under a **strict standard of excellence**. AI/LLM-generated code will fail this test 10 times out of 10.

## Licensing concerns

This is an area of ongoing litigation, and as such is still very iffy. For the time being, know that allowing AI to ingest the codebase may end up with its copyleft code regurgitated into incompatibly-licensed proprietary or permissive software. For you, this means to **not** feed code into LLMs.

AI models may have also ingested AGPLv3 code, which is license-incompatible with our codebase.

## Vibe-coding

Just don't. If you're not going to put the effort in to understand every line of code you wrote, neither will we, and your patch or pull request will be ignored.

## Commit messages

AI-generated commit messages are absolutely terrible, and your pull request or patch will immediately be rejected if you choose to do this. They are, quite simply, actively detrimental to our understanding of your changes, and if you're not willing to summarize the intent behind your changes, then we're not going to bother reading the code you wrote.

Write concise, simple, and descriptive commit messages that actually convey the proper intent behind what your change is trying to do.

See the [Addendum](#addendum-commit-messages) for an instance of how bad AI models are at commit messages.

## Miscellaneous concerns

- While many environmental concerns about AI are typically blown out of proportion, it *is* a legitimate issue, and should be taken into account.
- Many people have significant concerns over the ethics of AI usage due to inhumane and predatory behavior by large AI companies, particularly Anthropic and OpenAI. This can technically be avoided through the usage of local LLMs.
- LLMs have a tendency to add unicode characters (such as the arrow → and the em-dash — symbols) to their output, which can make viewing code or documents harder on command-line editors and viewers.
- Dedicated coding models--namely Claude--also like to add a lot of comments to overexplain every individual line of code it produces. This actually makes it *harder* to understand the code!

## Unacceptable Use Examples

Here are a few examples of unacceptable use:

- Solving problems
  - Slapping a few files and a "please fix bug XYZ" into an LLM is a recipe for disaster that will pretty much never work.
- Fully AI-generated code, aka "vibecoding"
- Writing code based on pseudo-instructions
  - If you don't know how to write code, don't. If you've figured out the root cause (preferably without feeding random files into an LLM) and actively know what's going on, provide information to other developers or friends of yours who have knowledge of the language and/or the codebase.

## Addendum: Commit Messages

The patchset for pull request [#3422](https://git.eden-emu.dev/eden-emu/eden/pulls/3422) was fed into several LLMs to generate a commit message. One LLM produced the following:

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
- Has unicode arrows (this is bad for command-line editors)
- Uses corporate and word-salad language

Another (code-oriented) LLM output the following:

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
