# build-windows.ps1 - Windows build helper for n0s-ryo-miner
#
# Usage:
#   .\scripts\build-windows.ps1 [-CudaEnable] [-OpenclEnable] [-BuildType Release]
#
# Notes:
#   - Prefers an MSVC + NMake build so it works on stock Windows PowerShell.
#   - If vcpkg is missing, builds the core miner with HTTP/TLS/hwloc disabled.

param(
    [switch]$CudaEnable,
    [switch]$OpenclEnable,
    [string]$BuildType = "Release",
    [string]$CudaArch = "",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$BuildDir = Join-Path $ProjectRoot "build"

function Find-Vcvars64 {
    $pf86 = ${env:ProgramFiles(x86)}
    $candidates = @(
        (Join-Path $pf86 "Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
        (Join-Path $pf86 "Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"),
        (Join-Path $pf86 "Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
        (Join-Path $pf86 "Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"),
        (Join-Path $pf86 "Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvars64.bat"),
        (Join-Path $pf86 "Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat")
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) {
            return $candidate
        }
    }

    return $null
}

function Find-VcpkgToolchain {
    if ($env:VCPKG_ROOT) {
        $toolchain = Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $toolchain) {
            return $toolchain
        }
    }

    $defaultVcpkg = Join-Path $env:USERPROFILE "vcpkg"
    if (Test-Path (Join-Path $defaultVcpkg "vcpkg.exe")) {
        $env:VCPKG_ROOT = $defaultVcpkg
        $toolchain = Join-Path $defaultVcpkg "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $toolchain) {
            return $toolchain
        }
    }

    return $null
}

function Get-CudaVersion {
    $nvcc = Get-Command nvcc.exe -ErrorAction SilentlyContinue
    if ($nvcc) {
        $text = (& $nvcc.Source --version 2>$null | Out-String)
        $match = [regex]::Match($text, 'release\s+(\d+)\.(\d+)')
        if ($match.Success) {
            return [version]::new([int]$match.Groups[1].Value, [int]$match.Groups[2].Value)
        }
    }

    if ($env:CUDA_PATH) {
        $versionTxt = Join-Path $env:CUDA_PATH 'version.txt'
        if (Test-Path $versionTxt) {
            $text = Get-Content $versionTxt -Raw
            $match = [regex]::Match($text, '(\d+)\.(\d+)')
            if ($match.Success) {
                return [version]::new([int]$match.Groups[1].Value, [int]$match.Groups[2].Value)
            }
        }
    }

    return $null
}

function Get-DefaultCudaArch {
    param([version]$CudaVersion)

    if ($null -eq $CudaVersion) {
        return '61;75;80;86;89'
    }
    if ($CudaVersion -lt [version]'11.1') {
        return '61;75;80'
    }
    if ($CudaVersion -lt [version]'12.0') {
        return '61;75;80;86'
    }
    return '61;75;80;86;89'
}

function Invoke-CmdFile {
    param([string]$Path)

    & $Path
    if ($LASTEXITCODE -ne 0) {
        throw "Command file failed with exit code $LASTEXITCODE"
    }
}

Push-Location $ProjectRoot

Write-Host ""
Write-Host "  ===========================================" -ForegroundColor Cyan
Write-Host "    n0s-ryo-miner Windows Build" -ForegroundColor Cyan
Write-Host "  ===========================================" -ForegroundColor Cyan
Write-Host ""

$Vcvars64 = Find-Vcvars64
if (-not $Vcvars64) {
    Pop-Location
    throw "MSVC vcvars64.bat not found. Install Visual Studio Build Tools or Community with Desktop C++."
}
Write-Host "  MSVC env: $Vcvars64" -ForegroundColor Green

$VcpkgToolchain = Find-VcpkgToolchain
$HasVcpkg = $null -ne $VcpkgToolchain
if ($HasVcpkg) {
    Write-Host "  vcpkg:    $VcpkgToolchain" -ForegroundColor Green
} else {
    Write-Host "  vcpkg:    not found, building core miner only" -ForegroundColor Yellow
    Write-Host "            (MICROHTTPD/OpenSSL/hwloc disabled)" -ForegroundColor Yellow
}

Write-Host "  Preparing embedded GUI assets..." -ForegroundColor Yellow
$GuiDir = Join-Path $ProjectRoot "gui"
$OutputHpp = Join-Path (Join-Path $ProjectRoot "n0s") "http\embedded_assets.hpp"
$EmbedScript = Join-Path $ProjectRoot "scripts\embed_assets.ps1"
if (Test-Path $EmbedScript) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $EmbedScript $GuiDir $OutputHpp
    if ($LASTEXITCODE -ne 0) {
        Pop-Location
        throw "Asset embedding failed"
    }
} elseif (Test-Path $OutputHpp) {
    Write-Host "  Reusing checked-in embedded_assets.hpp" -ForegroundColor Yellow
} else {
    Pop-Location
    throw "embedded_assets.hpp is missing and no Windows embed script is available"
}

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "  Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

if ($CudaEnable -and [string]::IsNullOrWhiteSpace($CudaArch)) {
    $CudaVersion = Get-CudaVersion
    $CudaArch = Get-DefaultCudaArch -CudaVersion $CudaVersion
    if ($CudaVersion) {
        Write-Host "  CUDA toolkit version: $CudaVersion" -ForegroundColor Green
    } else {
        Write-Host "  CUDA toolkit version: unknown" -ForegroundColor Yellow
    }
}

$ConfigureLines = @(
    'cmake .. -G "NMake Makefiles" ^',
    ('  -DCMAKE_BUILD_TYPE={0} ^' -f ([string]$BuildType)),
    '  -DCMAKE_LINK_STATIC=ON ^',
    '  -DN0S_COMPILE=generic ^'
)

if ($HasVcpkg) {
    $ConfigureLines += ('  -DCMAKE_TOOLCHAIN_FILE="{0}" ^' -f ([string]$VcpkgToolchain))
    $ConfigureLines += '  -DVCPKG_TARGET_TRIPLET=x64-windows-static ^'
} else {
    $ConfigureLines += '  -DMICROHTTPD_ENABLE=OFF ^'
    $ConfigureLines += '  -DOpenSSL_ENABLE=OFF ^'
    $ConfigureLines += '  -DHWLOC_ENABLE=OFF ^'
}

if ($CudaEnable) {
    $ConfigureLines += '  -DCUDA_ENABLE=ON ^'
    $ConfigureLines += ('  -DCUDA_ARCH={0} ^' -f ([string]$CudaArch))
    Write-Host "  CUDA:     ON ($CudaArch)" -ForegroundColor Green
} else {
    $ConfigureLines += '  -DCUDA_ENABLE=OFF ^'
    Write-Host "  CUDA:     OFF" -ForegroundColor DarkGray
}

if ($OpenclEnable) {
    $ConfigureLines += '  -DOpenCL_ENABLE=ON'
    Write-Host "  OpenCL:   ON" -ForegroundColor Green
} else {
    $ConfigureLines += '  -DOpenCL_ENABLE=OFF'
    Write-Host "  OpenCL:   OFF" -ForegroundColor DarkGray
}

$NumProcs = [string](Get-CimInstance Win32_Processor | Select-Object -First 1 -ExpandProperty NumberOfLogicalProcessors)
if (-not $NumProcs) { $NumProcs = '4' }

$CmdFile = Join-Path $BuildDir "invoke-build.cmd"
$CmdLines = @(
    '@echo off',
    ('call "{0}" >nul || exit /b 1' -f ([string]$Vcvars64)),
    ('cd /d "{0}" || exit /b 1' -f ([string]$ProjectRoot)),
    ('cd /d "{0}" || exit /b 1' -f ([string]$BuildDir))
) + $ConfigureLines + @(
    'if errorlevel 1 exit /b 1',
    ('cmake --build . -j {0}' -f $NumProcs),
    'if errorlevel 1 exit /b 1'
)
Set-Content -Path $CmdFile -Value $CmdLines -Encoding Ascii

Write-Host ""
Write-Host "  Configuring and building with CMake..." -ForegroundColor Yellow
Invoke-CmdFile -Path $CmdFile

$Binary = Join-Path (Join-Path $BuildDir "bin") "n0s-ryo-miner.exe"
if (-not (Test-Path $Binary)) {
    Pop-Location
    throw "Binary not found at $Binary"
}

$SizeMb = [math]::Round(((Get-Item $Binary).Length / 1MB), 1)
Write-Host ""
Write-Host "  Build successful" -ForegroundColor Green
Write-Host "    Binary: $Binary" -ForegroundColor Cyan
Write-Host "    Size:   $SizeMb MB" -ForegroundColor Cyan
& $Binary --version
if ($LASTEXITCODE -ne 0) {
    Pop-Location
    throw "Version check failed"
}

Pop-Location
