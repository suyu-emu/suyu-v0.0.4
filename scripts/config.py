"""
Configuration settings for Forgejo to GitHub issue sync
"""

# Default Forgejo API URL
DEFAULT_FORGEJO_URL = "https://git.suyu.dev/api/v1/repos/suyu/suyu/issues"

# Issue title prefix for imported issues
ISSUE_TITLE_PREFIX = "[Forgejo]"

# Label to add to all imported issues
IMPORT_LABEL = "imported-from-forgejo"

# Rate limiting settings
RATE_LIMIT_DELAY = 1  # seconds between issue creations
RATE_LIMIT_BATCH_SIZE = 10  # pause after this many issues
RATE_LIMIT_BATCH_DELAY = 5  # seconds to pause after batch

# API pagination settings
FORGEJO_PAGE_SIZE = 50
GITHUB_PAGE_SIZE = 100

# Issue states to sync from Forgejo
SYNC_STATES = ["open"]  # Can include "closed" if needed

# Attribution message template
ATTRIBUTION_TEMPLATE = """**Imported from Forgejo**: {source_url}

---

{original_body}"""

# Maximum number of issues to process in one run (0 = no limit)
MAX_ISSUES_PER_RUN = 0
