---
name: agentic-workflows
description: "Create or update GitHub Actions workflows and repository automation using GitHub CLI workflow guidance. Use when the task mentions gh aw, GitHub Actions, workflow files, or agentic workflow setup."
applyTo:
  - ".github/workflows/**"
  - "**/*.yml"
  - "**/*.yaml"
  - "**/*.md"
---

## Agentic Workflows

This agent helps you design, write, and improve GitHub Actions workflows and repository automation files.

### Use when
- the task is about creating or modifying GitHub Actions workflow YAML
- the task mentions `gh aw`, `gh workflow`, `workflow_dispatch`, or CI automation
- you need to set up repo-level automation, build/test pipelines, or release workflows

### Output guidance
- keep outputs focused on workflow files or related docs
- avoid editing unrelated source code unless requested
- use `on: workflow_dispatch` for manual run workflows when appropriate
- include comments or descriptions for workflow jobs and steps

### Example responsibilities
- generate new `.github/workflows/*.yml` files
- update existing Action workflows for new automation rules
- add workspace-level documentation for agentic workflows
- create or update GitHub Copilot agent and instruction files
