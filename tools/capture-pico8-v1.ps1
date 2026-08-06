param(
    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,
    [string]$Pico8
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
$outputRoot = [System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $outputRoot | Out-Null

$matrix = [ordered]@{
    "mouse-cheese" = @("title", "gameplay", "hint", "win", "middle-gameplay", "late-gameplay", "reset-holding", "reset-cancelled", "reset-complete")
    "blocks-buttons" = @("title", "gameplay", "undo", "late-gameplay", "win", "fail", "reset-holding", "reset-cancelled", "reset-complete")
    "snake" = @("title", "gameplay", "endless-gameplay", "win", "fail", "reset-holding", "reset-cancelled", "reset-complete")
}

$records = [System.Collections.Generic.List[object]]::new()
Push-Location $repoRoot
try {
    foreach ($game in $matrix.Keys) {
        $gameDirectory = Join-Path $outputRoot $game
        New-Item -ItemType Directory -Force -Path $gameDirectory | Out-Null
        foreach ($state in $matrix[$game]) {
            $destination = Join-Path $gameDirectory "$state.png"
            $arguments = @("tools/pico8_game.py", "capture", $game, "--state", $state, "--output", $destination)
            if ($Pico8) { $arguments += @("--pico8", $Pico8) }
            & python @arguments
            if ($LASTEXITCODE -ne 0) { throw "PICO capture failed for $game/$state." }
            $records.Add([pscustomobject]@{ game = $game; state = $state; file = "$game/$state.png" })
        }
    }
} finally {
    Pop-Location
}

$records | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath (Join-Path $outputRoot "coverage.json") -Encoding utf8
Write-Host "Captured $($records.Count) PICO-8 views to $outputRoot"
