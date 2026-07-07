# ============================================================================
# AMU publish helper (C++ app) - the successor of publish.au3.
# Run AFTER a release build (cpp\build.ps1). Produces .\publish\release\ - the
# asset set for a GitHub Release (the update channel is
#   https://github.com/oNdsen/ark-mod-updater/releases/latest/download/<asset> ):
#   version.txt      plain version (parsed from cpp\CMakeLists.txt project VERSION)
#   manifest.txt     one line per file: <relpath>|<sha256 lowercase hex>|<size>
#   <flat assets>    amu.exe, sciter.dll, lib_amu_updater.exe, ui_main.html, ...
#                    (GitHub assets have no directories -> '/' and '\' become '_';
#                    the updater flattens the SAME way when building URLs, while
#                    manifest relpaths keep their real slashes for installing)
# plus .\publish\amu-cpp-<version>.zip for manual downloads (REAL folder layout).
#
# The manifest is the updater's authority: amu_updater.exe only ever touches
# files listed here. lib\amu.db (user data) and lib\steamcmd\ (installed
# separately) are deliberately NOT listed.
# Usage:  .\publish.ps1                 package the current build
#         .\publish.ps1 -WithSteamcmd   also bundle lib\steamcmd into the zip
#                                       (zip only - never into the manifest)
# Release:  gh release create v<version> .\publish\release\* --title "AMU v<version>"
# ============================================================================
param([switch]$WithSteamcmd)
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$buildDir = Join-Path $root "cpp\build\amu"
$outRoot = Join-Path $root "publish"
$relDir = Join-Path $outRoot "release"
$zipStage = Join-Path $outRoot "zip-stage"

# --- version from the CMake project() line -----------------------------------
$cmake = Get-Content (Join-Path $root "cpp\CMakeLists.txt") -Raw
if ($cmake -notmatch 'project\(\s*amu\s+VERSION\s+([0-9.]+)') {
  throw "Could not parse the project VERSION from cpp\CMakeLists.txt"
}
$version = $Matches[1]

# --- collect the package files (relative to the build dir) -------------------
# The ui/ folder is NOT shipped: it is packed INTO amu.exe at build time
# (Sciter packfolder resources); a loose ui\ next to the exe is dev-only.
foreach ($required in @("amu.exe", "sciter.dll", "lib\amu_updater.exe")) {
  if (-not (Test-Path (Join-Path $buildDir $required))) {
    throw "Missing $required in $buildDir - run cpp\build.ps1 first."
  }
}
$files = @("amu.exe", "sciter.dll", "lib\amu_updater.exe")

# --- flat release-asset folder + manifest --------------------------------------
foreach ($d in @($relDir, $zipStage)) {
  if (Test-Path $d) { Remove-Item $d -Recurse -Force -Confirm:$false }
  New-Item -ItemType Directory -Force -Path $d | Out-Null
}

$manifest = @("# AMU update manifest v$version - <relpath>|<sha256>|<size>")
foreach ($rel in $files) {
  $src = Join-Path $buildDir $rel
  $hash = (Get-FileHash $src -Algorithm SHA256).Hash.ToLower()
  $size = (Get-Item $src).Length
  $manifest += "$($rel -replace '\\','/')|$hash|$size"
  # flat asset name for the GitHub release (same rule as updateAssetName in C++)
  Copy-Item $src (Join-Path $relDir ($rel -replace '[\\/]', '_')) -Force
  # real layout for the manual zip
  $zdst = Join-Path $zipStage $rel
  New-Item -ItemType Directory -Force -Path (Split-Path $zdst) | Out-Null
  Copy-Item $src $zdst -Force
}
Set-Content -Path (Join-Path $relDir "manifest.txt") -Value ($manifest -join "`n") -Encoding utf8NoBOM
Set-Content -Path (Join-Path $relDir "version.txt") -Value $version -Encoding utf8NoBOM -NoNewline

# steamcmd.exe as an EXTRA release asset (NOT in the manifest - the updater never
# touches it): AMU bootstraps a missing SteamCMD from this asset, and steamcmd
# self-installs the rest of its files on first run.
$steamcmd = @("$buildDir\lib\steamcmd\steamcmd.exe", "$root\lib\steamcmd\steamcmd.exe") |
  Where-Object { Test-Path $_ } | Select-Object -First 1
if ($steamcmd) { Copy-Item $steamcmd (Join-Path $relDir "steamcmd.exe") -Force }
else { Write-Warning "steamcmd.exe not found - the bootstrap asset will be missing from this release." }

# --- zip for manual download (real folder layout) ------------------------------
$zip = Join-Path $outRoot "amu-cpp-$version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force -Confirm:$false }
if ($WithSteamcmd -and (Test-Path (Join-Path $buildDir "lib\steamcmd"))) {
  Copy-Item (Join-Path $buildDir "lib\steamcmd") (Join-Path $zipStage "lib\steamcmd") -Recurse -Force
}
Compress-Archive -Path (Join-Path $zipStage "*") -DestinationPath $zip
Remove-Item $zipStage -Recurse -Force -Confirm:$false

"Published v$version"
"  Release assets : $relDir"
"  Manual zip     : $zip  (attach to the release too)"
"  Manifest files : $($files.Count)"
""
"Create the GitHub release (update channel = 'latest'):"
"  gh release create v$version `"$relDir\*`" `"$zip`" --title `"AMU v$version`""
