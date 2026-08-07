# NovatOS — Build Guide

This document explains how NovatOS is built, both in GitHub Actions and locally.

## Build Pipeline Overview

```
GitHub Push to main
         │
         ▼
┌──────────────────────┐
│  validate.yml        │  ← fast lint (~30s)
│  - bash -n           │
│  - shellcheck        │
│  - duplicate checks  │
└──────────┬───────────┘
           │
           ▼
┌──────────────────────┐
│  build-iso.yml       │  ← heavy build (~30-60 min)
│  container:          │
│    archlinux:latest  │
│  - pacman-key init   │
│  - install archiso   │
│  - reflector         │
│  - mkarchiso -v      │
│  - sha256sum         │
│  - upload artifact   │
│  - publish release   │
└──────────┬───────────┘
           │ on failure
           ▼
┌──────────────────────┐
│  auto-fix.yml        │  ← self-healing
│  - download logs     │
│  - grep for errors   │
│  - patch profile     │
│  - commit + push     │
│  → re-triggers build │
└──────────────────────┘
```

## What `mkarchiso` Does

1. Reads `archiso/novatos/profiledef.sh` for ISO metadata
2. Sets up a working directory (`build/`)
3. Installs all packages from `packages.x86_64` into a chroot (`build/airootfs/`)
4. Copies overlay files from `airootfs/` on top of the chroot
5. Runs `airootfs/root/customize_airootfs.sh` **inside the chroot**
6. Builds initramfs for `linux` and `linux-zen`
7. Squashes the chroot into `airootfs.sfs`
8. Builds syslinux (BIOS) + systemd-boot (UEFI) boot images
9. Assembles the final hybrid ISO with xorriso

## Local Build

### Requirements

- Arch Linux host (or archlinux container with `--privileged`)
- `archiso`, `dosfstools`, `squashfs-tools`, `xorriso`, `mtools`, `grub`
- ~10 GB free disk space

### Steps

```bash
# Install build deps
sudo pacman -S archiso dosfstools squashfs-tools xorriso mtools grub edk2-ovmf

# Build (replace paths as needed)
sudo mkarchiso -v \
    -w /tmp/novatos-build \
    -o /tmp/novatos-out \
    archiso/novatos

# Result
ls -la /tmp/novatos-out/
# novatos-YYYY.MM.DD-x86_64.iso
```

### Test in QEMU

```bash
# UEFI boot
qemu-system-x86_64 \
    -enable-kvm -m 4096 -smp 4 \
    -bios /usr/share/edk2-ovmf/x64/OVMF_CODE.fd \
    -cdrom /tmp/novatos-out/novatos-*.iso \
    -boot d

# BIOS boot
qemu-system-x86_64 \
    -enable-kvm -m 4096 -smp 4 \
    -cdrom /tmp/novatos-out/novatos-*.iso \
    -boot d
```

## GitHub Actions Specifics

### Container Choice

The build runs in `archlinux:latest` because:
- archiso is an Arch-only tool
- The container's package set matches what the ISO will use
- No need to cross-build from Ubuntu

### Privileged Mode

The container needs `--privileged` because:
- pacman-key creates device files
- mkarchiso mounts loop devices for the ESP
- squashfs-tools may need FUSE

### Build Time

Typical build times on GitHub's `ubuntu-latest` runner:
- Container setup: ~3 min
- Mirror refresh: ~1 min
- Package download: ~5-10 min
- Package install (chroot): ~10-15 min
- customize_airootfs.sh: ~2 min
- Squashfs + ISO assembly: ~5 min
- **Total: ~25-35 min**

### Caching

The build does NOT cache the pacman package cache because:
- Arch is rolling — stale caches cause breakage
- GitHub's cache restore adds complexity for marginal gain
- 30-35 min is acceptable for a release ISO

## Auto-Fix Strategy

The `auto-fix.yml` workflow runs when `build-iso.yml` fails. It:

1. Downloads all job logs from the failed run
2. Greps for common error patterns:
   - `target not found: <pkg>` → removes the package
   - `missing dependency: <pkg>` → adds the dependency
   - `invalid or corrupted package` → strengthens keyring refresh
   - `conflicting dependencies` → notes for manual review
3. Commits fixes with a clear commit message
4. Pushes to `main`, which auto-triggers a new build
5. Opens an issue if no auto-fix is possible

This loop continues until the build is green or all known patterns are exhausted.

## Adding Packages

To add a package to the ISO:

1. Edit `archiso/novatos/packages.x86_64`
2. Add the package name (one per line, no comments on package lines)
3. Commit and push
4. The build will pick it up on next run

**Always check** that the package exists in the official Arch repos (core/extra/multilib).
AUR-only packages need a different strategy (post-install via `novatos-first-run`).
