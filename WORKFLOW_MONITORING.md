# Workflow Monitoring & Auto-Run Guide

## How to Monitor Workflows

### Checking Build Status

When reviewing a pull request, you can monitor CI workflow status from:
- **Checks tab** in the pull request
- **Actions tab** in the repository
- Direct links to specific workflow runs

**CMake on multiple platforms** status indicators:
- ⏳ **IN PROGRESS** - Build is running
- ✅ **SUCCESS** - Build completed successfully
- ❌ **FAILURE** - Build failed

**Other Workflows**:
- ✅ autofix.ci - SUCCESS
- ✅ codespell - SUCCESS
- ❌ suyu-ci - FAILURE (pre-existing REUSE licensing issue, not build-related)
- ❌ suyu verify - FAILURE (pre-existing Docker permission issue, not build-related)

## Workflow Triggers

### Workflows with `workflow_dispatch` (Manual Trigger Enabled)
The following workflows can be manually triggered:
- `.github/workflows/ci.yml` (suyu-ci)
- `.github/workflows/verify.yml` (suyu verify)
- `.github/workflows/autofix.yml` (autofix.ci)
- `.github/workflows/codespell.yml` (codespell)
- `.github/workflows/cmake-multi-platform.yml` (CMake builds)

### Auto-Run Limitations

**For Pull Requests**:
- Workflows require manual approval from repository maintainers (GitHub security feature)
- Status shows as `action_required` until approved
- This prevents untrusted code from consuming CI resources

**For Direct Commits to `dev` Branch**:
- Workflows run automatically without approval
- All `workflow_dispatch` workflows can be triggered via GitHub UI or API

## Monitoring Commands

### Check Workflow Status
```bash
# List recent workflow runs
gh run list --repo suyu-emu/suyu --limit 5

# Check specific run
gh run view 22035013707 --repo suyu-emu/suyu

# Watch a run in real-time
gh run watch 22035013707 --repo suyu-emu/suyu
```

### Trigger Manual Workflow
```bash
# Trigger a workflow_dispatch workflow
gh workflow run "CMake on multiple platforms" --repo suyu-emu/suyu --ref dev
```

## Build Monitoring - vcpkg Overlay Fix

### What's Being Tested
The current build (5ce476e) is testing a critical fix:
- **Problem**: boost-coroutine fails to build on MSVC 2024+
- **Solution**: vcpkg overlay stub in `vcpkg-overlays/boost-coroutine/`
- **Expected**: Step 6 should bypass boost-coroutine and complete successfully

### Key Build Steps
1. ✅ Set up job
2. ✅ Create vcpkg binary cache
3. 🔄 Checkout repository (current - ~6 minutes)
4. ⏳ Cache vcpkg packages
5. ⏳ Clean phantom submodule references
6. ⏳ **Install vcpkg and dependencies** ⚠️ **CRITICAL STEP**
7. ⏳ Diagnostic: vcpkg info
8. ⏳ Configure CMake
9. ⏳ Build
10. ⏳ Test
11. ⏳ Upload artifacts

## Project Board Migration Task

### Source
https://web.archive.org/web/20250615055256/https://git.suyu.dev/suyu/suyu/projects/11

### Target
https://github.com/orgs/suyu-emu/projects/1/

### Migration Steps
1. Access old Forgejo project board (requires authentication)
2. Export items, columns, and metadata
3. Recreate structure in GitHub Projects
4. Transfer issues/tasks to new board
5. Verify completeness

**Note**: Old board URL currently inaccessible via automated tools - requires manual action by repository maintainer.

## Success Criteria

✅ **Build Fix Validated When**:
- Step 6 (vcpkg installation) completes without boost-coroutine errors
- CMake configuration succeeds
- Build completes and generates artifacts
- All steps complete with green checkmarks

## Next Actions

1. **Monitor**: Continue watching CMake build progress
2. **Verify**: Confirm overlay fix works when Step 6 completes
3. **Document**: Update PR with successful build results
4. **Migrate**: Complete project board migration (requires maintainer access)
