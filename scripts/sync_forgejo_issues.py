#!/usr/bin/env python3
"""
Sync issues from Forgejo to GitHub

This script fetches issues from a Forgejo repository and creates corresponding
issues in a GitHub repository, avoiding duplicates.

Usage:
    python sync_forgejo_issues.py

Environment Variables:
    FORGEJO_URL: Forgejo API URL (e.g., https://git.suyu.dev/api/v1/repos/suyu/suyu/issues)
    FORGEJO_TOKEN: Forgejo access token (optional for public repos)
    GITHUB_REPOSITORY: Target GitHub repository (e.g., owner/repo)
    GITHUB_TOKEN: GitHub access token
"""

import json
import os
import sys
import time
from urllib.parse import urlparse

import requests
# Configuration constants
DEFAULT_FORGEJO_URL = "https://git.suyu.dev/api/v1/repos/suyu/suyu/issues"
ISSUE_TITLE_PREFIX = "[Forgejo]"
IMPORT_LABEL = "imported-from-forgejo"
RATE_LIMIT_DELAY = 1
RATE_LIMIT_BATCH_SIZE = 10
RATE_LIMIT_BATCH_DELAY = 5
FORGEJO_PAGE_SIZE = 50
GITHUB_PAGE_SIZE = 100
SYNC_STATES = ["open"]
MAX_ISSUES_PER_RUN = 0

# Attribution message template
ATTRIBUTION_TEMPLATE = """**Imported from Forgejo**: {source_url}

---

{original_body}"""


def get_existing_github_issues(github_repo, github_headers):
    """Get all existing GitHub issues to avoid duplicates"""
    existing_issues = {}
    page = 1

    print("📋 Fetching existing GitHub issues...")

    while True:
        url = f"https://api.github.com/repos/{github_repo}/issues"
        params = {"state": "all", "per_page": GITHUB_PAGE_SIZE, "page": page}

        try:
            response = requests.get(url, headers=github_headers, params=params)
            response.raise_for_status()
            issues = response.json()

            if not issues:
                break

            for issue in issues:
                # Use title as key for duplicate detection
                existing_issues[issue["title"]] = issue["number"]

            page += 1

        except requests.exceptions.RequestException as e:
            print(f"❌ Error fetching existing GitHub issues: {e}")
            break

    return existing_issues


def fetch_forgejo_issues(forgejo_api_url, forgejo_headers):
    """Fetch issues from Forgejo API"""
    all_issues = []
    page = 1

    print("🔍 Fetching issues from Forgejo...")

    while True:
        params = {"state": ",".join(SYNC_STATES), "per_page": FORGEJO_PAGE_SIZE, "page": page}

        try:
            response = requests.get(forgejo_api_url, headers=forgejo_headers, params=params)

            if response.status_code == 403:
                print("⚠️  403 Forbidden: Trying without authentication...")
                # Try without authentication for public repos
                response = requests.get(forgejo_api_url, params=params)

            response.raise_for_status()
            issues = response.json()

            if not issues:
                break

            all_issues.extend(issues)
            page += 1

        except requests.exceptions.RequestException as e:
            print(f"❌ Error fetching issues from Forgejo: {e}")
            if page == 1:  # If first page fails, exit
                return []
            break

    return all_issues


def create_github_issue(title, body, labels, github_repo, github_headers, forgejo_api_url):
    """Create a new issue on GitHub"""
    create_issue_url = f"https://api.github.com/repos/{github_repo}/issues"

    # Prepare issue body with source attribution
    source_url = forgejo_api_url.replace('/api/v1/repos/', '/').replace('/issues', '')
    attributed_body = ATTRIBUTION_TEMPLATE.format(source_url=source_url, original_body=body)

    new_issue = {
        "title": f"{ISSUE_TITLE_PREFIX} {title}",
        "body": attributed_body,
    }

    if labels:
        new_issue["labels"] = labels + [IMPORT_LABEL]
    else:
        new_issue["labels"] = [IMPORT_LABEL]

    try:
        create_response = requests.post(create_issue_url, json=new_issue, headers=github_headers)

        if create_response.status_code == 201:
            issue_data = create_response.json()
            print(f"✅ Issue '{title}' created successfully (#{issue_data['number']})")
            return True
        else:
            print(f"❌ Failed to create issue '{title}': {create_response.status_code}")
            print(f"Response: {create_response.text}")
            return False

    except requests.exceptions.RequestException as e:
        print(f"❌ Error creating issue '{title}' on GitHub: {e}")
        return False


def main():
    # Environment variables
    forgejo_api_url = os.getenv("FORGEJO_URL", DEFAULT_FORGEJO_URL)
    forgejo_token = os.getenv("FORGEJO_TOKEN")
    github_repo = os.getenv("GITHUB_REPOSITORY")
    github_token = os.getenv("GITHUB_TOKEN")

    # Validate required environment variables
    if not forgejo_api_url:
        print("❌ FORGEJO_URL environment variable is required")
        sys.exit(1)
    if not github_repo:
        print("❌ GITHUB_REPOSITORY environment variable is required")
        sys.exit(1)
    if not github_token:
        print("❌ GITHUB_TOKEN environment variable is required")
        sys.exit(1)

    # Headers for API requests
    forgejo_headers = {}
    if forgejo_token:
        forgejo_headers["Authorization"] = f"token {forgejo_token}"

    github_headers = {"Authorization": f"Bearer {github_token}"}

    print("🔄 Starting Forgejo to GitHub issue sync...")

    # Get existing GitHub issues
    existing_issues = get_existing_github_issues(github_repo, github_headers)
    print(f"Found {len(existing_issues)} existing GitHub issues")

    # Fetch Forgejo issues
    forgejo_issues = fetch_forgejo_issues(forgejo_api_url, forgejo_headers)

    if not forgejo_issues:
        print("❌ No issues found on Forgejo or failed to fetch issues")
        sys.exit(1)

    print(f"Found {len(forgejo_issues)} issues on Forgejo")

    # Process each Forgejo issue
    created_count = 0
    skipped_count = 0

    for issue in forgejo_issues:
        title = issue.get("title", "Untitled Issue")
        body = issue.get("body", "")
        labels = [label["name"] for label in issue.get("labels", [])]

        # Check for duplicates (with and without [Forgejo] prefix)
        prefixed_title = f"{ISSUE_TITLE_PREFIX} {title}"

        if title in existing_issues or prefixed_title in existing_issues:
            print(f"⏭️  Issue '{title}' already exists, skipping...")
            skipped_count += 1
            continue

        # Create the issue
        if create_github_issue(title, body, labels, github_repo, github_headers, forgejo_api_url):
            created_count += 1
            # Add small delay to avoid rate limiting
            time.sleep(RATE_LIMIT_DELAY)

        # Rate limiting protection
        if created_count > 0 and created_count % RATE_LIMIT_BATCH_SIZE == 0:
            print("⏸️  Pausing for rate limit protection...")
            time.sleep(RATE_LIMIT_BATCH_DELAY)

        # Check if we've hit the maximum issues per run
        if MAX_ISSUES_PER_RUN > 0 and created_count >= MAX_ISSUES_PER_RUN:
            print(f"⏹️  Reached maximum issues per run ({MAX_ISSUES_PER_RUN}), stopping...")
            break

    print(f"\n📊 Sync completed:")
    print(f"   ✅ Created: {created_count} issues")
    print(f"   ⏭️  Skipped: {skipped_count} issues")
    print(f"   📋 Total processed: {len(forgejo_issues)} issues")


if __name__ == "__main__":
    main()
