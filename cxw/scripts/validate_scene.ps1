param(
    [string]$Scene = ".\cxw\data\cxw_demo_scene.bds.json",
    [string]$Report = ""
)

$ErrorActionPreference = "Stop"

function Add-Error {
    param([System.Collections.Generic.List[string]]$Errors, [string]$Message)
    [void]$Errors.Add($Message)
}

function Is-SortedTicks {
    param($Automation)
    $last = [Int64]::MinValue
    foreach ($kf in $Automation) {
        if ([Int64]$kf.time_ticks -lt $last) {
            return $false
        }
        $last = [Int64]$kf.time_ticks
    }
    return $true
}

$resolvedScene = Resolve-Path -LiteralPath $Scene
$json = Get-Content -LiteralPath $resolvedScene -Raw | ConvertFrom-Json
$errors = [System.Collections.Generic.List[string]]::new()

if ($json.sample_rate -ne 48000) {
    Add-Error $errors "sample_rate must be 48000"
}
if ($json.timebase -ne 48000) {
    Add-Error $errors "timebase must be 48000"
}
if (-not $json.codec_configs -or $json.codec_configs.Count -lt 1) {
    Add-Error $errors "codec_configs must not be empty"
}

$codecIds = @{}
foreach ($config in $json.codec_configs) {
    $codecIds[[int]$config.codec_config_id] = $true
    if ($config.codec_id -ne "opus") {
        Add-Error $errors "codec_config_id=$($config.codec_config_id) must use opus"
    }
    if ($config.sample_rate -ne 48000) {
        Add-Error $errors "codec_config_id=$($config.codec_config_id) sample_rate must be 48000"
    }
    if ($config.frame_duration_ticks -ne 480 -and $config.frame_duration_ticks -ne 960) {
        Add-Error $errors "codec_config_id=$($config.codec_config_id) frame_duration_ticks must be 480 or 960"
    }
}

$trackIds = @{}
$elementIdsFromTracks = @{}
foreach ($track in $json.tracks) {
    $id = [int]$track.track_id
    if ($trackIds.ContainsKey($id)) {
        Add-Error $errors "duplicate track_id=$id"
    }
    $trackIds[$id] = $true
    $elementIdsFromTracks[[int]$track.element_id] = $true
    if (-not $codecIds.ContainsKey([int]$track.codec_config_id)) {
        Add-Error $errors "track_id=$id references missing codec_config_id=$($track.codec_config_id)"
    }
    if ($track.channel_count -ne 2) {
        Add-Error $errors "track_id=$id channel_count should be 2 for this demo"
    }
}

$elementIds = @{}
foreach ($element in $json.elements) {
    $eid = [int]$element.element_id
    if ($elementIds.ContainsKey($eid)) {
        Add-Error $errors "duplicate element_id=$eid"
    }
    $elementIds[$eid] = $true
    if ($element.type -ne "object") {
        Add-Error $errors "element_id=$eid must be type=object in this MVP"
    }
    foreach ($tid in $element.track_ids) {
        if (-not $trackIds.ContainsKey([int]$tid)) {
            Add-Error $errors "element_id=$eid references missing track_id=$tid"
        }
    }
    if (-not (Is-SortedTicks $element.automation)) {
        Add-Error $errors "element_id=$eid automation time_ticks must be sorted"
    }
    foreach ($kf in $element.automation) {
        $p = $kf.position
        if ($p.azimuth_deg -lt -180 -or $p.azimuth_deg -ge 180) {
            Add-Error $errors "element_id=$eid has azimuth out of [-180, 180)"
        }
        if ($p.elevation_deg -lt -90 -or $p.elevation_deg -gt 90) {
            Add-Error $errors "element_id=$eid has elevation out of [-90, 90]"
        }
        if ($p.distance -lt 0) {
            Add-Error $errors "element_id=$eid has negative distance"
        }
    }
}

foreach ($eid in $elementIdsFromTracks.Keys) {
    if (-not $elementIds.ContainsKey([int]$eid)) {
        Add-Error $errors "track references missing element_id=$eid"
    }
}

foreach ($mix in $json.mix_presentations) {
    foreach ($eid in $mix.element_ids) {
        if (-not $elementIds.ContainsKey([int]$eid)) {
            Add-Error $errors "mix_id=$($mix.mix_id) references missing element_id=$eid"
        }
    }
}

$reportObject = [ordered]@{
    scene = $resolvedScene.Path
    passed = ($errors.Count -eq 0)
    checked_at = (Get-Date).ToString("s")
    checks = [ordered]@{
        sample_rate_48000 = ($json.sample_rate -eq 48000)
        timebase_48000 = ($json.timebase -eq 48000)
        codec_configs = $json.codec_configs.Count
        tracks = $json.tracks.Count
        elements = $json.elements.Count
        mix_presentations = $json.mix_presentations.Count
    }
    errors = $errors
}

if ($Report -ne "") {
    $reportJson = $reportObject | ConvertTo-Json -Depth 12
    $reportPath = Split-Path -Parent $Report
    if ($reportPath -ne "") {
        New-Item -ItemType Directory -Force -Path $reportPath | Out-Null
    }
    Set-Content -LiteralPath $Report -Value $reportJson -Encoding UTF8
}

if ($errors.Count -eq 0) {
    Write-Host "validate_scene: PASS"
    exit 0
}

Write-Host "validate_scene: FAIL"
foreach ($errorMessage in $errors) {
    Write-Host " - $errorMessage"
}
exit 1
