param(
    [string]$OutputDirectory = (Join-Path $PSScriptRoot "..\.local-work\visual-validation"),
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
$launcher = Join-Path $repoRoot "out\build\windows-ninja-x64\launcher\Debug\sprout-launcher.exe"
$runtime = Join-Path $repoRoot "out\build\windows-ninja-x64\runtime\Debug\sprout-runtime.exe"

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "dev.ps1") -Action build
    if ($LASTEXITCODE -ne 0) { throw "Sprout build failed with exit code $LASTEXITCODE." }
}
foreach ($executable in @($launcher, $runtime)) {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "Required executable was not found at $executable."
    }
}

$launcherDirectory = Join-Path $outputRoot "launcher"
$gameDirectory = Join-Path $outputRoot "games"
$fixtureDirectory = Join-Path $outputRoot ".fixtures"
$importsDirectory = Join-Path $fixtureDirectory "imports"
$storageDirectory = Join-Path $fixtureDirectory "storage"
New-Item -ItemType Directory -Force -Path $launcherDirectory, $gameDirectory, $importsDirectory, $storageDirectory | Out-Null
Copy-Item -LiteralPath (Join-Path $repoRoot "launcher\assets\avatars\masters\explorer-fox.png") `
    -Destination (Join-Path $importsDirectory "profile-image.png") -Force

$launcherScreens = @(
    "startup",
    "setup-welcome", "setup-locale", "setup-network", "setup-parent", "setup-pin",
    "setup-child", "setup-avatars", "setup-avatars-import", "setup-library",
    "setup-child-defaults", "setup-connectors", "setup-review", "setup-complete",
    "recovery-home", "recovery-confirm-restore", "recovery-confirm-reset",
    "pin-create", "pin-auth", "pin-auth-failed",
    "profile-select", "child-home", "parent-home",
    "library-recent", "library-favorites", "library-all", "library-arcade",
    "library-empty", "library-unavailable", "profile-settings",
    "profile-appearance", "profile-backgrounds",
    "profile-avatars-page-01", "profile-avatars-page-02", "profile-avatars-page-03",
    "profile-avatars-page-04", "profile-avatars-page-05", "profile-avatars-page-06",
    "profile-avatars-page-07", "profile-avatars-page-08", "profile-avatars-page-09",
    "profile-image-crop", "profile-image-result",
    "profile-archive-home", "profile-archive-export",
    "profile-archive-confirm-portrait", "profile-archive-restore"
)

$previousVideoDriver = $env:SDL_VIDEODRIVER
$previousRenderDriver = $env:SDL_RENDER_DRIVER
$previousStaticUi = $env:SPROUT_STATIC_UI
$env:SDL_VIDEODRIVER = "dummy"
$env:SDL_RENDER_DRIVER = "software"
$env:SPROUT_STATIC_UI = "1"
$records = [System.Collections.Generic.List[object]]::new()
try {
    foreach ($screen in $launcherScreens) {
        $path = Join-Path $launcherDirectory "$screen.bmp"
        & $launcher --screenshot $path $screen --data-dir $fixtureDirectory `
            --arcade-root (Join-Path $repoRoot "games") --runtime $runtime
        if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $path -PathType Leaf)) {
            throw "Launcher capture failed for $screen."
        }
        $records.Add([pscustomobject]@{ area = "launcher"; view = $screen; status = "captured"; file = "launcher/$screen.bmp" })
    }

    $games = @(
        @{ id = "snake"; terminal = @("fail") },
        @{ id = "mouse-maze"; terminal = @("win") },
        @{ id = "blocks-buttons"; terminal = @("win", "fail") }
    )
    foreach ($game in $games) {
        $package = Join-Path $repoRoot "games\$($game.id)"
        $destination = Join-Path $gameDirectory $game.id
        New-Item -ItemType Directory -Force -Path $destination | Out-Null
        $titlePath = Join-Path $destination "title.bmp"
        & $runtime --package $package --storage (Join-Path $storageDirectory $game.id) --seed 7 --capture-title $titlePath
        if ($LASTEXITCODE -ne 0) { throw "Title capture failed for $($game.id)." }
        $records.Add([pscustomobject]@{ area = $game.id; view = "title"; status = "captured"; file = "games/$($game.id)/title.bmp" })

        foreach ($state in @("gameplay") + $game.terminal) {
            $path = Join-Path $destination "$state.bmp"
            & $runtime --package $package --storage (Join-Path $storageDirectory $game.id) --seed 7 --capture $path --capture-state $state
            if ($LASTEXITCODE -ne 0) { throw "State capture failed for $($game.id)/$state." }
            $records.Add([pscustomobject]@{ area = $game.id; view = $state; status = "captured"; file = "games/$($game.id)/$state.bmp" })
        }
    }
} finally {
    $env:SDL_VIDEODRIVER = $previousVideoDriver
    $env:SDL_RENDER_DRIVER = $previousRenderDriver
    $env:SPROUT_STATIC_UI = $previousStaticUi
}

$records.Add([pscustomobject]@{ area = "snake"; view = "win"; status = "not-applicable"; reason = "Snake is endless; it has no win condition." })
$records.Add([pscustomobject]@{ area = "mouse-maze"; view = "fail"; status = "not-applicable"; reason = "Mouse Maze has no loss condition." })
$records.Add([pscustomobject]@{ area = "runtime"; view = "pause-overlay"; status = "not-implemented"; reason = "The runtime currently emits pause lifecycle events but has no visual pause overlay." })
$records | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot "coverage.json") -Encoding utf8

$captured = $records | Where-Object status -eq "captured"
$html = [System.Text.StringBuilder]::new()
[void]$html.AppendLine('<!doctype html><html><head><meta charset="utf-8"><title>Sprout visual validation</title>')
[void]$html.AppendLine('<style>body{font:16px system-ui;background:#10241d;color:#f8f3dc;margin:24px}h2{margin-top:42px}.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(330px,1fr));gap:18px}.card{background:#18372b;padding:12px;border-radius:14px}.card img{width:100%;image-rendering:auto;border-radius:8px}.name{font-weight:700;margin-top:8px}.note{color:#f0c66c}</style></head><body>')
[void]$html.AppendLine("<h1>Sprout visual validation</h1><p>$($captured.Count) deterministic Windows captures.</p>")
foreach ($group in ($captured | Group-Object area)) {
    [void]$html.AppendLine("<h2>$($group.Name)</h2><div class=`"grid`">")
    foreach ($record in $group.Group) {
        [void]$html.AppendLine("<div class=`"card`"><img src=`"$($record.file)`" alt=`"$($record.view)`"><div class=`"name`">$($record.view)</div></div>")
    }
    [void]$html.AppendLine('</div>')
}
[void]$html.AppendLine('<h2>Explicit gaps</h2><ul>')
foreach ($record in ($records | Where-Object status -ne "captured")) {
    [void]$html.AppendLine("<li><span class=`"note`">$($record.area) / $($record.view): $($record.status)</span> - $($record.reason)</li>")
}
[void]$html.AppendLine('</ul></body></html>')
$html.ToString() | Set-Content -LiteralPath (Join-Path $outputRoot "index.html") -Encoding utf8
Write-Output "Captured $($captured.Count) views to $outputRoot"
