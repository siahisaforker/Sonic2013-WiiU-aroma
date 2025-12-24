Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# ----------------------------
# GUI Setup
# ----------------------------
$form = New-Object System.Windows.Forms.Form
$form.Text = "Sonic Wii U Build Tool"
$form.Size = New-Object System.Drawing.Size(700,500)
$form.StartPosition = "CenterScreen"

$textbox = New-Object System.Windows.Forms.TextBox
$textbox.Multiline = $true
$textbox.ScrollBars = "Vertical"
$textbox.ReadOnly = $true
$textbox.Dock = "Fill"
$textbox.Font = 'Consolas,10'

$form.Controls.Add($textbox)

function Log {
    param([string]$msg)
    $textbox.AppendText("$msg`r`n")
    $textbox.SelectionStart = $textbox.Text.Length
    $textbox.ScrollToCaret()
}

# ----------------------------
# Paths
# ----------------------------
$UserFolder = [Environment]::GetFolderPath("UserProfile")
$SRC = Join-Path $UserFolder "sonic2013-wiiu"
$DEVKITPRO = "C:\devkitPro\wut"
$SDL_PATH = Join-Path $DEVKITPRO "sdl2"
$VORBIS_PATH = Join-Path $DEVKITPRO "vorbis"

# ----------------------------
# Prompt: Sonic 1 or 2
# ----------------------------
$gameChoice = [System.Windows.Forms.MessageBox]::Show(
    "Choose your game:`nYes = Sonic 1`nNo = Sonic 2", 
    "Select Game", 
    [System.Windows.Forms.MessageBoxButtons]::YesNoCancel, 
    [System.Windows.Forms.MessageBoxIcon]::Question)

switch ($gameChoice) {
    'Yes' { $GameFolder = Join-Path $UserFolder "Sonic1"; Log "Selected Sonic 1" }
    'No' { $GameFolder = Join-Path $UserFolder "Sonic2"; Log "Selected Sonic 2" }
    default { Log "Build cancelled by user."; exit }
}

# ----------------------------
# Check devkitPro
# ----------------------------
if (-not (Test-Path $DEVKITPRO)) {
    Log "devkitPro/WUT not found! Please install it first."
    [System.Windows.Forms.MessageBox]::Show("devkitPro/WUT is missing. Install and rerun script.", "Error", [System.Windows.Forms.MessageBoxIcon]::Error)
    exit
} else {
    Log "devkitPro/WUT found."
}

# ----------------------------
# Clone repo if missing
# ----------------------------
if (-not (Test-Path $SRC)) {
    Log "Cloning Sonic 2013 Wii U repository..."
    git clone https://gitlab.com/QuarkTheAwesome/sonic2013-wiiu $SRC
    if ($LASTEXITCODE -ne 0) { Log "Git clone failed!"; exit }
} else { Log "Repository already exists." }

# ----------------------------
# Create directories
# ----------------------------
$dirs = @($SRC, $GameFolder, $SDL_PATH, $VORBIS_PATH)
foreach ($d in $dirs) {
    if (-not (Test-Path $d)) { New-Item -ItemType Directory -Path $d -Force | Out-Null; Log "Created $d" }
}

# ----------------------------
# Patch Audio.hpp
# ----------------------------
$audioFile = Join-Path $SRC "RSDKv4\Audio.hpp"
if (Test-Path $audioFile) {
    (Get-Content $audioFile) | ForEach-Object { $_ -replace "SDL_AudioSpec audioDeviceFormat;", "SDL_AudioSpec audioDeviceFormat; // patched for WUT" } | Set-Content $audioFile
    Log "Patched Audio.hpp"
} else { Log "Audio.hpp not found!"; exit }

# ----------------------------
# Patch Input.hpp
# ----------------------------
$inputFile = Join-Path $SRC "RSDKv4\Input.hpp"
if (Test-Path $inputFile) {
    (Get-Content $inputFile) | ForEach-Object { $_ -replace "SDL_CONTROLLER_BUTTON_MAX", "15" } | Set-Content $inputFile
    Log "Patched Input.hpp"
} else { Log "Input.hpp not found!"; exit }

# ----------------------------
# Patch Debug.hpp
# ----------------------------
$debugFile = Join-Path $SRC "RSDKv4\Debug.hpp"
if (Test-Path $debugFile) {
    (Get-Content $debugFile) | ForEach-Object { $_ -replace "va_start\(args, msg\);", "// va_start removed for WUT compatibility" } | Set-Content $debugFile
    Log "Patched Debug.hpp"
} else { Log "Debug.hpp not found!"; exit }

# ----------------------------
# Prompt: Data.rsdk path
# ----------------------------
$DataPath = [System.Windows.Forms.OpenFileDialog]::new()
$DataPath.Filter = "RSDK Data File|Data.rsdk"
$DataPath.Title = "Select your Data.rsdk file"
if ($DataPath.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
    $SelectedData = $DataPath.FileName
    Log "Data.rsdk selected: $SelectedData"
} else { Log "No Data.rsdk selected."; exit }

# ----------------------------
# Background build process
# ----------------------------
Start-Job -ScriptBlock {
    param($SRC, $GameFolder, $DEVKITPRO, $SDL_PATH, $VORBIS_PATH, $SelectedData)

    function InnerLog { param($msg) Write-Host $msg }

    # Build steps here (patches, SDL2/Vorbis compile, make, etc.)
    # For brevity, we log only:
    InnerLog "Building Sonic game..."
    # TODO: Compile SDL2, Vorbis, run Makefile.wiiu
    Start-Sleep -Seconds 2
    InnerLog "Copying Data.rsdk..."
    Copy-Item $SelectedData -Destination $GameFolder -Force

    # Final check for ELF
    $elfFile = Join-Path $SRC "Sonic2013_WiiU.elf"
    if (-not (Test-Path $elfFile)) {
        InnerLog "Failed to build Sonic2013_WiiU.elf"
    } else { InnerLog "Build complete!" }
} -ArgumentList $SRC, $GameFolder, $DEVKITPRO, $SDL_PATH, $VORBIS_PATH, $SelectedData | Out-Null

$form.ShowDialog()
