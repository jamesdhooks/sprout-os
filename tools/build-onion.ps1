param(
    [ValidateSet("configure", "build", "audit")]
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
$artifactRelativePath = "out/build/onion-arm/launcher/sprout-onion-check"
$artifactPath = Join-Path $repoRoot "out\build\onion-arm\launcher\sprout-onion-check"

function Get-Sha256 {
    param([Parameter(Mandatory = $true)][string]$Path)

    $stream = [System.IO.File]::OpenRead($Path)
    try {
        $sha256 = [System.Security.Cryptography.SHA256]::Create()
        try {
            $hashBytes = $sha256.ComputeHash($stream)
            return [System.BitConverter]::ToString($hashBytes).Replace("-", "").ToLowerInvariant()
        } finally {
            $sha256.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

function Invoke-ArtifactAudit {
    if (-not [System.IO.File]::Exists($artifactPath)) {
        throw "Onion artifact was not found at $artifactPath. Build it before auditing."
    }

    $auditDockerArguments = @(
        "run", "--rm", "--platform", "linux/amd64",
        "--volume", "${repoRoot}:/root/workspace:ro",
        "--workdir", "/root/workspace",
        $toolchainImage
    )
    $readelf = "/opt/miyoomini-toolchain/bin/arm-linux-gnueabihf-readelf"

    $format = (& docker @auditDockerArguments file $artifactRelativePath | Out-String).Trim()
    if ($LASTEXITCODE -ne 0) {
        throw "Onion artifact format inspection failed with exit code $LASTEXITCODE."
    }
    if ($format -notmatch "ELF 32-bit LSB executable, ARM, EABI5" -or
        $format -notmatch "stripped") {
        throw "Onion artifact is not a stripped 32-bit ARM EABI5 executable."
    }

    $programHeaders = (& docker @auditDockerArguments $readelf -l $artifactRelativePath | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "Onion artifact program-header inspection failed with exit code $LASTEXITCODE."
    }
    $interpreterMatch = [regex]::Match(
        $programHeaders,
        "Requesting program interpreter: ([^\]]+)"
    )
    if (-not $interpreterMatch.Success -or
        $interpreterMatch.Groups[1].Value -ne "/lib/ld-linux-armhf.so.3") {
        throw "Onion artifact does not use the reviewed ARM hard-float interpreter."
    }

    $dynamic = (& docker @auditDockerArguments $readelf -d $artifactRelativePath | Out-String)
    if ($LASTEXITCODE -ne 0) {
        throw "Onion artifact dependency inspection failed with exit code $LASTEXITCODE."
    }
    $needed = @(
        [regex]::Matches($dynamic, "Shared library: \[([^\]]+)\]") |
            ForEach-Object { $_.Groups[1].Value }
    )
    $expectedLibraries = @(
        "libpthread.so.0",
        "libm.so.6",
        "libc.so.6",
        "ld-linux-armhf.so.3"
    )
    $unexpected = @($needed | Where-Object { $_ -notin $expectedLibraries })
    $missing = @($expectedLibraries | Where-Object { $_ -notin $needed })
    if ($unexpected.Count -ne 0 -or $missing.Count -ne 0 -or
        $needed.Count -ne $expectedLibraries.Count) {
        throw "Onion artifact shared dependencies differ from the reviewed set. Found: $($needed -join ', ')."
    }

    $artifact = Get-Item -LiteralPath $artifactPath
    Write-Output "Sprout Onion artifact audit passed"
    Write-Output "path: $artifactRelativePath"
    Write-Output "size-bytes: $($artifact.Length)"
    Write-Output "sha256: $(Get-Sha256 -Path $artifactPath)"
    Write-Output "format: $format"
    Write-Output "interpreter: $($interpreterMatch.Groups[1].Value)"
    Write-Output "needed: $($needed -join ', ')"
}

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "Docker is required for the pinned Onion cross-build."
}

if ($Action -eq "audit") {
    Invoke-ArtifactAudit
    return
}

New-Item -ItemType Directory -Force -Path $toolRoot | Out-Null
if (-not (Test-Path -LiteralPath $cmakeArchive)) {
    $uri = "https://github.com/Kitware/CMake/releases/download/v$cmakeVersion/$cmakeArchiveName"
    Invoke-WebRequest -Uri $uri -OutFile $cmakeArchive
}

$actualHash = Get-Sha256 -Path $cmakeArchive
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
    Invoke-ArtifactAudit
}
