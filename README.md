This script will install everything that you could possibly need for building sonic 1/2 for use with aroma. Submit problems with the script to Issues (or try and fix it yourself)


https://gitlab.com/QuarkTheAwesome/sonic2013-wiiu

the script pulls from this repo ^^

only works on windows, though if someone wants to fork this and add Mac/Linux support I wouldn't mind
How does this work??
This app builds as a .elf first, before then turning into .rpx. by removing the .rpx part, you can get a .elf file instead, which is usable with aroma cfw (convertable to .wuhb)

Could you do this with many other old tiramisu apps?
I probably will, since it was kinda easy
be wary, I had to patch multiple files in retro engine to work with aroma (since some tiramisu functions cause aroma to crash, it's weird) but other then that, it's as simple as editing the makefile.
Common Issues:
The exe doesn't run! what do i do!!!

ensure these 2 things:
you have powershell 7 
you tried running as administrator

if you dont have powershell 7, microsoft hosts the msi for that on their website here: https://learn.microsoft.com/en-us/powershell/scripting/install/install-powershell-on-windows?view=powershell-7.5#winget
otherwise you can use winget in powershell to get it via this command: winget install --id Microsoft.PowerShell --source winget

the data.rsdk button isnt working!
this one is actually a known issue, unfortunately i don't know how to fix it so for the time being, press build before attempting to open that

it says i have scripts disabled?
1. Run Powershell as Administrator
2. Copy & Paste this command: Set-ExecutionPolicy RemoteSigned -Scope LocalMachine (if you dont want it to be permanent, run this instead Set-ExecutionPolicy Bypass -Scope Process)

It keeps saying build failed!!

You have to install devkitPro, press Win + R, run C:\devkitPro\msys2\msys2_shell.bat -mingw64, then run pacman -S wiiu-tools
devkitPro can be found here: https://devkitpro.org/wiki/Getting_Started
if you want the source code of wiiu-tools, it can be found here: https://github.com/zhuowei/wiiu-tools

I don't know where my ELF files were compiled to!

the standard location of them is C:\Users\{Your User}\sonic2013-wiiu\Builds
if they aren't there the build was not successful.
if they are .rpx, something REALLY went wrong, and you should report it immediately




If NONE of these apply to you:
submit them to the issues section on github



TODO:
fix data.rsdk button
handle installing dependencies automatically
implement better GUI (possibly a whole new api)
fix wuhb converter with aroma tools