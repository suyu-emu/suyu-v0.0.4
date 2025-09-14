#!/usr/bin/env python3
"""
Simple test script to verify Forgejo API connection
"""

import sys

import requests


def test_forgejo_api():
    """Test basic connection to Forgejo API"""
    url = "https://git.suyu.dev/api/v1/repos/suyu/suyu/issues"

    print(f"Testing connection to: {url}")

    try:
        # First attempt with potential authentication
        response = requests.get(url, params={"per_page": 1}, timeout=10)

        if response.status_code == 200:
            issues = response.json()
            print(f"✅ Connection successful! Found {len(issues)} issue(s)")
            if issues:
                print(f"Sample issue: {issues[0].get('title', 'N/A')}")
            return True
        elif response.status_code == 403:
            print("⚠️  403 Forbidden: Trying without authentication...")
            # Try without authentication for public repos (same logic as main script)
            try:
                response = requests.get(url, params={"per_page": 1}, timeout=10)
                if response.status_code == 200:
                    issues = response.json()
                    print(f"✅ Connection successful without auth! Found {len(issues)} issue(s)")
                    if issues:
                        print(f"Sample issue: {issues[0].get('title', 'N/A')}")
                    return True
                else:
                    print(f"❌ Still failed without auth: HTTP {response.status_code}")
                    return False
            except Exception as e:
                print(f"❌ Failed without auth: {e}")
                return False
        else:
            print(f"❌ HTTP {response.status_code}: {response.text}")
            return False

    except Exception as e:
        print(f"❌ Connection failed: {e}")
        return False

if __name__ == "__main__":
    sys.exit(0 if test_forgejo_api() else 1)
