# Release Policy

While releases are usually made at the discretion of the team, we feel that establishing a clearer guideline on how those come to be will help expectations when it comes to features and fixes per version.

## Release candidates

Every full release is *preceded* by at least, 3 release candidates. The reasoning is that each week of the month, there will be a release candidate, with the "4th one" being the final full release.

The main expectation is that the release candidates bring both fixes and, sometimes, new features. But not guarantee a regression-free experience.

The criteria for choosing a date for a release candidate is at discretion, or "perceived necesity" at any given time.

## Full release

A full release means there are *no major* leftover regressions, importantly this means that a grand portion of regressions found between release candidates are swept out before declaring a full release. This doesn't mean a full release is regression-free; but we do a best-effort approach to reduce them for end-users.

The main expectation is that users can safely upgrade from a stable build to another, with no major regressions.

## Snapshot/rolling release

While we don't publish rolling releases, we are aware users may compile from source and/or provide binaries to master builds of the project.

This is mostly fine since we keep master very stable from major hiccups. However sometimes bugs do slip between tests or reviews - so users are advised to keep that in mind.

We advise that users also read git logs (`git log --oneline`) before recompiling to get a clearer picture of the changes given into the emulator.
