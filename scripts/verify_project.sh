#!/usr/bin/env bash
set -euo pipefail

repo_root="$(git rev-parse --show-toplevel)"
cd "${repo_root}"

if git ls-files --error-unmatch .playwright-mcp >/dev/null 2>&1; then
  echo "Tracked .playwright-mcp artifacts found. Remove them from git before publishing." >&2
  git ls-files .playwright-mcp >&2
  exit 1
fi

find resources/i18n -name '*.json' -print0 | xargs -0 jq empty

if find . -path './library' -prune -o -path './resources/i18n/*/resources/i18n/*' -print | grep -q .; then
  echo "Nested Crowdin translation paths found under resources/i18n." >&2
  exit 1
fi

if find . -maxdepth 1 -type d -regex './[a-z][a-z]\\(-[A-Z][A-Z]\\)?' -exec test -d '{}/resources/i18n' ';' -print | grep -q .; then
  echo "Crowdin translation folders were generated at repository root." >&2
  exit 1
fi

git diff --check
