param(
    [ValidateSet("configure", "build")]
    [string]$Action = "build"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$toolRoot = Join-Path $repoRoot "out\toolchains"
$cmakeVersion = "3.31.12"
$cmakeArchiveName = "cmake-$cmakeVersion-linux-x86_64.tar.gz"
$cmakeArchive = Join-Path $toolRoot $cmakeArchiveName
$cmakeDirectory = Join-Path $toolRoot "cmake-$cmakeVersion-linux-x86_64"
$cmakeHash = "0dc2e9a6860f06bf10bd8fadc03e35d9eeb4df46e33763a7e480e987758f385c"
$toolchainImage = "aemiii91/miyoomini-toolchain@sha256:a8da1021449c80c0ccb75e263f1dfc75b5a004278fefa8a54151e55698a352f4"

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "Docker is required for the pinned Onion cross-build."
}

New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
if (-not (Test-Path -LiteralPath $cmakeArchive)) {
    $uri = "https://github.com/Kitware/CMake/releases/download/v$cmakeVersion/$cmakeArchiveName"
    Invoke-WebRequest -Uri $uri -OutFile $cmakeArchive
}

$stream = [System.IO.File]::OpenRead($cmakeArchive)
try {
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash($stream)
        $actualHash = [System.BitConverter]::ToString($hashBytes).Replace("-", "").ToLowerInvariant()
    } finally {
        $sha256.Dispose()
    }
} finally {
    $stream.Dispose()
}
if ($actualHash -ne $cmakeHash) {
    throw "Downloaded CMake archive failed SHA-256 verification."
}

$cmakeExecutable = Join-Path $cmakeDirectory "bin/cmake"
if (-not (Test-Path -LiteralPath $cmakeExecutable)) {
    $tarCommand = if ([System.Environment]::OSVersion.Platform -eq "Win32NT") {
        "tar.exe"
    } else {
        "tar"
    }
    & $tarCommand -xzf $cmakeArchive -C $toolRoot
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $cmakeExecutable)) {
        throw "CMake extraction failed."
    }
}

$dockerArguments = @(
    "run", "--rm", "--platform", "linux/amd64",
    "--volume", "${repoRoot}:/root/workspace",
    "--volume", "${cmakeDirectory}:/opt/sprout-cmake:ro",
    "--workdir", "/root/workspace",
    $toolchainImage
)

& docker @dockerArguments /opt/sprout-cmake/bin/cmake --preset onion-arm
if ($LASTEXITCODE -ne 0) {
    throw "Onion CMake configuration failed with exit code $LASTEXITCODE."
}

if ($Action -eq "build") {
    & docker @dockerArguments /opt/sprout-cmake/bin/cmake --build --preset onion-release
    if ($LASTEXITCODE -ne 0) {
        throw "Onion cross-build failed with exit code $LASTEXITCODE."
    }
}
