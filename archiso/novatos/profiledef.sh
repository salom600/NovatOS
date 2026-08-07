#!/usr/bin/env bash
# shellcheck disable=SC2034,SC2154
# SPDX-License-Identifier: GPL-3.0-or-later
#
# NovatOS archiso profile definition
# Builds a hybrid BIOS+UEFI ISO with KDE Plasma 6, Calamares, and gaming stack.

# Image name (also used as ISO volume label, max 32 chars)
iso_name="novatos"
iso_label="NOVATOS_$(date +%Y%m)"
iso_publisher="NovatOS Project <https://github.com/salom600/NovatOS>"
iso_application="NovatOS Linux Live/Install ISO"
iso_version="$(date +%Y.%m.%d)"
iso_install_dir="novatos"
build_date="$(date +%Y-%m-%d)"

# Bootloaders (BIOS + UEFI hybrid ISO)
# NOTE: archiso 89+ removed the `buildmodes` array — only `bootmodes` is valid.
bootmodes=('bios.syslinux.mbr' 'bios.syslinux.eltorito'
           'uefi-x64.systemd-boot.esp'
           'uefi-x64.systemd-boot.eltorito')

# Architecture
arch="x86_64"

# pacman.conf to use (relative to profile dir)
pacman_conf="pacman.conf"

# airootfs directory (relative to profile dir)
airootfs_dir="airootfs"

# Don't export this — used internally
# vim:set ft=sh:
