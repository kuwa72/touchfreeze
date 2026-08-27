#!/bin/bash
# fetch-latest-build.sh
# Fetches latest GitHub Actions build artifact and extracts to target directory

set -e

REPO="kuwa72/touchfreeze"

if command -v cmd.exe >/dev/null 2>&1; then
    WIN_PROFILE=$(cmd.exe /c 'echo %USERPROFILE%' 2>/dev/null | tr -d '\r')
fi
if [ -z "$WIN_PROFILE" ]; then
    WIN_PROFILE="C:\\Users\\$USER"
fi

WSL_PROFILE=""
if command -v wslpath >/dev/null 2>&1; then
    WSL_PROFILE=$(wslpath -u "$WIN_PROFILE" 2>/dev/null || true)
fi
if [ -z "$WSL_PROFILE" ]; then
    WSL_PROFILE="/mnt/c/Users/$USER"
fi

DEST_WIN_DIR="$WIN_PROFILE\\Downloads\\touchfreeze-latest"
DEST_WSL_DIR="$WSL_PROFILE/Downloads/touchfreeze-latest"

echo "=== TouchFreeze Remote Build Artifact Fetcher ==="

# Check gh CLI
if ! command -v gh &> /dev/null; then
    echo "[ERROR] 'gh' (GitHub CLI) is not installed or not in PATH."
    exit 1
fi

echo "[1/4] Checking latest workflow run..."
RUN_ID=$(gh run list -R "$REPO" --workflow=build.yml --limit 1 --json databaseId -q '.[0].databaseId')
RUN_STATUS=$(gh run list -R "$REPO" --workflow=build.yml --limit 1 --json status -q '.[0].status')
RUN_CONCLUSION=$(gh run list -R "$REPO" --workflow=build.yml --limit 1 --json conclusion -q '.[0].conclusion')

echo "Latest Run ID: $RUN_ID (Status: $RUN_STATUS, Conclusion: $RUN_CONCLUSION)"

if [ "$RUN_STATUS" != "completed" ]; then
    echo "[INFO] Workflow run is currently $RUN_STATUS. Watching run until completion..."
    gh run watch -R "$REPO" "$RUN_ID"
fi

echo "[2/4] Preparing target directory: $DEST_WSL_DIR"
mkdir -p "$DEST_WSL_DIR"

TEMP_DIR=$(mktemp -d)
trap "rm -rf '$TEMP_DIR'" EXIT

echo "[3/4] Downloading artifacts from run $RUN_ID..."
gh run download -R "$REPO" "$RUN_ID" --name touchfreeze-bin --dir "$TEMP_DIR"

if [ -d "$TEMP_DIR/touchfreeze-bin" ]; then
    SRC_DIR="$TEMP_DIR/touchfreeze-bin"
else
    SRC_DIR="$TEMP_DIR"
fi

echo "[4/4] Copying files to $DEST_WIN_DIR ($DEST_WSL_DIR)..."
cp -fv "$SRC_DIR"/* "$DEST_WSL_DIR/"

echo ""
echo "=== Done! Artifacts copied successfully ==="
ls -lh "$DEST_WSL_DIR"
