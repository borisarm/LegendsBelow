<#

.NOTES
Copyright (c) Microsoft Corporation (the GDK template it started from).
Licensed under the MIT License.

.SYNOPSIS
Creates the game's layout from a build and packages it with makepkg.

.DESCRIPTION
PackageLayout.ps1 - Copies what the game ships (LegendsBelow.exe, MicrosoftGame.config, the
Agility SDK's D3D12 folder, and Assets) from x64\<Configuration> into <Destination>\Loose, checks
that the layout is complete (every Assets\Data file the game loads, the models, fonts, sounds,
music, menu art, and the Store logos MicrosoftGame.config names), and then runs makepkg (genmap,
then pack) unless -NoPackage. Tests, libraries, and symbols stay out.

.PARAMETER Destination
Where the layout (Destination\Loose) and the package go.

.PARAMETER Configuration
'Debug' or 'Release' (default) build to lay out.

.PARAMETER Overwrite
Lay out over an existing layout.

.PARAMETER Clean
Delete the existing layout first.

.PARAMETER Quiet
Print only problems and the summary.

.PARAMETER NoPackage
Lay out and check, without makepkg.

.LINK
https://aka.ms/gdkdocs

#>

param(
    [Parameter(Mandatory)]
    [string]$Destination,
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = "Release",
    [switch]$Overwrite,
    [switch]$Clean,
    [switch]$Quiet,
    [switch]$NoPackage
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$build = Join-Path $root "x64\$Configuration"
if (-not (Test-Path (Join-Path $build 'LegendsBelow.exe'))) { throw "No build at $build (build the solution first)" }

$layout = Join-Path $Destination 'Loose'
if ($Clean -and (Test-Path $layout)) { Remove-Item -Recurse -Force $layout }
if ((Test-Path $layout) -and -not $Overwrite) { throw "$layout exists: use -Overwrite or -Clean" }
New-Item -ItemType Directory -Force $layout | Out-Null
if (-not $Quiet) { Write-Host "Laying out $build in $layout" }

# What ships: the game, its configuration, the Agility SDK, and its assets (nothing else)
Copy-Item (Join-Path $build 'LegendsBelow.exe') $layout -Force
Copy-Item (Join-Path $build 'MicrosoftGame.config') $layout -Force
foreach ($folder in 'D3D12', 'Assets') {
    $source = Join-Path $build $folder
    if (-not (Test-Path $source)) { throw "The build has no $folder folder" }
    Copy-Item $source $layout -Recurse -Force
}

# Complete: every data file of the repository, and every kind of asset the game loads
$problems = @()
foreach ($file in Get-ChildItem (Join-Path $root 'Assets\Data') -File) {
    if (-not (Test-Path (Join-Path $layout "Assets\Data\$($file.Name)"))) { $problems += "Assets\Data\$($file.Name) is missing (is it in LegendsBelow.vcxproj's CopyFileToFolders?)" }
}
foreach ($folder in 'Models', 'Models\Animations', 'Fonts', 'Sounds', 'Music', 'UI') {
    $path = Join-Path $layout "Assets\$folder"
    if (-not (Test-Path $path) -or -not (Get-ChildItem $path -File)) { $problems += "Assets\$folder is missing or empty" }
}
$config = [xml](Get-Content (Join-Path $layout 'MicrosoftGame.config'))
foreach ($logo in $config.SelectNodes('//*[@StoreLogo or @Square150x150Logo or @Square44x44Logo or @SplashScreenImage]')) {
    foreach ($attribute in 'StoreLogo', 'Square150x150Logo', 'Square44x44Logo', 'SplashScreenImage') {
        $value = $logo.GetAttribute($attribute)
        if ($value -and -not (Test-Path (Join-Path $layout $value))) { $problems += "$value (MicrosoftGame.config $attribute) is missing" }
    }
}
if ($problems) {
    $problems | ForEach-Object { Write-Host "MISSING  $_" -ForegroundColor Red }
    throw "The layout is incomplete ($($problems.Count) problem(s))"
}
$size = (Get-ChildItem $layout -Recurse -File | Measure-Object Length -Sum).Sum
Write-Host ("Layout complete: {0} files, {1:N1} MB" -f (Get-ChildItem $layout -Recurse -File).Count, ($size / 1MB)) -ForegroundColor Green

if ($NoPackage) { return }

# makepkg from the GDK vcpkg installed: the chunk map first, then the package, each waited for
$makepkg = Get-ChildItem (Join-Path $root 'vcpkg_installed') -Recurse -Filter 'makepkg*.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $makepkg) { $makepkg = Get-ChildItem (Join-Path $PSScriptRoot 'vcpkg_installed') -Recurse -Filter 'makepkg*.exe' -ErrorAction SilentlyContinue | Select-Object -First 1 }
if (-not $makepkg) { throw 'makepkg was not found in vcpkg_installed' }
Push-Location $Destination
try {
    & $makepkg.FullName genmap /f chunks.xml /d Loose
    if ($LASTEXITCODE -ne 0) { throw "makepkg genmap failed ($LASTEXITCODE)" }
    & $makepkg.FullName pack /f chunks.xml /d Loose /pd . /pc
    if ($LASTEXITCODE -ne 0) { throw "makepkg pack failed ($LASTEXITCODE)" }
}
finally {
    Pop-Location
}
Write-Host "Packaged in $Destination" -ForegroundColor Green
