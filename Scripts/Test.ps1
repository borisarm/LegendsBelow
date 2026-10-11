# Test.ps1 - Build the solution and run every GoogleTest executable, failing on the first problem
#
#   Scripts/Test.ps1                          Debug build and tests
#   Scripts/Test.ps1 -Configuration Release   Release
#   Scripts/Test.ps1 -AddressSanitizer        instrumented build (rebuilds everything), tests
#                                             under ASan; run a plain build afterwards to go back
#   Scripts/Test.ps1 -NoBuild                 run the tests of the last build
#
# Run from anywhere; tests run from the repository so they find Assets/. Exits non-zero when
# the build fails, a test fails, or AddressSanitizer reports an error.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$AddressSanitizer,
    [switch]$NoBuild
)

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot

# Visual Studio (prerelease included) through vswhere, which every VS install ships
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -prerelease -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vs) { throw 'Visual Studio with MSBuild was not found' }
$msbuild = Join-Path $vs 'MSBuild\Current\Bin\MSBuild.exe'

if (-not $NoBuild) {
    $arguments = @((Join-Path $root 'LegendsBelow.slnx'), '-m', '-nologo', '-v:minimal',
                   "-p:Configuration=$Configuration", '-p:Platform=x64')
    if ($AddressSanitizer) { $arguments += @('-t:Rebuild', '-p:LBAddressSanitizer=true') }
    & $msbuild @arguments
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
}

# The ASan runtime ships with the toolset the build used (the default version)
if ($AddressSanitizer) {
    $toolset = (Get-Content (Join-Path $vs 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt')).Trim()
    $asanBin = Join-Path $vs "VC\Tools\MSVC\$toolset\bin\Hostx64\x64"
    if (-not (Test-Path (Join-Path $asanBin 'clang_rt.asan_dynamic-x86_64.dll'))) { throw "ASan runtime not found in $asanBin" }
    $env:PATH = "$asanBin;$env:PATH"
}

$tests = Get-ChildItem (Join-Path $root "x64\$Configuration") -Filter '*Tests.exe' | Sort-Object Name
if (-not $tests) { throw "No test executables in x64\$Configuration" }

$failed = @()
Push-Location $root
try {
    foreach ($test in $tests) {
        $output = & $test.FullName 2>&1 | Out-String
        $summary = (($output -split "`n" | Where-Object { $_ -match '^\[  (PASSED|FAILED)  \]' }) -join ' ').Trim()
        # No "[  PASSED  ]" line means the tests did not run (a crash, or gtest printing its help)
        if ($LASTEXITCODE -ne 0 -or $output -match 'ERROR: AddressSanitizer' -or $output -notmatch '\[  PASSED  \] \d+ test') {
            $failed += $test.Name
            Write-Host "FAIL  $($test.Name)  $summary" -ForegroundColor Red
            Write-Host $output
        }
        else {
            Write-Host "ok    $($test.Name)  $summary"
        }
    }
}
finally {
    Pop-Location
}

if ($failed) { throw "$($failed.Count) test executable(s) failed: $($failed -join ', ')" }
Write-Host "All $($tests.Count) test executables passed ($Configuration$(if ($AddressSanitizer) { ', AddressSanitizer' }))" -ForegroundColor Green
