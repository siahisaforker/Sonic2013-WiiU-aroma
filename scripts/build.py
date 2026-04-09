#!/usr/bin/env python3
""" 
Let's try to make a universal build script.
"""

import argparse
import datetime
import os
import platform
import shutil
import subprocess
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parent.parent
ICON_DIR = ROOT_DIR / "icon"
MOD_DIR = ROOT_DIR / "mod"
BIN_DIR = ROOT_DIR / "bin"
DEFAULT_OUT = ROOT_DIR / "out"
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
    for cmd in ("magick", "convert"):
        if shutil.which(cmd):
            return cmd
    return ""


def copy_or_convert(src: Path, dst: Path, size: str, tool: str):
    if tool:
        run([tool, str(src), "-resize", size, str(dst)])
    else:
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


# ── build ──────────────────────────────────────────────────────────────

def cmd_build(args):
    env = setup_env()
    jobs = args.jobs or os.cpu_count() or 4
    for game in args.games:
        print(f"\n=== Building RPX (PACKAGED_GAME={game}) ===")
        run(["make", "-f", "Makefile.wiiu", "clean"], cwd=ROOT_DIR, env=env)
        run(
            ["make", "-f", "Makefile.wiiu", f"PACKAGED_GAME={game}", f"-j{jobs}"],
            cwd=ROOT_DIR,
            env=env,
        )
        rpx = BIN_DIR / "RSDKv4.rpx"
        if rpx.is_file():
            print(f"Built {rpx} ({rpx.stat().st_size} bytes)")
        else:
            sys.exit(f"RPX not found after build: {rpx}")


# ── pack ───────────────────────────────────────────────────────────────

def pack_variant(choice: str, include_mod: bool, output_stem: str, out_dir: Path, env: dict):
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
            dst = app_dir / "mods" / folder
            shutil.copytree(mod_path, dst)
            (app_dir / "mods" / "modconfig.ini").write_text(f"[mods]\n{folder}=true\n")
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
    print(f"  Created {out_file}")


def cmd_pack(args):
    env = setup_env()
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    current_game = [None]

    def ensure_rpx(choice):
        if current_game[0] == choice and (BIN_DIR / "RSDKv4.rpx").is_file():
            return
        if not args.skip_build:
            jobs = args.jobs or os.cpu_count() or 4
            print(f"\n=== Building RPX (PACKAGED_GAME={choice}) ===")
            run(["make", "-f", "Makefile.wiiu", "clean"], cwd=ROOT_DIR, env=env)
            run(
                ["make", "-f", "Makefile.wiiu", f"PACKAGED_GAME={choice}", f"-j{jobs}"],
                cwd=ROOT_DIR,
                env=env,
            )
            current_game[0] = choice

    for choice in args.games:
        for modded in (False, True):
            stem = f"Sonic{choice}-{'modded' if modded else 'unmodded'}"
            ensure_rpx(choice)
            pack_variant(choice, modded, stem, out_dir, env)


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
    b = sub.add_parser("build", help="Compile RPX via Makefile.wiiu")
    b.add_argument("--games", nargs="+", default=["1", "2"], choices=["1", "2"])
    b.add_argument("-j", "--jobs", type=int, default=0)

    # pack
    pk = sub.add_parser("pack", help="Build + package WUHB files")
    pk.add_argument("--games", nargs="+", default=["1", "2"], choices=["1", "2"])
    pk.add_argument("--out-dir", default=str(DEFAULT_OUT))
    pk.add_argument("--skip-build", action="store_true")
    pk.add_argument("-j", "--jobs", type=int, default=0)

    # all (default)
    a = sub.add_parser("all", help="Build + pack all 4 WUHB variants")
    a.add_argument("--out-dir", default=str(DEFAULT_OUT))
    a.add_argument("-j", "--jobs", type=int, default=0)

    # docker
    d = sub.add_parser("docker", help="Build everything inside Docker")
    d.add_argument("--out-dir", default=str(DEFAULT_OUT))

    args = p.parse_args()
    if not args.command:
        args.command = "all"
        args.out_dir = str(DEFAULT_OUT)
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
