param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [string]$BuildDirectory = 'build/msvc'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer vswhere.exe was not found.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Install Visual Studio C++ desktop Build Tools with a Windows SDK.' }
$cmake = Join-Path $installation 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) {
    $found = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $found) { throw 'CMake not found. Install the Visual Studio CMake component or provide cmake on PATH.' }
    $cmake = $found.Source
}
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
$version = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
$major = [int]($version.Split('.')[0])
$generators = @{ 17 = 'Visual Studio 17 2022'; 18 = 'Visual Studio 18 2026' }
if (-not $generators.ContainsKey($major)) { throw "Unsupported Visual Studio major version: $major" }
& $cmake -S $projectRoot -B $BuildDirectory -G $generators[$major] -A x64 "-DCMAKE_GENERATOR_INSTANCE=$installation"
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }
& $cmake --build $BuildDirectory --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }
& $ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'Tests failed.' }
