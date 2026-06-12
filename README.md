# sonic2013-wiiu

This repository is a fork of https://gitlab.com/QuarkTheAwesome/sonic2013-wiiu with the goal of improving Aroma compatibility and polishing it.

See license at LICENSE.md 
## Highlights
- Focused fixes and build improvements for Aroma and Wii U runtime behavior
- Packaging helpers and scripts for creating WUHBs easily
- Quality-of-life changes and platform-specific fixes

## Wii U Data Layout

Keep the base datapacks on SD:
- `sd:/Sonic1/Data.rsdk`
- `sd:/Sonic2/Data.rsdk`

Modded WUHBs embed their `mods/` folder in the WUHB content root, so the base game data comes from SD while modded assets load from the mounted WUHB.
  
Mods used can be found on Team Forever's official page:
https://teamforeveronline.wixsite.com/home/sonic-1-forever
It's a great mod!
current known issues:

Pause softlocking the whole game on unmodded - currently figuring out how to fix it

I'm pretty sure this is actually the last issue I have to fix...
