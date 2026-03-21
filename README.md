# sonic2013-wiiu

This repository is a fork of https://gitlab.com/QuarkTheAwesome/sonic2013-wiiu with the goal of improving Aroma compatibility and polishing it.

## Highlights
- Focused fixes and build improvements for Aroma and Wii U runtime behavior
- Packaging helpers and scripts for creating WUHB / Wii U-ready artifacts
- Quality-of-life changes and platform-specific fixes

## Getting started
1. Clone the repo:

	git clone https://github.com/siahisaforker/sonic3air-wiiuport.git

2. See the `RSDKv4/` directory for engine sources and platform-specific projects.

> Note: the repository recently added many dependencies as submodules; run `git submodule update --init --recursive` after cloning.

## License
This fork follows the original project's license. See the `LICENSE.md` file for details.

## Building for Wii U

**Prerequisites**
- `docker` (optional, recommended for reproducible builds)
- `devkitPro` with WUT
- Set the `DEVKITPRO` environment variable to your devkitPro install

**Using Docker (recommended)**
- Build the image:

```bash
docker build -f Dockerfile.wiiu -t sonic3air-wiiu:latest .
```

- Build inside a container (mounts current repo):

```bash
docker run --rm -v "$(pwd)":/workspace -w /workspace sonic3air-wiiu:latest make -f Makefile.wiiu
```

- Open an interactive shell inside the image:

```bash
docker run --rm -it -v "$(pwd)":/workspace -w /workspace sonic3air-wiiu:latest /bin/bash
```

See [Dockerfile.wiiu](Dockerfile.wiiu) for image details.

**Native build (devkitPro)**
- Example environment setup (Linux/macOS):

```bash
export DEVKITPRO=/opt/devkitPro
# or on Windows PowerShell:
$env:DEVKITPRO='C:\\devkitPro'
```

- Build with the provided Makefile:

```bash
make -f Makefile.wiiu
```

- To enable the case-insensitive file fallback (uses `fcaseopen`):

```bash
make -f Makefile.wiiu FORCE_CASE_INSENSITIVE=1
```

Build artifacts are produced in the `bin/` directory (for example `bin/RSDKv4.rpx`). See [Makefile.wiiu](Makefile.wiiu) for available targets and compiler flags.

## Packing WUHB (Wii U homebrew package)

After building the RPX (`bin/RSDKv4.rpx`) you can create a .wuhb package using the included packer scripts or `wuhbtool` from devkitPro.

**Prerequisites**
- `wuhbtool` (part of devkitPro tools) or a compatible `wuhb`/`wuhbtool` on PATH
- ImageMagick (`magick` or `convert`) — optional, used to convert icons/banners
- The repo includes convenience scripts: `scripts/wuhb_packer.ps1` and `scripts/wuhb_packer.sh`

**Windows (PowerShell)**

Build first:

```powershell
make -f Makefile.wiiu
```

Run the packer (interactive icon choice or pass `-Choice`):

```powershell
powershell -ExecutionPolicy Bypass -File scripts/wuhb_packer.ps1 -RpxPath ./bin/RSDKv4.rpx -OutDir ./out -Choice 1
```

**Linux / macOS / WSL**

Build first:

```bash
make -f Makefile.wiiu
```

Run the shell packer (pass `1` or `2` as third arg to skip the prompt):

```bash
./scripts/wuhb_packer.sh ./out ./bin/RSDKv4.rpx 1
```

The scripts will locate an icon in the `icon/` directory, convert it to PNG if ImageMagick is present, assemble a `wiiu/apps/RSDKv4` content tree, and call `wuhbtool` to produce `out/Sonic1.wuhb` (or `Sonic2.wuhb`).

**Notes**
- Set `WUHB_CMD` environment variable to override the detected `wuhbtool` if necessary.
- The packer scripts will fail if the RPX or a valid icon cannot be found; place icon files in the `icon/` folder (see repo `icon/` subfolders).

See `scripts/wuhb_packer.ps1` and `scripts/wuhb_packer.sh` for full behavior and options.






current known issues:

incredibly slow loading (with the mods versions) - Will be fixed soon


Couldn't load Game.xml (logs) - this only seems to happen on real hardware, i haven't been able to reproduce it but expect a fix in the near future


Pause softlocking the whole game on unmodded - currently figuring out how to fix it, as of now the next few builds will probably just have the pause button disabled entirely

i made this list real quickly so excuse the formatting

TODO: Add pro controller and other wii controllers support
