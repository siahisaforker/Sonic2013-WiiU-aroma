Param(
    [string]$OutDir = "./out",
    [string]$Distro = ""
)

$Root = Split-Path -Parent (Resolve-Path $PSScriptRoot)
$ResolvedOut = Resolve-Path -LiteralPath $OutDir -ErrorAction SilentlyContinue | ForEach-Object { $_.ProviderPath }
if (-not $ResolvedOut) {
    $ResolvedOut = Join-Path $Root "out"
}

New-Item -ItemType Directory -Path $ResolvedOut -Force | Out-Null

$wslRoot = (wsl.exe wslpath -a "$Root").Trim()
$wslOut = (wsl.exe wslpath -a "$ResolvedOut").Trim()
$wslCommand = "cd '$wslRoot' && bash ./scripts/build_wiiu_wuhbs.sh --out-dir '$wslOut'"

Write-Host "Building Wii U WUHBs through WSL..."
if ($Distro) {
    & wsl.exe -d $Distro bash -lc $wslCommand
} else {
    & wsl.exe bash -lc $wslCommand
}

if ($LASTEXITCODE -ne 0) {
    Write-Error "WSL Wii U build failed."
    exit $LASTEXITCODE
}

Write-Host "Wii U WUHBs built in $ResolvedOut"
