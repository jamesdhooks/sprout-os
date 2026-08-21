[CmdletBinding()]
param(
  [Parameter(Mandatory)] [string] $SdRoot,
  [string] $ProfileId
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$games = @(
  [ordered]@{ Source = 'mouse-cheese'; DeviceDir = 'MouseCheese'; Cart = 'mouse-cheese.p8'; DevSave = 'sprout_mouse_cheese_dev'; Art = 'pico8-mouse-cheese.png' },
  [ordered]@{ Source = 'blocks-buttons'; DeviceDir = 'BlocksButtons'; Cart = 'blocks-buttons.p8'; DevSave = 'sprout_blocks_buttons_dev'; Art = 'pico8-blocks-buttons.png' },
  [ordered]@{ Source = 'snake'; DeviceDir = 'Snake'; Cart = 'snake.p8'; DevSave = 'sprout_snake_dev'; Art = 'pico8-snake.png' }
)

if (-not (Test-Path -LiteralPath (Join-Path $SdRoot 'Roms\PICO') -PathType Container)) {
  throw "'$SdRoot' is not an Onion card with Roms\PICO."
}

$mouse = Join-Path $repo 'pico8\mouse-cheese'
& python (Join-Path $repo 'tools\pico8_mouse_cheese_campaign.py') validate `
  --spec (Join-Path $mouse 'campaign-spec.json') `
  --campaign (Join-Path $mouse 'campaign.json') `
  --cart (Join-Path $mouse 'mouse-cheese.p8')
if ($LASTEXITCODE -ne 0) { throw 'Mouse & Cheese campaign validation failed.' }

& python (Join-Path $repo 'tools\pico8_arcade_build.py') validate `
  (Join-Path $repo 'pico8\blocks-buttons\blocks-buttons.p8') `
  (Join-Path $repo 'pico8\snake\snake.p8')
if ($LASTEXITCODE -ne 0) { throw 'Shared PICO arcade validation failed.' }

$catalogueDir = Join-Path $SdRoot 'Sprout\catalogue'
$artDir = Join-Path $catalogueDir 'art'
New-Item -ItemType Directory -Force -Path $artDir | Out-Null
$cataloguePath = Join-Path $catalogueDir 'pico8.json'
$catalogue = [ordered]@{ schemaVersion = 1; entries = @() }
if (Test-Path -LiteralPath $cataloguePath) {
  $catalogue = Get-Content -Raw $cataloguePath | ConvertFrom-Json
}

$profileKey = $null
if ($ProfileId) {
  $bytes = [Text.Encoding]::UTF8.GetBytes($ProfileId)
  $hash = ([Security.Cryptography.SHA256]::Create().ComputeHash($bytes) |
    ForEach-Object { $_.ToString('x2') }) -join ''
  $profileKey = $hash.Substring(0, 16)
}

foreach ($game in $games) {
  $source = Join-Path $repo ("pico8\" + $game.Source)
  $cart = Join-Path $source $game.Cart
  $manifest = Get-Content -Raw (Join-Path $source 'manifest.json') | ConvertFrom-Json
  $publicDir = Join-Path $SdRoot ("Roms\PICO\Sprout\" + $game.DeviceDir)
  New-Item -ItemType Directory -Force -Path $publicDir | Out-Null
  Copy-Item -LiteralPath $cart -Destination (Join-Path $publicDir $game.Cart) -Force
  Copy-Item -LiteralPath (Join-Path $source 'library-cover.png') `
    -Destination (Join-Path $artDir $game.Art) -Force

  $entry = [ordered]@{
    id = $manifest.id
    title = $manifest.title
    cart = "Sprout/$($game.DeviceDir)/$($game.Cart)"
    cover = "Sprout/catalogue/art/$($game.Art)"
    version = $manifest.version
    profileScoped = $true
  }
  $catalogue.entries = @($catalogue.entries | Where-Object { $_.id -ne $entry.id }) +
    [pscustomobject]$entry

  if ($profileKey) {
    $saveId = $manifest.saveTemplate.Replace('{profileHash}', $profileKey)
    if ($saveId -notmatch '^[a-z0-9_]{1,64}$') {
      throw "Generated cartdata identifier for '$($manifest.id)' is invalid."
    }
    $profileDir = Join-Path $SdRoot ".\Roms\PICO\.sprout-profiles\$profileKey"
    New-Item -ItemType Directory -Force -Path $profileDir | Out-Null
    $contents = Get-Content -Raw $cart
    $saveFunction = if ($game.Source -eq 'mouse-cheese') { 'cartdata' } else { 'arc_boot' }
    $marker = $saveFunction + '("' + $game.DevSave + '")'
    if (([regex]::Matches($contents, [regex]::Escape($marker))).Count -ne 1) {
      throw "Cart profile marker for '$($manifest.id)' is missing or ambiguous."
    }
    $profileCart = Join-Path $profileDir $game.Cart
    $contents.Replace($marker, ($saveFunction + '("' + $saveId + '")')) |
      Set-Content -LiteralPath $profileCart -Encoding utf8
    Write-Host "Profile cart prepared: $profileCart"
  }

  Write-Host "$($manifest.title) deployed to $publicDir"
}

$catalogue | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $cataloguePath -Encoding utf8
