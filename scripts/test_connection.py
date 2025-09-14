#!/usr/bin/env python3
"""
Simple test script to verify Forgejo API connection
"""

import requests
import sys

def test_forgejo_api():
    """Test basic connection to Forgejo API"""
    url = "https://git.suyu.dev/api/v1/repos/suyu/suyu/issues"

    try:
        print(f"Testing connection to: {url}")
        response = requests.get(url, params={"per_page": 1}, timeout=10)

        if response.status_code == 200:
            issues = response.json()
            print(f"✅ Connection successful! Found {len(issues)} issue(s)")
            if issues:
                print(f"Sample issue: {issues[0].get('title', 'N/A')}")
            return True
        elif response.status_code == 403:
            print("⚠️  403 Forbidden - Repository might require authentication")
            return False
        else:
            print(f"❌ HTTP {response.status_code}: {response.text}")
            return False

    except Exception as e:
        print(f"❌ Connection failed: {e}")
        return False

if __name__ == "__main__":
    sys.exit(0 if test_forgejo_api() else 1)
