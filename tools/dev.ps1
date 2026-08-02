param(
    [ValidateSet("configure", "build", "test", "run")]
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

function Find-VisualStudio {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "Visual Studio C++ tools were not found. Install Visual Studio or Build Tools with Desktop development with C++."
    }

    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installPath) {
        throw "Visual Studio C++ tools were not found."
    }
    return $installPath
}

function Initialize-Msvc {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

    $installPath = Find-VisualStudio
    $developerShell = Join-Path $installPath "Common7\Tools\VsDevCmd.bat"
    if (-not (Test-Path -LiteralPath $developerShell)) {
        throw "Visual Studio's developer shell was not found at $developerShell."
    }

    $environment = & $env:ComSpec /s /c "`"$developerShell`" -arch=amd64 -host_arch=amd64 >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio's developer shell failed with exit code $LASTEXITCODE."
    }

    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path "Env:$($matches[1])" -Value $matches[2]
        }
    }
}

function Find-CMake {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $installPath = Find-VisualStudio
    $bundledCMake = Join-Path $installPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
    if (-not (Test-Path -LiteralPath $bundledCMake)) {
        throw "Visual Studio's bundled CMake was not found at $bundledCMake."
    }
    return $bundledCMake
}

function Add-NinjaToPath {
    if (Get-Command ninja.exe -ErrorAction SilentlyContinue) {
        return
    }

    $installPath = Find-VisualStudio
    $ninja = Join-Path $installPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe"
    if (-not (Test-Path -LiteralPath $ninja)) {
        throw "Ninja was not found. Install Ninja or Visual Studio's C++ CMake tools."
    }
    $env:Path = "$(Split-Path -Parent $ninja);$env:Path"
}

Initialize-Msvc
Add-NinjaToPath
$cmake = Find-CMake
$ctest = Join-Path (Split-Path -Parent $cmake) "ctest.exe"

Push-Location $repoRoot
try {
    & $cmake --preset windows-x64
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE."
    }

    if ($Action -eq "configure") {
        return
    }

    & $cmake --build --preset windows-debug
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE."
    }

    if ($Action -eq "test") {
        & $ctest --preset windows-debug
        if ($LASTEXITCODE -ne 0) {
            throw "Tests failed with exit code $LASTEXITCODE."
        }
        return
    }

    if ($Action -eq "run") {
        $executable = Join-Path $repoRoot "out\build\windows-ninja-x64\launcher\Debug\sprout-launcher.exe"
        if (-not (Test-Path -LiteralPath $executable)) {
            throw "Launcher executable was not found at $executable."
        }
        & $executable --data-dir (Join-Path $repoRoot "out\preview-data")
    }
} finally {
    Pop-Location
}
