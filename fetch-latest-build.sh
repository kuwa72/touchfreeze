#!/usr/bin/env bash
set -euo pipefail

BRANCH=${1:-master}
OUT_DIR=${2:-Executable/Bin}
WIN_DIR=${3:-}

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

TMP_DIR=$(mktemp -d)
trap 'rm -rf "$TMP_DIR"' EXIT

gh run download "$RUN_ID" --repo "$REPO" --name touchfreeze-bin --dir "$TMP_DIR"

if [ -d "$TMP_DIR/touchfreeze-bin" ]; then
    SRC_DIR="$TMP_DIR/touchfreeze-bin"
else
    SRC_DIR="$TMP_DIR"
fi

mkdir -p "$OUT_DIR"
cp -f "$SRC_DIR"/* "$OUT_DIR"/

if [ -n "$WIN_DIR" ]; then
    if [[ "$WIN_DIR" == /* ]]; then
        WIN_ABS="$WIN_DIR"
    else
        WIN_ABS=$(wslpath -u "$WIN_DIR")
    fi
    mkdir -p "$WIN_ABS"
    cp -f "$SRC_DIR"/* "$WIN_ABS"/
    echo "Copied to Windows: $WIN_DIR"
fi

echo "Downloaded latest build (run $RUN_ID) to $OUT_DIR"
ls -la "$OUT_DIR"
