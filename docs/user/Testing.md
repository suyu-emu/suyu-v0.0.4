# User Handbook - Testing

While this is mainly aimed for testers - normal users can benefit from these guidelines to make their life easier when trying to outline and/or report an issue.

## How to Test a PR Against the Based Master When Issues Arise

When you're testing a pull request (PR) and encounter unexpected behavior, it's important to determine whether the issue was introduced by the PR or if it already exists in the base code. To do this, compare the behavior against the based master branch.

Even before an issue occurs, it is best practice to keep the same settings and delete the shader cache. Using an already made shader cache can make the PR look like it is having a regression in some rare cases.

### What to Do When Something Seems Off
If you notice something odd during testing:
- Reproduce the issue using the based master branch.
- Observe whether the same behavior occurs.

### Two Possible Outcomes
- If the issue exists in the based master: This means the problem was already present before the PR. The PR most likely did not introduce the regression.
- If the issue does not exist in the based master: This suggests the PR most likely introduced the regression and needs further investigation.

### Report your findings
When you report your results:
- Clearly state whether the behavior was observed in the based master.
- Indicate whether the result is good (expected behavior) or bad (unexpected or broken behavior). Without mentioning if your post/report/log is good or bad it may confuse the Developer of the PR.
- Example:
```
1. "Tested on based master — issue not present. Bad result for PR, likely regression introduced."
2. "Tested on based master — issue already present. Good result for PR, not a regression."
```

This approach helps maintain clarity and accountability in the testing process and ensures regressions are caught and addressed efficiently. If the behavior seems normal for a certain game/feature then it may not be always required to check against the based master.

If a master build for the PR' based master does not exist. It will be helpful to just test past and future builds nearby. That would help with gathering more information about the problem.

**Always include [debugging info](../Debug.md) as needed**.