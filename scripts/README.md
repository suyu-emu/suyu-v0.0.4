# Suyu Build Scripts

This directory contains utility scripts to help with building and maintaining the Suyu project.

## Vcpkg Build Fix Scripts

### `fix-vcpkg-build.ps1` (Recommended)
Comprehensive automated script that resolves vcpkg build issues:
- Cleans existing boost installations
- Clears vcpkg cache and buildtrees
- Reinstalls dependencies in correct order
- Provides troubleshooting guidance

**Usage:**
```powershell
.\scripts\fix-vcpkg-build.ps1
```

### `clean-boost.ps1`
Specialized script for cleaning boost components only:
- Removes all boost packages using boost-uninstall
- Useful for boost-specific issues

**Usage:**
```powershell
.\scripts\clean-boost.ps1
```

### `clean-boost.bat`
Batch file version of the boost cleanup script for environments without PowerShell.

**Usage:**
```cmd
scripts\clean-boost.bat
```

## Common Issues Resolved

### Missing vcpkg-cmake Configuration Files
**Error:** `include could not find requested file: vcpkg-cmake/vcpkg-port-config.cmake`
**Solution:** Run `fix-vcpkg-build.ps1` to install dependencies in correct order

### Boost Version Conflicts
**Error:** `no version database entry for boost-cobalt at 1.80.0`
**Solution:** Run `clean-boost.ps1` then reinstall with `vcpkg install`

### Build Cache Issues
**Error:** Various build failures after vcpkg updates
**Solution:** Use `fix-vcpkg-build.ps1` to clean cache and reinstall

## Other Scripts

### `sync_forgejo_issues.py`
Synchronizes issues between GitHub and Forgejo repositories.

### `test_connection.py` / `test_forgejo_connection.py`
Network connectivity testing utilities.

## For More Information

See `../VCPKG_BUILD_FIX.md` for detailed documentation of vcpkg build fixes and troubleshooting steps.

---

# Forgejo to GitHub Issue Sync

This directory also contains scripts and workflows to synchronize issues from a Forgejo repository to GitHub.

## Overview

The issue sync system addresses the problem of keeping GitHub issues in sync with a Forgejo repository.

**🔧 Recent Fix (Issue #29):**
- Fixed 403 Forbidden error by switching from web scraping to Forgejo REST API
- Corrected malformed GitHub API URL
- Added proper authentication handling for both public and private repositories
- Improved error handling and logging with emoji indicators
- Added comprehensive duplicate detection
- Implemented rate limiting to prevent API abuse

## Files

- `sync_forgejo_issues.py` - Standalone Python script for syncing issues
- `../.github/workflows/clone-issues.yml` - GitHub Actions workflow for automated syncing

## Features

- ✅ **API-based**: Uses Forgejo REST API instead of unreliable web scraping
- ✅ **Duplicate detection**: Prevents creating duplicate issues
- ✅ **Rate limiting**: Includes delays to avoid API rate limits
- ✅ **Error handling**: Robust error handling with informative messages
- ✅ **Authentication**: Supports both authenticated and public access
- ✅ **Attribution**: Clearly marks imported issues with source information
- ✅ **Label preservation**: Maintains original labels and adds import tag

## Setup

### GitHub Secrets

Configure the following secrets in your GitHub repository:

1. **Required:**
   - `GITHUB_TOKEN` - GitHub Personal Access Token with `repo` scope

2. **Optional:**
   - `FORGEJO_TOKEN` - Forgejo access token (only needed for private repositories)

### Environment Variables

The script uses these environment variables:

- `FORGEJO_URL` - Forgejo API endpoint (e.g., `https://git.suyu.dev/api/v1/repos/suyu/suyu/issues`)
- `FORGEJO_TOKEN` - Forgejo access token (optional for public repos)
- `GITHUB_REPOSITORY` - Target GitHub repository (e.g., `owner/repo`)
- `GITHUB_TOKEN` - GitHub access token

## Usage

### Automated Sync (GitHub Actions)

The workflow runs automatically:
- **Daily** at midnight UTC
- **Manual trigger** via GitHub Actions UI

To modify the schedule, edit `.github/workflows/clone-issues.yml` and change the cron expression.

### Manual Sync (Standalone Script)

1. **Install dependencies:**
   ```bash
   pip install requests
   ```

2. **Set environment variables:**
   ```bash
   export FORGEJO_URL="https://git.suyu.dev/api/v1/repos/suyu/suyu/issues"
   export GITHUB_REPOSITORY="suyu-emu/suyu"
   export GITHUB_TOKEN="your_github_token_here"
   # Optional: export FORGEJO_TOKEN="your_forgejo_token_here"
   ```

3. **Run the script:**
   ```bash
   python scripts/sync_forgejo_issues.py
   ```

## Troubleshooting

### Common Issues

1. **403 Forbidden Error**
   - The script will automatically retry without authentication for public repositories
   - For private repositories, ensure `FORGEJO_TOKEN` is set correctly

2. **Rate Limiting**
   - The script includes automatic delays and rate limiting protection
   - If you hit rate limits, the script will pause and continue

3. **Duplicate Issues**
   - Issues are prefixed with `[Forgejo]` to distinguish them
   - The script checks for both original and prefixed titles to avoid duplicates

## Configuration

### Customizing the Forgejo URL

Update the `FORGEJO_URL` in `.github/workflows/clone-issues.yml` to point to your Forgejo instance:

```yaml
env:
  FORGEJO_URL: https://your-forgejo-instance.com/api/v1/repos/owner/repo/issues
```

### Modifying Issue Format

To change how issues are formatted when imported, modify the `create_github_issue` function in the script:

- Change the title prefix (currently `[Forgejo]`)
- Modify the attribution message
- Add or remove labels

## API Endpoints

The script uses these API endpoints:

- **Forgejo**: `GET /api/v1/repos/{owner}/{repo}/issues`
- **GitHub**:
  - `GET /repos/{owner}/{repo}/issues` (fetch existing)
  - `POST /repos/{owner}/{repo}/issues` (create new)

## Contributing

When modifying the sync script:

1. Test changes with the standalone script first
2. Update both the workflow and standalone script
3. Update this documentation if needed
4. Test with a small number of issues before full deployment

## Issue Resolution

This implementation resolves the original issue #29 by:
- Using `https://git.suyu.dev/api/v1/repos/suyu/suyu/issues` instead of the web interface
- Handling 403 errors gracefully by falling back to unauthenticated requests for public repos
