# Troubleshooting Guide - Forgejo to GitHub Issue Sync

This guide helps resolve common issues with the Forgejo to GitHub issue synchronization.

## Common Issues

### 1. 403 Forbidden Error

**Error Message:**
```
Error fetching issues from Forgejo: 403 Client Error: Forbidden for url: https://git.suyu.dev/suyu/suyu/issues
```

**Causes & Solutions:**

1. **Using web scraping URL instead of API URL**
   - ❌ Wrong: `https://git.suyu.dev/suyu/suyu/issues`
   - ✅ Correct: `https://git.suyu.dev/api/v1/repos/suyu/suyu/issues`

2. **Repository requires authentication**
   - Set the `FORGEJO_TOKEN` environment variable
   - Generate a token in Forgejo: Settings → Applications → Generate New Token

3. **Repository is private or restricted**
   - Verify you have access to the repository
   - Check if the repository URL is correct

### 2. No Issues Found

**Error Message:**
```
No issues found on Forgejo or failed to fetch issues
```

**Solutions:**

1. **Check the repository has issues**
   - Visit the Forgejo repository directly
   - Verify there are open issues

2. **Verify API endpoint**
   - Test with: `curl https://git.suyu.dev/api/v1/repos/suyu/suyu/issues`
   - Should return JSON array of issues

3. **Check issue state filter**
   - The script only syncs "open" issues by default
   - Modify `SYNC_STATES` in config.py to include "closed" if needed

### 3. GitHub API Errors

**Error Message:**
```
Failed to create issue 'Title': 401 Unauthorized
```

**Solutions:**

1. **Invalid GitHub token**
   - Verify `GITHUB_TOKEN` is set correctly
   - Token needs `repo` scope for private repos, `public_repo` for public repos

2. **Rate limiting**
   - The script includes automatic rate limiting
   - If you hit limits, wait and try again

### 4. Duplicate Issues

**Issue:** Issues are being created multiple times

**Solutions:**

1. **Check duplicate detection logic**
   - Script checks for both original and prefixed titles
   - Verify existing issues are being fetched correctly

2. **Manual cleanup**
   - Close duplicate issues manually
   - The script will skip them on next run

## Testing Steps

1. **Test Forgejo connection:**
   ```bash
   python scripts/test_forgejo_connection.py
   ```

2. **Test with limited issues:**
   - Set `MAX_ISSUES_PER_RUN = 1` in config.py
   - Run the script to test with just one issue

3. **Check GitHub API access:**
   ```bash
   curl -H "Authorization: Bearer $GITHUB_TOKEN" \
        https://api.github.com/repos/owner/repo/issues
   ```

## Getting Help

If you're still experiencing issues:

1. Check the GitHub Actions logs for detailed error messages
2. Run the standalone script locally for easier debugging
3. Verify all environment variables are set correctly
4. Test API endpoints manually with curl

## Useful Commands

**Test Forgejo API:**
```bash
curl "https://git.suyu.dev/api/v1/repos/suyu/suyu/issues?per_page=1"
```

**Test GitHub API:**
```bash
curl -H "Authorization: Bearer $GITHUB_TOKEN" \
     "https://api.github.com/repos/$GITHUB_REPOSITORY/issues?per_page=1"
```

**Check environment variables:**
```bash
echo "FORGEJO_URL: $FORGEJO_URL"
echo "GITHUB_REPOSITORY: $GITHUB_REPOSITORY"
echo "GITHUB_TOKEN: ${GITHUB_TOKEN:0:10}..." # Only show first 10 chars
```
