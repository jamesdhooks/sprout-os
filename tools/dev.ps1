param(
    [ValidateSet("configure", "build", "test", "run", "run-arcade", "arcade-smoke")]
    [string]$Action = "build",
    [string]$SdRoot,
    [string]$ArcadePackage = "games\snake"
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

function Invoke-ArcadeSmoke {
    param(
        [Parameter(Mandatory = $true)][string]$Launcher,
        [Parameter(Mandatory = $true)][string]$Runtime,
        [Parameter(Mandatory = $true)][string]$PackagesRoot,
        [Parameter(Mandatory = $true)][string]$DataRoot,
        [Parameter(Mandatory = $true)][string]$ExpectedItemId,
        [string]$ExpectedEvent,
        [string]$ExpectedStoragePath
    )

    $output = & $Launcher --arcade-smoke-test `
        --arcade-smoke-item $ExpectedItemId `
        --data-dir $DataRoot `
        --arcade-root $PackagesRoot `
        --runtime $Runtime 2>&1
    $status = $LASTEXITCODE
    $text = $output -join "`n"
    $output | Write-Output
    if ($status -ne 0) {
        throw "Arcade smoke test failed with exit code $status."
    }
    if ($text -notmatch 'SPROUT_EVENT\s+GameStarted\s+fresh' -or
        $text -notmatch 'SPROUT_EVENT\s+GameExited\s+normal' -or
        $text -notmatch "arcade smoke result: $([regex]::Escape($ExpectedItemId)) outcome=0 exit=0") {
        throw "Arcade smoke test did not report the required start, exit, and launcher-return evidence."
    }
    if ($ExpectedEvent -and $text -notmatch [regex]::Escape($ExpectedEvent)) {
        throw "Arcade smoke test did not report expected event $ExpectedEvent."
    }
    if ($ExpectedStoragePath) {
        if (-not (Test-Path -LiteralPath $ExpectedStoragePath -PathType Leaf)) {
            throw "Arcade smoke test did not create expected storage at $ExpectedStoragePath."
        }
        $stored = Get-Content -Raw -LiteralPath $ExpectedStoragePath | ConvertFrom-Json
        if ($stored.'smoke-written' -ne 1) {
            throw "Arcade smoke storage did not contain the expected fixture value."
        }
    }
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
        $launcher = Join-Path $repoRoot "out\build\windows-ninja-x64\launcher\Debug\sprout-launcher.exe"
        $runtime = Join-Path $repoRoot "out\build\windows-ninja-x64\runtime\Debug\sprout-runtime.exe"
        Invoke-ArcadeSmoke `
            -Launcher $launcher `
            -Runtime $runtime `
            -PackagesRoot (Join-Path $repoRoot "games") `
            -DataRoot (Join-Path $repoRoot "out\arcade-smoke-data") `
            -ExpectedItemId "arcade:sprout.snake"
        $fixtureData = Join-Path $repoRoot "out\arcade-lifecycle-data"
        Invoke-ArcadeSmoke `
            -Launcher $launcher `
            -Runtime $runtime `
            -PackagesRoot (Join-Path $repoRoot "launcher\tests\fixtures\arcade") `
            -DataRoot $fixtureData `
            -ExpectedItemId "arcade:sprout.lifecycle-fixture" `
            -ExpectedEvent "AchievementUnlocked`tlifecycle-smoke" `
            -ExpectedStoragePath (Join-Path $fixtureData "data\native-games\diagnostic-child\sprout.lifecycle-fixture\storage.json")
        return
    }

    if ($Action -eq "run") {
        $executable = Join-Path $repoRoot "out\build\windows-ninja-x64\launcher\Debug\sprout-launcher.exe"
        if (-not (Test-Path -LiteralPath $executable)) {
            throw "Launcher executable was not found at $executable."
        }
        $launcherArguments = @("--data-dir", (Join-Path $repoRoot "out\preview-data"))
        $launcherArguments += @(
            "--arcade-root", (Join-Path $repoRoot "games"),
            "--runtime", (Join-Path $repoRoot "out\build\windows-ninja-x64\runtime\Debug\sprout-runtime.exe")
        )
        if ($SdRoot) {
            $launcherArguments += @("--sd-root", $SdRoot)
        }
        & $executable @launcherArguments
    }

    if ($Action -eq "run-arcade") {
        $executable = Join-Path $repoRoot "out\build\windows-ninja-x64\runtime\Debug\sprout-runtime.exe"
        if (-not (Test-Path -LiteralPath $executable)) {
            throw "Runtime executable was not found at $executable."
        }
        $packageRoot = Join-Path $repoRoot $ArcadePackage
        if (-not (Test-Path -LiteralPath $packageRoot -PathType Container)) {
            throw "Arcade package was not found at $packageRoot."
        }
        & $executable --package $packageRoot --storage (Join-Path $repoRoot "out\arcade-preview-data")
    }

    if ($Action -eq "arcade-smoke") {
        Invoke-ArcadeSmoke `
            -Launcher (Join-Path $repoRoot "out\build\windows-ninja-x64\launcher\Debug\sprout-launcher.exe") `
            -Runtime (Join-Path $repoRoot "out\build\windows-ninja-x64\runtime\Debug\sprout-runtime.exe") `
            -PackagesRoot (Join-Path $repoRoot "games") `
            -DataRoot (Join-Path $repoRoot "out\arcade-smoke-data") `
            -ExpectedItemId "arcade:sprout.snake"
    }
} finally {
    Pop-Location
}
