#!/usr/bin/env bash
set -euo pipefail

BRANCH=${1:-master}
OUT_DIR=${2:-Executable/Bin}

if ! command -v gh >/dev/null 2>&1; then
    echo "Error: gh CLI is required." >&2
    exit 1
fi

REPO=$(gh repo view --json nameWithOwner -q .nameWithOwner)
RUN_ID=$(gh run list \
    --repo "$REPO" \
    --branch "$BRANCH" \
    --workflow "Build and Release" \
    --json databaseId,conclusion \
    --jq '.[] | select(.conclusion=="success") | .databaseId' \
    --limit 1)

if [ -z "$RUN_ID" ]; then
    echo "Error: no successful 'Build and Release' run found for branch '$BRANCH'." >&2
    exit 1
fi

mkdir -p "$OUT_DIR"
gh run download "$RUN_ID" --repo "$REPO" --name touchfreeze-bin --dir "$OUT_DIR"
echo "Downloaded latest build (run $RUN_ID) to $OUT_DIR"
