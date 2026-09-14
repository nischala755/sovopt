param(
    [ValidateSet('Release','RelWithDebInfo')][string]$Configuration = 'Release',
    [string]$BuildDirectory = 'build/cuda',
    [string]$CudaArchitectures = 'native',
    [int]$Scale = 100000,
    [int]$Iterations = 100,
    [string]$EvidenceDirectory = 'benchmark-results/cuda'
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not [IO.Path]::IsPathRooted($BuildDirectory)){$BuildDirectory=Join-Path $root $BuildDirectory}
$nvcc=Get-Command nvcc -ErrorAction SilentlyContinue
$smi=Get-Command nvidia-smi -ErrorAction SilentlyContinue
if(-not $nvcc){throw 'CUDA toolkit not found: nvcc must be on PATH.'}
if(-not $smi){throw 'NVIDIA driver utility not found: nvidia-smi must be on PATH.'}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if(-not (Test-Path -LiteralPath $vswhere)){throw 'Visual Studio Installer vswhere.exe was not found.'}
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'A Visual Studio x64 C++ toolchain is required by CUDA on Windows.'}
$cmake=Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if(-not (Test-Path -LiteralPath $cmake)){$found=Get-Command cmake -ErrorAction SilentlyContinue;if(-not $found){throw 'CMake 3.25 or newer was not found.'};$cmake=$found.Source}
$ctest=Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
$version=& $vswhere -latest -products '*' -property installationVersion
$generator=if([int]$version.Split('.')[0] -eq 18){'Visual Studio 18 2026'}else{'Visual Studio 17 2022'}
& $cmake -S $root -B $BuildDirectory -G $generator -A x64 "-DCMAKE_GENERATOR_INSTANCE=$vs" -DSOVEREIGN_ENABLE_CUDA=ON "-DSOVEREIGN_CUDA_ARCHITECTURES=$CudaArchitectures"
if($LASTEXITCODE-ne 0){throw 'CUDA CMake configuration failed.'}
& $cmake --build $BuildDirectory --config $Configuration --parallel
if($LASTEXITCODE-ne 0){throw 'CUDA build failed.'}
& $ctest --test-dir $BuildDirectory -C $Configuration --output-on-failure
if($LASTEXITCODE-ne 0){throw 'CUDA regression tests failed.'}
$evidence=if([IO.Path]::IsPathRooted($EvidenceDirectory)){$EvidenceDirectory}else{Join-Path $root $EvidenceDirectory};New-Item -ItemType Directory -Force -Path $evidence|Out-Null
& $smi.Source --query-gpu=name,driver_version,memory.total,compute_cap --format=csv,noheader | Set-Content -Encoding utf8 (Join-Path $evidence 'nvidia-smi.csv')
& $nvcc.Source --version | Set-Content -Encoding utf8 (Join-Path $evidence 'nvcc-version.txt')
$probe=Join-Path $BuildDirectory "$Configuration/sovereign_cuda_probe.exe"
& $probe $Scale $Iterations | Set-Content -Encoding utf8 (Join-Path $evidence 'execution-benchmark.json')
if($LASTEXITCODE-ne 0){throw "CUDA numerical probe failed with exit code $LASTEXITCODE."}
git -C $root rev-parse HEAD | Set-Content -Encoding ascii (Join-Path $evidence 'git-commit.txt')
Get-ComputerInfo | Select-Object WindowsProductName,WindowsVersion,OsArchitecture,CsProcessors,CsTotalPhysicalMemory | ConvertTo-Json | Set-Content -Encoding utf8 (Join-Path $evidence 'host.json')
Write-Host "CUDA evidence written to $evidence"
