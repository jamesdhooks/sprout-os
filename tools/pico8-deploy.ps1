[CmdletBinding()]
param(
  [Parameter(Mandatory)] [string] $SdRoot,
  [string] $ProfileId
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repo 'pico8\mouse-cheese'
$cart = Join-Path $source 'mouse-cheese.p8'
$campaign = Join-Path $source 'campaign.json'
$spec = Join-Path $source 'campaign-spec.json'
$manifest = Get-Content -Raw (Join-Path $source 'manifest.json') | ConvertFrom-Json

if (-not (Test-Path -LiteralPath (Join-Path $SdRoot 'Roms\PICO') -PathType Container)) {
  throw "'$SdRoot' is not an Onion card with Roms\PICO."
}
& python (Join-Path $repo 'tools\pico8_mouse_cheese_campaign.py') validate --spec $spec --campaign $campaign --cart $cart
if ($LASTEXITCODE -ne 0) { throw 'PICO-8 campaign validation failed.' }

$publicDir = Join-Path $SdRoot 'Roms\PICO\Sprout\MouseCheese'
New-Item -ItemType Directory -Force -Path $publicDir | Out-Null
Copy-Item -LiteralPath $cart -Destination (Join-Path $publicDir 'mouse-cheese.p8') -Force

$catalogueDir = Join-Path $SdRoot 'Sprout\catalogue'
$artDir = Join-Path $catalogueDir 'art'
New-Item -ItemType Directory -Force -Path $artDir | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'library-cover.png') -Destination (Join-Path $artDir 'pico8-mouse-cheese.png') -Force

$cataloguePath = Join-Path $catalogueDir 'pico8.json'
$entry = [ordered]@{
  id = $manifest.id; title = $manifest.title; cart = 'Sprout/MouseCheese/mouse-cheese.p8'
  cover = 'Sprout/catalogue/art/pico8-mouse-cheese.png'; version = $manifest.version
  profileScoped = $true
}
$catalogue = [ordered]@{ schemaVersion = 1; entries = @() }
if (Test-Path -LiteralPath $cataloguePath) { $catalogue = Get-Content -Raw $cataloguePath | ConvertFrom-Json }
$entries = @($catalogue.entries | Where-Object { $_.id -ne $entry.id }) + [pscustomobject]$entry
$catalogue.entries = $entries
$catalogue | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $cataloguePath -Encoding utf8

if ($ProfileId) {
  $bytes = [Text.Encoding]::UTF8.GetBytes($ProfileId)
  $hash = ([Security.Cryptography.SHA256]::Create().ComputeHash($bytes) | ForEach-Object { $_.ToString('x2') }) -join ''
  $profileKey = $hash.Substring(0, 16)
  $id = "sprout_mouse_cheese_v1_$profileKey"
  if ($id -notmatch '^[a-z0-9_]{1,64}$') { throw 'Generated cartdata identifier is invalid.' }
  $profileDir = Join-Path $SdRoot ".\Roms\PICO\.sprout-profiles\$profileKey"
  New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
  $profileCart = Join-Path $profileDir 'mouse-cheese.p8'
  $contents = Get-Content -Raw $cart
  if (([regex]::Matches($contents, 'cartdata\("sprout_mouse_cheese_dev"\)')).Count -ne 1) { throw 'Cart profile marker is missing or ambiguous.' }
  $contents.Replace('cartdata("sprout_mouse_cheese_dev")', ('cartdata("' + $id + '")')) | Set-Content -LiteralPath $profileCart -Encoding utf8
  Write-Host "Profile cart prepared: $profileCart"
}

Write-Host "Mouse & Cheese deployed to $publicDir"
