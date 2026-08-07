#!/usr/bin/env bash
# shellcheck disable=SC2034,SC2154
# SPDX-License-Identifier: GPL-3.0-or-later
#
# NovatOS archiso profile definition
# Builds a hybrid BIOS+UEFI ISO with KDE Plasma 6, archinstall, and gaming stack.
#
# CRITICAL archiso 89 changes from older versions:
#   - Variable is `install_dir` (NOT `iso_install_dir`)
#   - `buildmodes=('iso')` is required (specifies ISO output)
#   - `bootmodes` uses new syntax without '.mbr'/'.eltorito' suffixes
#   - Initramfs hooks configured via /etc/mkinitcpio.conf.d/archiso.conf drop-in
#   - `mkinitcpio-archiso` package provides the live-boot hooks

# Image name (also used as ISO volume label, max 32 chars)
iso_name="novatos"
iso_label="NOVATOS_$(date +%Y%m)"
iso_publisher="NovatOS Project <https://github.com/salom600/NovatOS>"
iso_application="NovatOS Linux Live/Install ISO"
iso_version="$(date +%Y.%m.%d)"

# CRITICAL: In archiso 89+, the variable is `install_dir` (NOT `iso_install_dir`).
# If wrong, archiso falls back to app_name (="mkarchiso"), breaking all boot paths.
install_dir="novatos"

build_date="$(date +%Y-%m-%d)"

# Build mode: 'iso' = produce a hybrid ISO image
buildmodes=('iso')

# Bootloaders (archiso 89 new syntax — no '.mbr'/'.eltorito' suffixes)
#   bios.syslinux        → BIOS boot via syslinux (includes MBR + El Torito automatically)
#   uefi.systemd-boot    → UEFI boot via systemd-boot (includes ESP + El Torito automatically)
bootmodes=('bios.syslinux' 'uefi.systemd-boot')

# Architecture
arch="x86_64"

# pacman.conf to use (relative to profile dir)
pacman_conf="pacman.conf"

# airootfs directory (relative to profile dir)
airootfs_dir="airootfs"

# Use squashfs for the airootfs image (standard for live ISOs)
airootfs_image_type="squashfs"

# xz compression for smallest ISO size
# -comp xz: use xz compression
# -Xbcj x86: x86 BCJ filter (better ratio for x86 binaries)
# -b 1M: 1MB block size
# -Xdict-size 1M: dictionary size
airootfs_image_tool_options=('-comp' 'xz' '-Xbcj' 'x86' '-b' '1M' '-Xdict-size' '1M')

# File permissions for special files in the airootfs
file_permissions=(
  ["/etc/shadow"]="0:0:400"
  ["/root"]="0:0:750"
  ["/usr/local/bin/novatos-installer"]="0:0:755"
  ["/usr/local/bin/novatos-welcome"]="0:0:755"
  ["/usr/local/bin/novatos-first-run"]="0:0:755"
  ["/usr/local/bin/novatos-mirror-refresh"]="0:0:755"
  ["/usr/local/bin/novatos-gpu-info"]="0:0:755"
)

# vim:set ft=sh:
