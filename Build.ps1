# --------------------------------------
# Sonic 2013 Wii U Fully Automated Build Script
# --------------------------------------

# --- CONFIG ---
$UserProjectFolder = "$env:USERPROFILE\sonic2013-wiiu"
$DevkitProFolder   = "C:\devkitPro\wut"
$SDL2Folder        = "$DevkitProFolder\sdl2"
$VorbisFolder      = "$DevkitProFolder\vorbis"
$RepoURL           = "https://gitlab.com/QuarkTheAwesome/sonic2013-wiiu.git"

# --- Ensure Git ---
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "Git not found. Installing Git..."
    $gitInstaller = "$env:TEMP\Git-Installer.exe"
    Invoke-WebRequest -Uri "https://github.com/git-for-windows/git/releases/latest/download/Git-2.42.0-64-bit.exe" -OutFile $gitInstaller
    Start-Process -FilePath $gitInstaller -ArgumentList "/SILENT" -Wait
    Remove-Item $gitInstaller
}

# --- Ensure 7zip ---
if (-not (Get-Command 7z -ErrorAction SilentlyContinue)) {
    Write-Host "7zip not found. Please install 7zip and add it to PATH."
    exit
}

# --- Ensure DevkitPro & WUT ---
if (-not (Test-Path $DevkitProFolder)) {
    Write-Host "DevkitPro/WUT not found. Launching installer..."
    Start-Process "https://devkitpro.org/wiki/Getting_Started#Windows" -Wait
    exit
}

# --- Clone Sonic2013 repo if missing ---
if (-not (Test-Path $UserProjectFolder)) {
    Write-Host "Cloning Sonic2013 repository..."
    git clone $RepoURL $UserProjectFolder
}

# --- Select Game ---
Write-Host "Select game:"
Write-Host "1) Sonic 1"
Write-Host "2) Sonic 2"
$gameChoice = Read-Host "Enter 1 or 2"
switch ($gameChoice) {
    "1" { $GameFolder = "Sonic 1" }
    "2" { $GameFolder = "Sonic 2" }
    default { Write-Host "Invalid selection"; exit }
}

# --- Patch Files ---
$PatchFolder = "$UserProjectFolder\patched"
if (-not (Test-Path $PatchFolder)) { New-Item -ItemType Directory -Path $PatchFolder -Force }

# Replace hpp/cpp files
$FilesToPatch = @("Audio.hpp","Input.hpp","Debug.hpp")
foreach ($f in $FilesToPatch) {
    $srcFile = "$UserProjectFolder\RSDKv4\$f"
    $patchFile = "$PatchFolder\$f"
    if (Test-Path $patchFile) {
        Copy-Item -Path $patchFile -Destination $srcFile -Force
    } else {
        Write-Host "$f patch missing! Exiting."
        exit
    }
}

# --- Download & compile SDL2 ---
$SDL2Zip = "$SDL2Folder\SDL2.zip"
if (-not (Test-Path "$SDL2Folder\SDL2-2.0.20")) {
    Invoke-WebRequest "https://www.libsdl.org/release/SDL2-2.0.20.zip" -OutFile $SDL2Zip
    [System.IO.Compression.ZipFile]::ExtractToDirectory($SDL2Zip, $SDL2Folder)
    Remove-Item $SDL2Zip
}
Push-Location "$SDL2Folder\SDL2-2.0.20"
$sdlFiles = Get-ChildItem -Recurse -Include *.c
foreach ($f in $sdlFiles) {
    & "$DevkitProFolder\devkitPPC\bin\powerpc-eabi-gcc" -O2 -g -c $f.FullName -I"$DevkitProFolder\wut\include"
    if ($LASTEXITCODE -ne 0) { throw "Failed to compile $($f.Name)" }
}
$sdlObjs = Get-ChildItem -Recurse -Include *.o | ForEach-Object { $_.FullName }
& "$DevkitProFolder\devkitPPC\bin\powerpc-eabi-ar" rcs libSDL2.a $sdlObjs
Pop-Location

# --- Download & compile Vorbis ---
$VorbisTar = "$VorbisFolder\vorbis.tar.gz"
if (-not (Test-Path "$VorbisFolder\vorbis-1.3.7")) {
    Invoke-WebRequest "https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.gz" -OutFile $VorbisTar
    7z x $VorbisTar -o$VorbisFolder -y | Out-Null
    7z x "$VorbisFolder\libvorbis-1.3.7.tar" -o$VorbisFolder -y | Out-Null
    Remove-Item $VorbisTar
    Remove-Item "$VorbisFolder\libvorbis-1.3.7.tar"
}
Push-Location "$VorbisFolder\vorbis-1.3.7"
$vorbisFiles = @("analysis.c","bitrate.c","block.c","codebook.c","envelope.c","floor0.c","floor1.c","icore.c","lookup.c","lsp.c","mapping0.c","mdct.c","misc.c","res0.c","synthesis.c","vorbisfile.c")
foreach ($vf in $vorbisFiles) {
    $fullPath = Join-Path $VorbisFolder "vorbis-1.3.7\$vf"
    if (-not (Test-Path $fullPath)) { throw "$vf missing" }
    & "$DevkitProFolder\devkitPPC\bin\powerpc-eabi-gcc" -O2 -g -c $fullPath -I"$DevkitProFolder\wut\include"
    if ($LASTEXITCODE -ne 0) { throw "Failed to compile $vf" }
}
$vorbisObjs = Get-ChildItem -Recurse -Include *.o | ForEach-Object { $_.FullName }
& "$DevkitProFolder\devkitPPC\bin\powerpc-eabi-ar" rcs libvorbis.a $vorbisObjs
Pop-Location

# --- Build Sonic2013 ---
Push-Location $UserProjectFolder
& make -f Makefile.wiiu
if ($LASTEXITCODE -ne 0) { throw "Failed to build Sonic2013_WiiU.elf" }
Pop-Location

# --- Copy Data.rsdk ---
$DataPath = Read-Host "Enter path to your Data.rsdk"
$DestPath = Join-Path $UserProjectFolder $GameFolder
if (-not (Test-Path $DestPath)) { New-Item -ItemType Directory -Path $DestPath -Force }
Copy-Item $DataPath -Destination $DestPath -Force

# --- Final Verification ---
Write-Host "`n=== VERIFICATION ==="
$checks = @(
    "$UserProjectFolder\RSDKv4\Audio.hpp",
    "$UserProjectFolder\RSDKv4\Input.hpp",
    "$UserProjectFolder\RSDKv4\Debug.hpp",
    "$SDL2Folder\SDL2-2.0.20",
    "$VorbisFolder\vorbis-1.3.7",
    "$UserProjectFolder\$GameFolder\Data.rsdk",
    "$UserProjectFolder\Sonic2013_WiiU.elf"
)
foreach ($c in $checks) {
    if (-not (Test-Path $c)) { Write-Host "MISSING: $c" }
    else { Write-Host "OK: $c" }
}
Write-Host "Build and setup completed successfully!"
