#!/bin/bash
# release.sh - Automated Release Script for TouchFreeze from Linux/WSL

set -e

REPO="kuwa72/touchfreeze"
VERSION="$1"
MESSAGE="$2"

SETUP_MAK="Sources/Setup/Setup.mak"
RC_FILE="Sources/TestApp/KeyHookTest.rc"

# 1. Determine Target Version
CURRENT_VERSION=$(grep -oP -- '-dVERSION=\K[0-9]+\.[0-9]+\.[0-9]+' "$SETUP_MAK" || true)
if [ -z "$CURRENT_VERSION" ]; then
    echo "[ERROR] Could not parse current version from $SETUP_MAK"
    exit 1
fi

if [ -z "$VERSION" ]; then
    # Auto-increment patch version (e.g. 1.2.3 -> 1.2.4)
    IFS='.' read -r major minor patch <<< "$CURRENT_VERSION"
    patch=$((patch + 1))
    VERSION="$major.$minor.$patch"
fi

CLEAN_VERSION="${VERSION#v}"
CLEAN_VERSION="${CLEAN_VERSION#V}"
TAG="v$CLEAN_VERSION"
VER_COMMAS="${CLEAN_VERSION//./,},0"

if [ -z "$MESSAGE" ]; then
    MESSAGE="Release $TAG"
fi

echo "=========================================="
echo " Starting TouchFreeze Release: $TAG"
echo " Current Version: $CURRENT_VERSION -> New Version: $CLEAN_VERSION"
echo "=========================================="

# 2. Update Version in Setup.mak
echo "[1/4] Updating version numbers in Setup.mak and KeyHookTest.rc..."
sed -i "s/-dVERSION=[0-9]\+\.[0-9]\+\.[0-9]\+/-dVERSION=$CLEAN_VERSION/" "$SETUP_MAK"

# Update Version in KeyHookTest.rc
sed -i "s/TouchFreeze [0-9]\+\.[0-9]\+\.[0-9]\+/TouchFreeze $CLEAN_VERSION/g" "$RC_FILE"
sed -i "s/FILEVERSION [0-9]\+,[0-9]\+,[0-9]\+,[0-9]\+/FILEVERSION $VER_COMMAS/g" "$RC_FILE"
sed -i "s/PRODUCTVERSION [0-9]\+,[0-9]\+,[0-9]\+,[0-9]\+/PRODUCTVERSION $VER_COMMAS/g" "$RC_FILE"
sed -i "s/VALUE \"FileVersion\", \"[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+\"/VALUE \"FileVersion\", \"$CLEAN_VERSION.0\"/g" "$RC_FILE"
sed -i "s/VALUE \"ProductVersion\", \"[0-9]\+\.[0-9]\+\.[0-9]\+\.[0-9]\+\"/VALUE \"ProductVersion\", \"$CLEAN_VERSION.0\"/g" "$RC_FILE"

# 3. Commit and Tag
echo "[2/4] Committing changes and creating tag $TAG..."
git add -A
if ! git diff-index --quiet HEAD --; then
    git commit -m "$MESSAGE"
else
    echo "No unstaged changes to commit."
fi

git tag -f "$TAG"

# 4. Push to GitHub
echo "[3/4] Pushing commits and tag $TAG to origin..."
BRANCH=$(git branch --show-current)
git push origin "$BRANCH"
git push origin "$TAG" --force

# 5. Monitor GitHub Actions
echo "[4/4] Monitoring GitHub Actions release workflow..."
sleep 5

RUN_ID=$(gh run list -R "$REPO" --workflow=build.yml --limit 1 --json databaseId -q '.[0].databaseId')
echo "Watching workflow run #$RUN_ID..."
gh run watch -R "$REPO" "$RUN_ID"

echo ""
echo "=== GitHub Release Verification ==="
gh release view -R "$REPO" "$TAG"

echo ""
echo "=== Fetching Latest Release Artifacts to Downloads ==="
./scripts/fetch-latest-build.sh

echo ""
echo "=== Release $TAG completed successfully! ==="
