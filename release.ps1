# release.ps1 - Complete 1-Command Release Workflow for TouchFreeze
param(
    [string]$Version,
    [string]$Message
)

$ErrorActionPreference = "Stop"

# 1. Determine Target Version
$setupMakPath = "Sources\Setup\Setup.mak"
$rcPath       = "Sources\TestApp\KeyHookTest.rc"

if (-not (Test-Path $setupMakPath) -or -not (Test-Path $rcPath)) {
    throw "Required version files not found in Sources."
}

$setupMakContent = Get-Content $setupMakPath -Raw
if ($setupMakContent -match '-dVERSION=(\d+\.\d+\.\d+)') {
    $currentVersion = $matches[1]
} else {
    throw "Could not parse current version from $setupMakPath"
}

if (-not $Version) {
    # Auto-increment patch version (e.g. 1.2.3 -> 1.2.4)
    $parts = $currentVersion.Split('.')
    $major = [int]$parts[0]
    $minor = [int]$parts[1]
    $patch = [int]$parts[2] + 1
    $Version = "$major.$minor.$patch"
}

# Clean version prefix (strip leading 'v' if provided)
$cleanVersion = $Version.TrimStart('v', 'V')
$tag = "v$cleanVersion"
$verCommas = $cleanVersion.Replace('.', ',') + ",0"

if (-not $Message) {
    $Message = "Release $tag"
}

Write-Host "==========================================" -ForegroundColor Cyan
Write-Host " Starting TouchFreeze Release: $tag" -ForegroundColor Cyan
Write-Host " Current Version: $currentVersion -> New Version: $cleanVersion" -ForegroundColor Gray
Write-Host "==========================================" -ForegroundColor Cyan

# 2. Update Version in Setup.mak
Write-Host "`n[1/5] Updating version numbers..." -ForegroundColor Yellow
$setupMakContent = $setupMakContent -replace '-dVERSION=\d+\.\d+\.\d+', "-dVERSION=$cleanVersion"
Set-Content $setupMakPath $setupMakContent -NoNewline

# Update Version in KeyHookTest.rc
$rcContent = Get-Content $rcPath -Raw
$rcContent = $rcContent -replace 'TouchFreeze \d+\.\d+\.\d+', "TouchFreeze $cleanVersion"
$rcContent = $rcContent -replace 'FILEVERSION \d+,\d+,\d+,\d+', "FILEVERSION $verCommas"
$rcContent = $rcContent -replace 'PRODUCTVERSION \d+,\d+,\d+,\d+', "PRODUCTVERSION $verCommas"
$rcContent = $rcContent -replace 'VALUE "FileVersion", "\d+\.\d+\.\d+\.\d+"', "VALUE `"FileVersion`", `"$cleanVersion.0`""
$rcContent = $rcContent -replace 'VALUE "ProductVersion", "\d+\.\d+\.\d+\.\d+"', "VALUE `"ProductVersion`", `"$cleanVersion.0`""
Set-Content $rcPath $rcContent -NoNewline

Write-Host "Version updated in $setupMakPath and $rcPath" -ForegroundColor Green

# 3. Local Build & Validation
Write-Host "`n[2/5] Running local build and artifact verification..." -ForegroundColor Yellow
& powershell -ExecutionPolicy Bypass -File .\build.ps1
if ($LASTEXITCODE -ne 0) {
    throw "Local build failed."
}

# 4. Git Commit & Tag
Write-Host "`n[3/5] Committing changes and creating tag $tag..." -ForegroundColor Yellow
$branch = (git branch --show-current).Trim()
if (-not $branch) {
    $branch = "master"
}

git add -A
$status = git status --porcelain
if ($status) {
    git commit -m $Message
    if ($LASTEXITCODE -ne 0) {
        throw "Git commit failed."
    }
} else {
    Write-Host "No unstaged changes to commit." -ForegroundColor Gray
}

# Create tag if it does not exist
$existingTag = git tag -l $tag
if ($existingTag) {
    Write-Host "Tag $tag already exists locally. Updating tag..." -ForegroundColor Yellow
    git tag -d $tag | Out-Null
}
git tag $tag
if ($LASTEXITCODE -ne 0) {
    throw "Failed to create tag $tag."
}

# 5. Push to GitHub
Write-Host "`n[4/5] Pushing commits and tag ($tag) to origin/$branch..." -ForegroundColor Yellow
git push origin $branch
if ($LASTEXITCODE -ne 0) {
    throw "Git push branch failed."
}
git push origin $tag --force
if ($LASTEXITCODE -ne 0) {
    throw "Git push tag failed."
}

# 6. Watch GitHub Actions Workflow & Verify Release
Write-Host "`n[5/5] Waiting for GitHub Actions release workflow..." -ForegroundColor Yellow
Start-Sleep -Seconds 5

$runId = ""
for ($i = 0; $i -lt 10; $i++) {
    $runId = (gh run list --workflow=build.yml --limit 1 --json databaseId -q '.[0].databaseId').Trim()
    if ($runId) {
        break
    }
    Start-Sleep -Seconds 3
}

if ($runId) {
    Write-Host "Watching workflow run #$runId..." -ForegroundColor DarkGray
    & gh run watch $runId
} else {
    Write-Warning "Could not retrieve workflow run ID automatically. Check GitHub Actions online."
}

Write-Host "`n=== Release Verification ===" -ForegroundColor Cyan
& gh release view $tag

Write-Host "`nRelease $tag completed successfully!" -ForegroundColor Green
