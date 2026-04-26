---
description: "Workspace instructions for GitHub Copilot to assist with repository automation, GitHub Actions, and agentic workflow setup."
applyTo:
  - ".github/**"
  - "src/**"
---

# GitHub Actions and Agentic Workflow Instructions

When editing this repository, use the following conventions for GitHub Actions and agentic workflows:

- Prefer creating or updating workflow files in `.github/workflows/`.
- Use `workflow_dispatch` for workflows that should run manually.
- Use `pull_request` and `push` triggers for CI validation on branches.
- Keep YAML syntax valid and avoid unrelated source changes.
- If asked to set up agentic workflows, create or update `.github/agents/` and `.github/AGENTS.md` to expose available agent definitions.
- When the task mentions `gh aw` or GitHub Actions workflow guidance, focus on workflow file structure, jobs, and steps.
