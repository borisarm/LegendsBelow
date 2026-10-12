# Test.ps1 - Build the solution and run every GoogleTest executable, failing on the first problem
#
#   Scripts/Test.ps1                          Debug build and tests
#   Scripts/Test.ps1 -Configuration Release   Release
#   Scripts/Test.ps1 -AddressSanitizer        instrumented build (rebuilds everything), tests
#                                             under ASan; run a plain build afterwards to go back
#   Scripts/Test.ps1 -StaticAnalysis          build with /analyze (findings fail the build), tests
#   Scripts/Test.ps1 -NoBuild                 run the tests of the last build
#   Scripts/Test.ps1 -Coverage                each test under OpenCppCoverage (it must be on PATH or
#                                             in Program Files): x64\<Configuration>\Coverage holds
#                                             coverage.xml (Cobertura) and an HTML report
#
# Run from anywhere; tests run from the repository so they find Assets/. Exits non-zero when
# the build fails, a test fails, or AddressSanitizer reports an error.
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [switch]$AddressSanitizer,
    [switch]$StaticAnalysis,
    [switch]$NoBuild,
    [switch]$Coverage
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
    if ($StaticAnalysis) { $arguments += '-p:LBStaticAnalysis=true' }
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

# Coverage: our components' sources only (not the tests, not vcpkg's)
if ($Coverage) {
    $occ = (Get-Command OpenCppCoverage -ErrorAction SilentlyContinue).Source
    if (-not $occ) { $occ = Join-Path $env:ProgramFiles 'OpenCppCoverage\OpenCppCoverage.exe' }
    if (-not (Test-Path $occ)) { throw 'OpenCppCoverage was not found (choco install opencppcoverage)' }
    $coverageDir = Join-Path $root "x64\$Configuration\Coverage"
    if (Test-Path $coverageDir) { Remove-Item -Recurse -Force $coverageDir }
    New-Item -ItemType Directory -Force $coverageDir | Out-Null
    $sources = Join-Path $root 'Components'
    $excluded = Join-Path $root 'Components\Tests'
}

$tests = Get-ChildItem (Join-Path $root "x64\$Configuration") -Filter '*Tests.exe' | Sort-Object Name
if (-not $tests) { throw "No test executables in x64\$Configuration" }

$failed = @()
Push-Location $root
try {
    foreach ($test in $tests) {
        if ($Coverage) {
            $output = & $occ --quiet --sources $sources --excluded_sources $excluded --working_dir $root `
                --export_type "binary:$(Join-Path $coverageDir "$($test.BaseName).cov")" -- $test.FullName 2>&1 | Out-String
        }
        else {
            $output = & $test.FullName 2>&1 | Out-String
        }
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

# Every test's coverage merged: Cobertura XML (for tools) and HTML (for people), and the line rate
if ($Coverage) {
    $merge = @()
    foreach ($file in Get-ChildItem $coverageDir -Filter '*.cov') { $merge += @('--input_coverage', $file.FullName) }
    & $occ --quiet @merge --export_type "cobertura:$(Join-Path $coverageDir 'coverage.xml')" --export_type "html:$(Join-Path $coverageDir 'html')"
    if ($LASTEXITCODE -ne 0) { throw "Merging the coverage failed ($LASTEXITCODE)" }
    $rate = [double]([xml](Get-Content (Join-Path $coverageDir 'coverage.xml'))).coverage.'line-rate'
    Write-Host ("Line coverage: {0:P1} (x64\$Configuration\Coverage)" -f $rate) -ForegroundColor Green
}
Write-Host "All $($tests.Count) test executables passed ($Configuration$(if ($AddressSanitizer) { ', AddressSanitizer' }))" -ForegroundColor Green
