#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUT_DIR="$ROOT_DIR/out"
ICON_DIR="$ROOT_DIR/icon"
CURRENT_PACKAGED_GAME=""

usage() {
  cat <<'EOF'
Usage: build_wiiu_wuhbs.sh [--out-dir DIR]

Builds and packs all Wii U WUHB variants:
  - Sonic1-unmodded
  - Sonic1-modded
  - Sonic2-unmodded
  - Sonic2-modded

The script auto-detects devkitPro inside Linux/WSL and exports DEVKITPRO,
DEVKITPPC, and PATH before building and packing each WUHB.
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --out-dir)
      OUT_DIR="$2"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

find_devkitpro() {
  local candidates=()
  if [ -n "${DEVKITPRO:-}" ]; then
    candidates+=("$DEVKITPRO")
  fi
  candidates+=(
    /opt/devkitpro
    /opt/devkitPro
    /usr/local/devkitpro
    /usr/local/devkitPro
  )

  local candidate
  for candidate in "${candidates[@]}"; do
    if [ -d "$candidate" ]; then
      printf '%s\n' "$candidate"
      return 0
    fi
  done

  return 1
}

DEVKITPRO_ROOT="$(find_devkitpro || true)"
if [ -z "$DEVKITPRO_ROOT" ]; then
  echo "Unable to locate devkitPro. Set DEVKITPRO or install it under /opt/devkitpro." >&2
  exit 1
fi

export DEVKITPRO="$DEVKITPRO_ROOT"
if [ -z "${DEVKITPPC:-}" ]; then
  if [ -d "$DEVKITPRO/devkitPPC" ]; then
    export DEVKITPPC="$DEVKITPRO/devkitPPC"
  fi
fi

if [ -d "${DEVKITPPC:-}" ]; then
  export PATH="$DEVKITPRO/tools/bin:$DEVKITPPC/bin:$PATH"
else
  export PATH="$DEVKITPRO/tools/bin:$PATH"
fi

if ! command -v wuhbtool >/dev/null 2>&1 && [ ! -x "$DEVKITPRO/tools/bin/wuhbtool" ]; then
  echo "wuhbtool is not available in the detected devkitPro install ($DEVKITPRO)." >&2
  exit 1
fi

mkdir -p "$OUT_DIR"
OUT_DIR="$(cd "$OUT_DIR" && pwd)"

echo "Using DEVKITPRO=$DEVKITPRO"
if [ -n "${DEVKITPPC:-}" ]; then
  echo "Using DEVKITPPC=$DEVKITPPC"
fi

find_wuhbtool() {
  if [ -n "${WUHB_CMD:-}" ]; then
    printf '%s\n' "$WUHB_CMD"
    return 0
  fi

  if command -v wuhbtool >/dev/null 2>&1; then
    command -v wuhbtool
    return 0
  fi

  if [ -x "$DEVKITPRO/tools/bin/wuhbtool" ]; then
    printf '%s\n' "$DEVKITPRO/tools/bin/wuhbtool"
    return 0
  fi

  return 1
}

find_image_tool() {
  if command -v magick >/dev/null 2>&1; then
    printf '%s\n' "magick"
    return 0
  fi

  if command -v convert >/dev/null 2>&1; then
    printf '%s\n' "convert"
    return 0
  fi

  return 1
}

copy_or_convert_image() {
  local source_file="$1"
  local target_file="$2"
  local size="$3"
  local image_tool="${4:-}"

  if [ -n "$image_tool" ]; then
    "$image_tool" "$source_file" -resize "$size" "$target_file"
  else
    cp "$source_file" "$target_file"
  fi
}

find_icon_asset() {
  local choice="$1"
  local asset_name="$2"
  local exts=(png tga jpg jpeg)
  local candidates=(
    "$ICON_DIR/Sonic $choice"
    "$ICON_DIR/Sonic$choice"
    "$ICON_DIR/sonic$choice"
    "$ICON_DIR/sonic_$choice"
    "$ICON_DIR"
  )

  local dir ext candidate
  for dir in "${candidates[@]}"; do
    for ext in "${exts[@]}"; do
      candidate="$dir/$asset_name.$ext"
      if [ -f "$candidate" ]; then
        printf '%s\n' "$candidate"
        return 0
      fi
    done
  done

  return 1
}

resolve_mod_info() {
  local choice="$1"
  local -n mod_src_ref="$2"
  local -n mod_dir_ref="$3"
  local -n display_name_ref="$4"

  mod_src_ref=""
  mod_dir_ref=""
  display_name_ref=""

  if [ "$choice" = "1" ]; then
    display_name_ref="Sonic Forever Mod"
    if [ -d "$ROOT_DIR/mod/$display_name_ref" ]; then
      mod_dir_ref="$display_name_ref"
      mod_src_ref="$ROOT_DIR/mod/$mod_dir_ref"
    elif [ -d "$ROOT_DIR/mod/SonicForeverMod" ]; then
      mod_dir_ref="SonicForeverMod"
      mod_src_ref="$ROOT_DIR/mod/$mod_dir_ref"
    fi
  else
    display_name_ref="Sonic 2 Absolute"
    if [ -d "$ROOT_DIR/mod/$display_name_ref" ]; then
      mod_dir_ref="$display_name_ref"
      mod_src_ref="$ROOT_DIR/mod/$mod_dir_ref"
    elif [ -d "$ROOT_DIR/mod/S2A" ]; then
      mod_dir_ref="S2A"
      mod_src_ref="$ROOT_DIR/mod/$mod_dir_ref"
    fi
  fi
}

ensure_rpx_for_choice() {
  local choice="$1"

  if [ "$CURRENT_PACKAGED_GAME" = "$choice" ] && [ -f "$ROOT_DIR/bin/RSDKv4.rpx" ]; then
    return 0
  fi

  echo "Building RPX with PACKAGED_GAME=$choice..."
  (
    cd "$ROOT_DIR"
    make -f Makefile.wiiu clean >/dev/null 2>&1 || true
    make -f Makefile.wiiu PACKAGED_GAME="$choice"
  )

  if [ ! -f "$ROOT_DIR/bin/RSDKv4.rpx" ]; then
    echo "Expected RPX missing after build: $ROOT_DIR/bin/RSDKv4.rpx" >&2
    exit 1
  fi

  CURRENT_PACKAGED_GAME="$choice"
}

build_variant() {
  local choice="$1"
  local include_mod="$2"
  local output_stem="$3"
  local app_internal
  local icon_file
  local banner_file=""
  local image_tool=""
  local wuhb_cmd
  local pkg_dir
  local app_dir
  local target_icon
  local target_tv_banner=""
  local target_drc_banner=""
  local out_file
  local mod_src=""
  local mod_dir=""
  local display_name=""
  local include_name
  local cmd=()

  ensure_rpx_for_choice "$choice"

  wuhb_cmd="$(find_wuhbtool)"
  image_tool="$(find_image_tool || true)"
  icon_file="$(find_icon_asset "$choice" icon)"
  banner_file="$(find_icon_asset "$choice" banner || true)"

  app_internal="RSDKv4_Sonic${choice}"
  pkg_dir="$OUT_DIR/wuhb_pack_${output_stem}"
  app_dir="$pkg_dir/wiiu/apps/$app_internal"
  target_icon="$app_dir/icon.png"
  out_file="$OUT_DIR/${output_stem}.wuhb"

  rm -rf "$pkg_dir"
  mkdir -p "$app_dir" "$pkg_dir/wiiu/code"

  echo "Packaging $output_stem..."
  copy_or_convert_image "$icon_file" "$target_icon" "128x128" "$image_tool"
  if [ -n "$banner_file" ]; then
    target_tv_banner="$app_dir/banner-tv.png"
    target_drc_banner="$app_dir/banner-drc.png"
    copy_or_convert_image "$banner_file" "$target_tv_banner" "1280x720" "$image_tool"
    copy_or_convert_image "$banner_file" "$target_drc_banner" "854x480" "$image_tool"
  fi

  cp "$ROOT_DIR/bin/RSDKv4.rpx" "$app_dir/RSDKv4.rpx"

  cat > "$app_dir/metadata.txt" <<EOF
title=Sonic $choice
game=RSDKv4
source=Sonic $choice
game_folder=Sonic${choice}
pack_time=$(date -u +%Y-%m-%dT%H:%M:%SZ)
EOF
  cp "$app_dir/metadata.txt" "$pkg_dir/wiiu/code/metadata.txt"

  if [ "$include_mod" = "y" ]; then
    resolve_mod_info "$choice" mod_src mod_dir display_name
    if [ -n "$mod_src" ] && [ -d "$mod_src" ]; then
      echo "Including mod '$display_name' from $mod_src"
      mkdir -p "$app_dir/mods"
      cp -r "$mod_src" "$app_dir/mods/"
      printf '[mods]\n%s=true\n' "$mod_dir" > "$app_dir/mods/modconfig.ini"
    else
      echo "Requested mod for Sonic $choice but no matching mod folder was found." >&2
      exit 1
    fi
  fi

  include_name="Sonic $choice"
  cmd=(
    "$wuhb_cmd"
    "$app_dir/RSDKv4.rpx"
    "$out_file"
    --content "$pkg_dir/wiiu"
    --icon "$target_icon"
    --name "$include_name"
    --short-name "Sonic${choice}"
    --author "RSDKv4 Packager"
  )
  if [ -n "$target_tv_banner" ] && [ -n "$target_drc_banner" ]; then
    cmd+=(--tv-image "$target_tv_banner" --drc-image "$target_drc_banner")
  fi

  "${cmd[@]}"

  echo "Created $out_file"
}

build_variant 1 n Sonic1-unmodded
build_variant 1 y Sonic1-modded
build_variant 2 n Sonic2-unmodded
build_variant 2 y Sonic2-modded

echo "Finished building Wii U WUHBs in $OUT_DIR"
