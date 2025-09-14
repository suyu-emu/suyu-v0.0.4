#!/usr/bin/env python3
"""
Test script to validate Forgejo API connection

This script tests the connection to the Forgejo API and displays
basic information about the repository and available issues.

Usage:
    python test_forgejo_connection.py

Environment Variables:
    FORGEJO_URL: Forgejo API URL (default: https://git.suyu.dev/api/v1/repos/suyu/suyu/issues)
    FORGEJO_TOKEN: Forgejo access token (optional)
"""

import os
import requests
import json


def test_forgejo_connection():
    # Default to the suyu repository if not specified
    forgejo_api_url = os.getenv("FORGEJO_URL", "https://git.suyu.dev/api/v1/repos/suyu/suyu/issues")
    forgejo_token = os.getenv("FORGEJO_TOKEN")

    # Headers for API requests
    headers = {}
    if forgejo_token:
        headers["Authorization"] = f"token {forgejo_token}"
        print("🔐 Using authentication token")
    else:
        print("🌐 Testing without authentication (public access)")

    print(f"🔗 Testing connection to: {forgejo_api_url}")

    try:
        # Test basic connection
        response = requests.get(forgejo_api_url, headers=headers, params={"per_page": 1})

        if response.status_code == 403:
            print("⚠️  403 Forbidden with auth, trying without...")
            response = requests.get(forgejo_api_url, params={"per_page": 1})

        response.raise_for_status()

        issues = response.json()

        print("✅ Connection successful!")
        print(f"📊 Response status: {response.status_code}")
        print(f"📋 Found {len(issues)} issue(s) in first page")

        if issues:
            first_issue = issues[0]
            print(f"\n📝 Sample issue:")
            print(f"   Title: {first_issue.get('title', 'N/A')}")
            print(f"   State: {first_issue.get('state', 'N/A')}")
            print(f"   Labels: {[label['name'] for label in first_issue.get('labels', [])]}")
            print(f"   Created: {first_issue.get('created_at', 'N/A')}")

        return True

    except requests.exceptions.RequestException as e:
        print(f"❌ Connection failed: {e}")
        return False


if __name__ == "__main__":
    test_forgejo_connection()
