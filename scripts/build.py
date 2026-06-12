#!/usr/bin/env python3
""" 
Let's try to make a universal build script.
"""

import argparse
import datetime
import os
import platform
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
ICON_DIR = ROOT_DIR / "icon"
MOD_DIR = ROOT_DIR / "mod"
BIN_DIR = ROOT_DIR / "bin"
SDL_DIR = ROOT_DIR / "dependencies" / "all" / "SDL"
SDL_PATCHES = [ROOT_DIR / "patches" / "sdl-wiiu-audio-input.patch"]
DEFAULT_OUT = ROOT_DIR / "out"
S3AIR_PLUS_ROOT = Path(r"C:\Users\josiah\Music\sonic3air-plus-wiiu\sonic3air-plus-wiiu")
DEFAULT_BOOT_SOUND = S3AIR_PLUS_ROOT / "assets" / "wiiu" / "boot.btsnd"
DEFAULT_BOOT_SOUND_INJECTOR = S3AIR_PLUS_ROOT / "tools" / "wiiu_inject_wuhb_bootsound.py"
# Common locations to check for devkitPro if DEVKITPRO env var is not set
DEVKITPRO_CANDIDATES = [
    "/opt/devkitpro",
    "/opt/devkitPro",
    "/usr/local/devkitpro",
    "/usr/local/devkitPro",
]
# Check for mod folders with these names
MODS = {
    "1": {
        "display": "Sonic Forever Mod",
        "folders": ["Sonic Forever Mod", "SonicForeverMod"],
    },
    "2": {
        "display": "Sonic 2 Absolute",
        "folders": ["Sonic 2 Absolute", "S2A"],
    },
}

IMAGE_EXTS = ("png", "tga", "jpg", "jpeg")


def find_devkitpro() -> str:
    env = os.environ.get("DEVKITPRO", "")
    if env and os.path.isdir(env):
        return env
    for c in DEVKITPRO_CANDIDATES:
        if os.path.isdir(c):
            return c
    return ""


def msys_path(path: Path | str) -> str:
    p = Path(path).resolve()
    text = str(p).replace("\\", "/")
    if platform.system() == "Windows" and len(text) >= 2 and text[1] == ":":
        return f"/{text[0].lower()}{text[2:]}"
    return text


def devkit_shell_path(path: Path | str) -> str:
    if platform.system() == "Windows":
        return msys_path(path)
    return str(Path(path).resolve())


def setup_env() -> dict:
    env = os.environ.copy()
    dkp = find_devkitpro()
    if not dkp:
        sys.exit("Cannot locate devkitPro. Set DEVKITPRO or install under /opt/devkitpro.")
    env["DEVKITPRO"] = dkp
    ppc = os.path.join(dkp, "devkitPPC")
    if os.path.isdir(ppc):
        env["DEVKITPPC"] = ppc
        env["PATH"] = os.pathsep.join(
            [os.path.join(dkp, "tools", "bin"), os.path.join(ppc, "bin"), env.get("PATH", "")]
        )
    else:
        env["PATH"] = os.pathsep.join(
            [os.path.join(dkp, "tools", "bin"), env.get("PATH", "")]
        )
    return env


def run(cmd, **kwargs):
    print(f"+ {' '.join(str(c) for c in cmd)}")
    subprocess.check_call(cmd, **kwargs)


def apply_sdl_patches():
    git = shutil.which("git")
    if not git:
        sys.exit("git is required to apply SDL patches")

    for patch in SDL_PATCHES:
        if not patch.is_file():
            sys.exit(f"SDL patch not found: {patch}")

        check = subprocess.run(
            [git, "-C", str(SDL_DIR), "apply", "--check", str(patch)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if check.returncode == 0:
            run([git, "-C", str(SDL_DIR), "apply", str(patch)])
            continue

        reverse_check = subprocess.run(
            [git, "-C", str(SDL_DIR), "apply", "--reverse", "--check", str(patch)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if reverse_check.returncode == 0:
            continue

        sys.exit(f"SDL patch could not be applied cleanly: {patch}")


def run_msys(command: str, env: dict, cwd: Path = ROOT_DIR):
    dkp = Path(env["DEVKITPRO"]).resolve()
    bash = dkp / "msys2" / "usr" / "bin" / "bash.exe"
    if not bash.is_file():
        sys.exit(f"MSYS2 bash not found under devkitPro: {bash}")

    dkp_msys = msys_path(dkp)
    path_prefix = ":".join([
        f"{dkp_msys}/tools/bin",
        f"{dkp_msys}/devkitPPC/bin",
        f"{dkp_msys}/msys2/usr/bin",
    ])
    script = (
        f"export DEVKITPRO={shlex.quote(dkp_msys)}; "
        f"export DEVKITPPC={shlex.quote(dkp_msys + '/devkitPPC')}; "
        f"export PATH={shlex.quote(path_prefix)}:$PATH; "
        f"cd {shlex.quote(msys_path(cwd))}; "
        f"{command}"
    )
    run([str(bash), "-lc", script], env=env)


def run_devkit_command(cmd: list[str], env: dict, cwd: Path = ROOT_DIR):
    if platform.system() == "Windows":
        run_msys(" ".join(shlex.quote(str(c)) for c in cmd), env, cwd)
    else:
        run([str(c) for c in cmd], cwd=cwd, env=env)


def cmake_build_game(choice: str, jobs: int, env: dict):
    build_dir = ROOT_DIR / "build" / f"wiiu-sonic{choice}"
    cmake_wrapper = Path(env["DEVKITPRO"]) / "portlibs" / "wiiu" / "bin" / "powerpc-eabi-cmake"
    if not cmake_wrapper.is_file():
        sys.exit(f"powerpc-eabi-cmake not found: {cmake_wrapper}")

    configure = [
        devkit_shell_path(cmake_wrapper),
        "-S", devkit_shell_path(ROOT_DIR),
        "-B", devkit_shell_path(build_dir),
        "-G", "Ninja",
        f"-DPACKAGED_GAME={choice}",
        "-DCMAKE_BUILD_TYPE=Release",
    ]
    run_devkit_command(configure, env)

    build = [
        "cmake",
        "--build", devkit_shell_path(build_dir),
        "--parallel", str(jobs),
    ]
    run_devkit_command(build, env)

    rpx = BIN_DIR / "RSDKv4.rpx"
    cmake_rpx = build_dir / "RSDKv4.rpx"
    if not rpx.is_file() and cmake_rpx.is_file():
        BIN_DIR.mkdir(exist_ok=True)
        shutil.copy2(cmake_rpx, rpx)
    if rpx.is_file():
        print(f"Built {rpx} ({rpx.stat().st_size} bytes)")
    else:
        sys.exit(f"RPX not found after CMake build: {rpx}")


def make_build_game(choice: str, jobs: int, env: dict):
    run(["make", "-f", "Makefile.wiiu", "clean"], cwd=ROOT_DIR, env=env)
    run(
        ["make", "-f", "Makefile.wiiu", f"PACKAGED_GAME={choice}", f"-j{jobs}"],
        cwd=ROOT_DIR,
        env=env,
    )
    rpx = BIN_DIR / "RSDKv4.rpx"
    if rpx.is_file():
        print(f"Built {rpx} ({rpx.stat().st_size} bytes)")
    else:
        sys.exit(f"RPX not found after build: {rpx}")


def build_game(choice: str, jobs: int, env: dict, build_system: str):
    print(f"\n=== Building RPX ({build_system}, PACKAGED_GAME={choice}) ===")
    apply_sdl_patches()
    if build_system == "cmake":
        cmake_build_game(choice, jobs, env)
    else:
        make_build_game(choice, jobs, env)


def find_icon(choice: str, name: str) -> Path | None:
    candidates = [
        ICON_DIR / f"Sonic {choice}",
        ICON_DIR / f"Sonic{choice}",
        ICON_DIR / f"sonic{choice}",
        ICON_DIR / f"sonic_{choice}",
        ICON_DIR,
    ]
    for d in candidates:
        for ext in IMAGE_EXTS:
            p = d / f"{name}.{ext}"
            if p.is_file():
                return p
    return None


def find_wuhbtool(env: dict) -> str:
    custom = os.environ.get("WUHB_CMD", "")
    if custom and shutil.which(custom):
        return custom
    w = shutil.which("wuhbtool", path=env.get("PATH"))
    if w:
        return w
    dkp = env.get("DEVKITPRO", "")
    fallback = os.path.join(dkp, "tools", "bin", "wuhbtool")
    if os.path.isfile(fallback) and os.access(fallback, os.X_OK):
        return fallback
    return ""


def find_image_tool() -> str:
    magick = shutil.which("magick")
    if magick:
        return magick

    convert = shutil.which("convert")
    if convert:
        try:
            result = subprocess.run([convert, "-version"], capture_output=True, text=True)
            if "ImageMagick" in (result.stdout + result.stderr):
                return convert
        except OSError:
            pass
    return ""


def copy_or_convert(src: Path, dst: Path, size: str, tool: str):
    if tool:
        run([tool, str(src), "-resize", size, str(dst)])
    else:
        try:
            from PIL import Image

            width, height = (int(part) for part in size.lower().split("x", 1))
            with Image.open(src) as image:
                image = image.convert("RGBA")
                image = image.resize((width, height), Image.Resampling.LANCZOS)
                image.save(dst)
        except Exception:
            shutil.copy2(src, dst)


def resolve_mod(choice: str):
    info = MODS.get(choice)
    if not info:
        return None, None, None
    for folder in info["folders"]:
        p = MOD_DIR / folder
        if p.is_dir():
            return info["display"], folder, p
    return info["display"], None, None


def resolve_build_path(value: str, default_path: Path | None = None) -> Path | None:
    if value:
        path = Path(value).expanduser()
        if not path.is_absolute():
            path = ROOT_DIR / path
        return path
    if default_path and default_path.is_file():
        return default_path
    return None


def resolve_boot_sound(args):
    if getattr(args, "no_boot_sound", False):
        return None, None

    boot_sound = resolve_build_path(
        getattr(args, "boot_sound", "") or os.environ.get("WUHB_BOOT_SOUND", ""),
        DEFAULT_BOOT_SOUND,
    )
    if not boot_sound:
        print("No boot sound configured; WUHB boot audio will be skipped.")
        return None, None
    if not boot_sound.is_file():
        sys.exit(f"Boot sound not found: {boot_sound}")

    injector = resolve_build_path(
        getattr(args, "boot_sound_injector", "") or os.environ.get("WUHB_BOOT_SOUND_INJECTOR", ""),
        DEFAULT_BOOT_SOUND_INJECTOR,
    )
    if not injector or not injector.is_file():
        sys.exit(f"WUHB boot sound injector not found: {injector or DEFAULT_BOOT_SOUND_INJECTOR}")

    return boot_sound, injector


def inject_boot_sound(wuhb: Path, boot_sound: Path | None, injector: Path | None):
    if not boot_sound or not injector:
        return
    run([sys.executable, str(injector), str(wuhb), str(boot_sound)])


# ── build ──────────────────────────────────────────────────────────────

def cmd_build(args):
    env = setup_env()
    jobs = args.jobs or os.cpu_count() or 4
    for game in args.games:
        build_game(game, jobs, env, args.build_system)


# ── pack ───────────────────────────────────────────────────────────────

def pack_variant(
    choice: str,
    include_mod: bool,
    output_stem: str,
    out_dir: Path,
    env: dict,
    boot_sound: Path | None,
    boot_sound_injector: Path | None,
):
    wuhbtool = find_wuhbtool(env)
    if not wuhbtool:
        sys.exit("wuhbtool not found. Install devkitPro tools or set WUHB_CMD.")

    image_tool = find_image_tool()
    icon_file = find_icon(choice, "icon")
    if not icon_file:
        sys.exit(f"No icon found for Sonic {choice} in {ICON_DIR}")
    banner_file = find_icon(choice, "banner")

    app_internal = f"RSDKv4_Sonic{choice}"
    pkg_dir = out_dir / f"wuhb_pack_{output_stem}"
    app_dir = pkg_dir / "wiiu" / "apps" / app_internal

    if pkg_dir.exists():
        shutil.rmtree(pkg_dir)
    app_dir.mkdir(parents=True)
    (pkg_dir / "wiiu" / "code").mkdir(parents=True, exist_ok=True)

    print(f"Packaging {output_stem}...")
    target_icon = app_dir / "icon.png"
    copy_or_convert(icon_file, target_icon, "128x128", image_tool)

    target_tv = target_drc = None
    if banner_file:
        target_tv = app_dir / "banner-tv.png"
        target_drc = app_dir / "banner-drc.png"
        copy_or_convert(banner_file, target_tv, "1280x720", image_tool)
        copy_or_convert(banner_file, target_drc, "854x480", image_tool)

    rpx = BIN_DIR / "RSDKv4.rpx"
    if not rpx.is_file():
        sys.exit(f"RPX not found at {rpx}")
    shutil.copy2(rpx, app_dir / "RSDKv4.rpx")

    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    meta = (
        f"title=Sonic {choice}\n"
        f"game=RSDKv4\n"
        f"source=Sonic {choice}\n"
        f"game_folder=Sonic{choice}\n"
        f"pack_time={now}\n"
    )
    (app_dir / "metadata.txt").write_text(meta)
    (pkg_dir / "wiiu" / "code" / "metadata.txt").write_text(meta)

    if include_mod:
        display, folder, mod_path = resolve_mod(choice)
        if mod_path and mod_path.is_dir():
            print(f"  Including mod '{display}' from {mod_path}")
            dst = pkg_dir / "wiiu" / "mods" / folder
            shutil.copytree(mod_path, dst)
            (pkg_dir / "wiiu" / "mods" / "modconfig.ini").write_text(f"[mods]\n{folder}=true\n")
        else:
            sys.exit(f"Mod for Sonic {choice} not found in {MOD_DIR}")

    out_file = out_dir / f"{output_stem}.wuhb"
    cmd = [
        wuhbtool,
        str(app_dir / "RSDKv4.rpx"),
        str(out_file),
        "--content", str(pkg_dir / "wiiu"),
        "--icon", str(target_icon),
        "--name", f"Sonic {choice}",
        "--short-name", f"Sonic{choice}",
        "--author", "RSDKv4 Packager",
    ]
    if target_tv and target_drc:
        cmd.extend(["--tv-image", str(target_tv), "--drc-image", str(target_drc)])

    run(cmd)
    inject_boot_sound(out_file, boot_sound, boot_sound_injector)
    print(f"  Created {out_file}")


def cmd_pack(args):
    env = setup_env()
    boot_sound, boot_sound_injector = resolve_boot_sound(args)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    current_game = [None]

    def ensure_rpx(choice):
        if current_game[0] == choice and (BIN_DIR / "RSDKv4.rpx").is_file():
            return
        if not args.skip_build:
            jobs = args.jobs or os.cpu_count() or 4
            build_game(choice, jobs, env, args.build_system)
            current_game[0] = choice

    for choice in args.games:
        for modded in (False, True):
            stem = f"Sonic{choice}-{'modded' if modded else 'unmodded'}"
            ensure_rpx(choice)
            pack_variant(choice, modded, stem, out_dir, env, boot_sound, boot_sound_injector)


# ── all ────────────────────────────────────────────────────────────────

def cmd_all(args):
    args.games = ["1", "2"]
    args.skip_build = False
    cmd_pack(args)


# ── docker ─────────────────────────────────────────────────────────────

def cmd_docker(args):
    print("Building Docker image...")
    run(["docker", "build", "-t", "sonic3air-wiiu", "-f", "Dockerfile.wiiu", "."], cwd=ROOT_DIR)
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    mount = f"{ROOT_DIR}:/workspace"
    print("Running build inside container...")
    run([
        "docker", "run", "--rm",
        "-v", mount,
        "-w", "/workspace",
        "sonic3air-wiiu",
        "python3", "scripts/build.py", "all",
        "--out-dir", "/workspace/out",
    ])


# ── CLI ────────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="command")

    # build
    b = sub.add_parser("build", help="Compile RPX via CMake or Makefile.wiiu")
    b.add_argument("--games", nargs="+", default=["1", "2"], choices=["1", "2"])
    b.add_argument("--build-system", choices=["cmake", "make"], default="cmake")
    b.add_argument("-j", "--jobs", type=int, default=0)

    # pack
    pk = sub.add_parser("pack", help="Build + package WUHB files")
    pk.add_argument("--games", nargs="+", default=["1", "2"], choices=["1", "2"])
    pk.add_argument("--out-dir", default=str(DEFAULT_OUT))
    pk.add_argument("--skip-build", action="store_true")
    pk.add_argument("--build-system", choices=["cmake", "make"], default="cmake")
    pk.add_argument("--boot-sound", default="", help="Path to boot.btsnd to inject into each WUHB")
    pk.add_argument("--boot-sound-injector", default="", help="Path to the WUHB boot sound injector")
    pk.add_argument("--no-boot-sound", action="store_true", help="Do not inject WUHB boot audio")
    pk.add_argument("-j", "--jobs", type=int, default=0)

    # all (default)
    a = sub.add_parser("all", help="Build + pack all 4 WUHB variants")
    a.add_argument("--out-dir", default=str(DEFAULT_OUT))
    a.add_argument("--build-system", choices=["cmake", "make"], default="cmake")
    a.add_argument("--boot-sound", default="", help="Path to boot.btsnd to inject into each WUHB")
    a.add_argument("--boot-sound-injector", default="", help="Path to the WUHB boot sound injector")
    a.add_argument("--no-boot-sound", action="store_true", help="Do not inject WUHB boot audio")
    a.add_argument("-j", "--jobs", type=int, default=0)

    # docker
    d = sub.add_parser("docker", help="Build everything inside Docker")
    d.add_argument("--out-dir", default=str(DEFAULT_OUT))

    args = p.parse_args()
    if not args.command:
        args.command = "all"
        args.out_dir = str(DEFAULT_OUT)
        args.build_system = "cmake"
        args.boot_sound = ""
        args.boot_sound_injector = ""
        args.no_boot_sound = False
        args.jobs = 0

    {
        "build": cmd_build,
        "pack": cmd_pack,
        "all": cmd_all,
        "docker": cmd_docker,
    }[args.command](args)

    print("\nDone.")


if __name__ == "__main__":
    main()
