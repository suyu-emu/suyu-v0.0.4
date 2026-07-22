# Orphaned Profiles

A bug present in earlier versions of Eden and Yuzu caused some profiles to be read from the incorrect location if your NAND directory was set to anything other than the default. This bug was fixed in Eden v0.0.4-rc1, but it can be destructive if you're not careful.

## What are they?

Orphaned profiles refer to emulated user profiles that may or may not contain valid save data, but are not referenced by the internal profile map. This means the save data is effectively inaccessible, and should be fixed in order to access your save data.

## How do I fix it?

There are lots of different cases of varying complexity.

Remember to ALWAYS back up your saves!

### Simple Copy

Sometimes, a simple copying is all you need. For example, if the orphaned profile folder contains game saves, BUT the good profile is completely empty, then you can simply remove the empty folder and rename the orphaned profile to the same name as the good one.

### Combination

In more extreme cases, game saves can be strewn all throughout different profiles. In this case, you must look at each profile individually.

Typically, one folder will clearly have more recent/numerous save data, in which case you can remove all the other profile folders and follow the same procedure as the simple copy.

If multiple profile folders contain valid data, the recommended approach is to copy the contents of one folder into the other. There are likely to be file conflicts, the resolution of which is up to you.

An alternate method for dealing with multiple valid profiles is to go into System -> Profiles, and create a new profile. From there, you can copy the contents of each previously-orphaned profile into a new profile.

### Edge Cases

There are way too many edge cases to cover here, but in general, make backups! You can never go wrong if you always have a backup of your saves.