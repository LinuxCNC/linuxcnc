#!/bin/bash
# Sourced by launch.sh and quit-launch.sh. The launcher shows the INI's
# [DISPLAY] INTRO_GRAPHIC as a splash that closes itself after INTRO_TIME
# and would cover the GUI in a fast run's confirm shot. no_splash_ini runs
# a config without it: it mirrors the config's directory as symlinks, all
# but the INI, which it copies without INTRO_GRAPHIC. The files the INI
# names are the shipped ones; a value starting with ../ is made absolute,
# since the mirror sits elsewhere. It sets NO_SPLASH_INI to the mirrored
# INI and NO_SPLASH_DIR to the mirror, for the caller to remove.

no_splash_ini() {
    local src_ini="$1" src_dir
    src_dir="$(cd "$(dirname "$src_ini")" && pwd)" || return 1
    NO_SPLASH_DIR="$(mktemp -d -t ui-smoke-ini.XXXXXX)" || return 1
    cp -as "$src_dir/." "$NO_SPLASH_DIR/" || return 1
    NO_SPLASH_INI="$NO_SPLASH_DIR/$(basename "$src_ini")"
    rm -f "$NO_SPLASH_INI"
    sed -e '/^INTRO_GRAPHIC[[:space:]]*=/d' \
        -e "s|^\([A-Z_]*[[:space:]]*=[[:space:]]*\)\.\./|\1$src_dir/../|" \
        "$src_ini" >"$NO_SPLASH_INI"
}
