# Signup

To prevent spam and reduce bandwidth usage, registration is closed, and will likely remain this way.

## Valid Reasons

First of all, you MUST have a valid reason to sign up for our Git. Valid reasons include (but are not limited to):

- I want to add feature XYZ...
- I want to improve the macOS version...
- I want to improve the Vulkan backend...
- I want to fix bug XYZ...
- I have experience in XYZ...
- I can provide insight on XYZ...

## Invalid Reasons

The following are not valid reasons to sign up:

- I want to contribute to Eden.
  * Be at least somewhat specific! We always welcome contributors and developers, but generic "I want to contribute" messages don't give us enough information.
- I want to support Eden.
  * If you wish to support us through development, be more specific; otherwise, to support us, check out our [donations page](https://eden-emu.dev/donations).
- I want to report issues.
  * Most of our issue tracking is handled on [GitHub](https://github.com/eden-emulator/Issue-Reports) for the time being. This is subject to change.
- I want to play/use Eden.
  * To download and use Eden, see our [Releases page](https://github.com/eden-emulator/Releases/releases)!
- I want to see the source code.
  * To see Eden's source code, go [here](https://git.eden-emu.dev/eden-emu/eden).

## Other Information

Requests that appear suspicious, automated, OR blank will generally be automatically filtered. In cases of suspicion, or any of the invalid reasons listed above, you may receive an email back asking for clarification.

You MUST use the following format:

```
Subject: [Eden Git] Registration Request
Username: <Your Desired Username>
Email: <Your Desired Email>
I wish to sign up because... <your reason here>
```

Email notifications are disabled for the time being, so you don't have to use a real email. If you wish to remain anonymous, either send a separate email asking for access to a shared anonymous account, *or* create a fake username and email. Do note that the email you sign up with is used to accredit commits on the web UI, and *must* match your configured GPG key.

## Patches

In general, PRs are the preferred method of tracking patches, as they allow us to go through our standard triage, CI, and testing process without having to deal with the minutiae of incremental patches. However, we also understand that many people prefer to use raw patches, and that's totally okay! While we currently don't have a mailing list, we do accept email patches. To do so:

1. Make your changes on a clean copy of the master branch
2. Commit your changes with a descriptive, well-formed message (see the [commit message docs](https://git.eden-emu.dev/eden-emu/eden/src/branch/master/docs/Development.md#pull-requests)), and a proper description thoroughly explaining your changes.
  * Note that we don't need to know all the individual details about your code. A description explaining the motivation and general implementation of your changes is enough, alongside caveats and any potential blockers.
3. Format your patch with `git format-patch -1 HEAD`.
4. Email us with the subject `[Eden] [PATCH] <brief patch description...>`, with a brief description of your patch, and the previously-formatted patch file as an attachment.
  * If you don't include the first two bracketed parts, your email may be lost!

The following emails are currently set up to receive and process patches:

- [eden@eden-emu.dev](mailto:eden@eden-emu.dev]
- [crueter@eden-emu.dev](mailto:eden@eden-emu.dev)

## Instructions

If you have read everything above and affirm that you will not abuse your access, click the summary below to get the email to send your request to.

<details>
<summary>I affirm that I have read ALL of the information above, and will not abuse my access to Eden, nor will I send unnecessary spam to the following email.</summary>

Email [crueter@crueter.xyz](mailto:crueter@crueter.xyz) with the format above.

Once your request is processed, you should receive a confirmation email from crueter with your password alongside a link to a repository containing instructions on SSH, etc. Note that you are required to change your password. If your request is rejected, you will receive a notice as such, asking for clarification if needed. If you do not receive a response in 48 hours, you may send another email.

> [!WARNING]
> Some email providers may place the response email in your spam/junk folder; notable offenders include Gmail and Outlook. *Always* ensure to check your Spam/Junk folder, until Google/Microsoft finally end their vendetta against the great evil of my `.xyz` domain.

</details>
