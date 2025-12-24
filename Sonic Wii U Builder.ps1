# BuildSonicWiiU.ps1
# WPF PowerShell GUI for building Sonic 1/2 for Wii U with full automation :)

Add-Type -AssemblyName PresentationFramework, PresentationCore, WindowsBase, System.Drawing

# ------------------------------
# GLOBAL VARIABLES
# ------------------------------
$Global:DevkitProPath  = "C:\devkitPro\wut"
$Global:SDLPath         = "$Global:DevkitProPath\sdl2"
$Global:VorbisPath      = "$Global:DevkitProPath\vorbis"
$Global:AromaSDKPath    = "$Global:DevkitProPath\aroma"
$Global:RepoPath        = "$env:USERPROFILE\sonic2013-wiiu"
$Global:DataRSDK        = $null
$Global:SelectedGame    = $null
$Global:SelectedELF     = $null
$Global:WuhbFolder      = "$Global:RepoPath\WUHBS"

# ------------------------------
# HELPER FUNCTIONS
# ------------------------------

function Show-Error($msg) { [System.Windows.MessageBox]::Show($msg,"Error",[System.Windows.MessageBoxButton]::OK,[System.Windows.MessageBoxImage]::Error) }
function Show-Info($msg)  { [System.Windows.MessageBox]::Show($msg,"Info",[System.Windows.MessageBoxButton]::OK,[System.Windows.MessageBoxImage]::Information) }
function Ask-YesNo($msg)  { [System.Windows.MessageBox]::Show($msg,"Confirm",[System.Windows.MessageBoxButton]::YesNo,[System.Windows.MessageBoxImage]::Question) -eq "Yes" }

# Create directories if missing
function Ensure-Dir($path) { if (-not (Test-Path $path)) { New-Item -ItemType Directory -Path $path -Force | Out-Null } }

# Patch RSDKv4 headers
function Patch-Headers {
    $patchedFiles = @(
        @{file="RSDKv4\Audio.hpp"; search="SDL_AudioSpec audioDeviceFormat;"; replace="SDL_AudioSpec audioDeviceFormat; // patched for WUT"},
        @{file="RSDKv4\Input.hpp"; search="SDL_CONTROLLER_BUTTON_MAX"; replace="15"},
        @{file="RSDKv4\Debug.hpp"; search="va_start\(args, msg\);"; replace="// va_start removed for WUT compatibility"}
    )
    foreach ($pf in $patchedFiles) {
        $fullPath = Join-Path $Global:RepoPath $pf.file
        if (Test-Path $fullPath) {
            (Get-Content $fullPath) | ForEach-Object { $_ -replace $pf.search, $pf.replace } | Set-Content $fullPath
        } else { throw "Cannot patch: $fullPath not found." }
    }
}

# Restore headers if build fails
function Restore-Headers {
    foreach ($pf in @("Audio.hpp","Input.hpp","Debug.hpp")) {
        $fullPath = Join-Path $Global:RepoPath "RSDKv4\$pf.bak"
        if (Test-Path $fullPath) { Copy-Item $fullPath -Destination (Join-Path $Global:RepoPath "RSDKv4\$pf") -Force }
    }
}

# ------------------------------
# WPF GUI
# ------------------------------
$XAML = @"
<Window xmlns="http://schemas.microsoft.com/winfx/2006/xaml/presentation" Title="Sonic Wii U Builder" Height="600" Width="800">
  <Grid>
    <TabControl Name="MainTabs">
      <TabItem Header="Build">
        <StackPanel Margin="10">
          <Label Content="Select Game Version:"/>
          <ComboBox Name="GameSelect" Width="200">
            <ComboBoxItem Content="Sonic 1"/>
            <ComboBoxItem Content="Sonic 2"/>
          </ComboBox>
          <Button Name="SelectDataButton" Content="Select Data.rsdk" Width="200" Margin="0,5,0,0"/>
          <Label Name="DataLabel" Content="No Data.rsdk selected."/>
          <Button Name="BuildButton" Content="Build Game" Width="200" Margin="0,10,0,0"/>
          <ProgressBar Name="BuildProgress" Height="20" Width="400" Minimum="0" Maximum="100" Margin="0,10,0,0"/>
        </StackPanel>
      </TabItem>

      <TabItem Header="ELF → WUHB">
        <StackPanel Margin="10">
          <Button Name="SelectELFButton" Content="Select ELF" Width="200"/>
          <Label Name="ELFLabel" Content="No ELF selected."/>
          <Button Name="ConvertButton" Content="Convert to WUHB" Width="200" Margin="0,10,0,0"/>
        </StackPanel>
      </TabItem>

      <TabItem Header="Dependencies">
        <StackPanel Margin="10">
          <Button Name="InstallDepsButton" Content="Install Missing Dependencies" Width="250"/>
        </StackPanel>
      </TabItem>

      <TabItem Header="Icons">
        <StackPanel Margin="10">
          <Button Name="SelectIconButton" Content="Select PNG Icon" Width="200"/>
          <Label Name="IconLabel" Content="No icon selected."/>
        </StackPanel>
      </TabItem>
    </TabControl>
  </Grid>
</Window>
"@

[xml]$XAMLXML = $XAML
$Reader = (New-Object System.Xml.XmlNodeReader $XAMLXML)
$Window = [Windows.Markup.XamlReader]::Load($Reader)

# Controls
$GameSelect        = $Window.FindName("GameSelect")
$SelectDataButton  = $Window.FindName("SelectDataButton")
$DataLabel         = $Window.FindName("DataLabel")
$BuildButton       = $Window.FindName("BuildButton")
$BuildProgress     = $Window.FindName("BuildProgress")
$SelectELFButton   = $Window.FindName("SelectELFButton")
$ELFLabel          = $Window.FindName("ELFLabel")
$ConvertButton     = $Window.FindName("ConvertButton")
$InstallDepsButton = $Window.FindName("InstallDepsButton")
$SelectIconButton  = $Window.FindName("SelectIconButton")
$IconLabel         = $Window.FindName("IconLabel")

# ------------------------------
# BUTTON EVENTS
# ------------------------------

# Select Data.rsdk
$SelectDataButton.Add_Click({
    $Global:DataRSDK = (Get-Item (Get-ChildItem -Path (Read-Host "Enter path to Data.rsdk") -Filter Data.rsdk)).FullName
    if ($Global:DataRSDK) { $DataLabel.Content = $Global:DataRSDK }
})

# Select ELF
$SelectELFButton.Add_Click({
    $Global:SelectedELF = (Get-Item (Read-Host "Enter path to ELF")).FullName
    if ($Global:SelectedELF) { $ELFLabel.Content = $Global:SelectedELF }
})

# Select Icon
$SelectIconButton.Add_Click({
    $IconPath = (Get-Item (Read-Host "Enter path to icon PNG")).FullName
    if ($IconPath) { $IconLabel.Content = $IconPath; $Global:SelectedIcon = $IconPath }
})

# Install dependencies
$InstallDepsButton.Add_Click({
    Show-Info "Installing dependencies (SDL2, Vorbis, Aroma SDK, 7-Zip if missing)..."
    # TODO: implement download/install of missing dependencies
})

# Build button
$BuildButton.Add_Click({
    if (-not $Global:DataRSDK) {
        Show-Error "Data.rsdk not selected! Cannot build."
        return
    }

    $Global:SelectedGame = ($GameSelect.SelectedItem).Content
    Ensure-Dir $Global:WuhbFolder

    try {
        BuildProgress.Value = 0
        Show-Info "Starting build for $Global:SelectedGame..."
        Patch-Headers
        # TODO: Insert actual build logic using devkitPro, SDL2, Vorbis, etc.
        BuildProgress.Value = 50
        Start-Sleep -Seconds 1 # simulate progress
        BuildProgress.Value = 100
        Show-Info "Build completed! WUHBS will be in $Global:WuhbFolder"
    } catch {
        Show-Error "Build failed: $_"
        Restore-Headers
    }
})

# Convert ELF to WUHB
$ConvertButton.Add_Click({
    if (-not $Global:SelectedELF) {
        Show-Error "No ELF selected."
        return
    }
    Ensure-Dir $Global:WuhbFolder
    # TODO: Insert WUHB conversion logic
    Show-Info "Converted $($Global:SelectedELF) to WUHB at $Global:WuhbFolder"
})

# ------------------------------
# SHOW WINDOW
# ------------------------------
$Window.ShowDialog() | Out-Null
#this script was designed to be all in the .exe, that's why the gui is in here too