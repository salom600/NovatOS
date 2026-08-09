# NovatOS — Aurora Edition 2026 (Hyprland)

> A modern, lightweight **Arch Linux**-based distribution featuring **Hyprland**
> (Windows 11-style desktop, ~250MB RAM), built for 2009-2026 hardware.

[![Build ISO](https://github.com/salom600/NovatOS/actions/workflows/build-iso.yml/badge.svg)](https://github.com/salom600/NovatOS/actions/workflows/build-iso.yml)
[![Build Burner](https://github.com/salom600/NovatOS/actions/workflows/build-burner.yml/badge.svg)](https://github.com/salom600/NovatOS/releases)
[![Validate](https://github.com/salom600/NovatOS/actions/workflows/validate.yml/badge.svg)](https://github.com/salom600/NovatOS/actions/workflows/validate.yml)

---

## What is NovatOS?

NovatOS is an Arch-based Linux distribution assembled via [`archiso`](https://wiki.archlinux.org/title/Archiso)
and built entirely in **GitHub Actions**. Every push to `main` produces a fresh, bootable ISO
+ a Windows `.exe` USB burner, published to the [Releases](https://github.com/salom600/NovatOS/releases) page.

### Highlights

| Feature | Details |
|---|---|
| **Base** | Arch Linux, `linux-zen` + `linux-lts` kernels |
| **Desktop** | **Hyprland** (Wayland) — Windows 11-style with waybar + wofi, ~250MB RAM |
| **Fallback** | Sway with pixman (software rendering) → TTY console |
| **App Store** | **bauh** (AUR + Flatpak + Snap + AppImage — one-click installs) |
| **Installer** | archinstall (official Arch installer, NovatOS-branded) |
| **GPU Drivers** | AMD + Intel + NVIDIA open + llvmpipe (software fallback) |
| **Hardware** | 2009-2026 (linux-lts kernel for old hardware) |
| **Boot** | BIOS + UEFI hybrid, Ventoy-compatible (zstd initramfs) |
| **USB Burner** | Windows `.exe` included (NovatOSBurner.exe) |
| **Live USB** | `dd` / Ventoy / Rufus (DD mode) / NovatOSBurner.exe |

---

## Download & Use

### Option 1: Windows .exe Burner (easiest)

1. Download `NovatOSBurner.exe` from [Releases](https://github.com/salom600/NovatOS/releases)
2. **Right-click → Run as Administrator**
3. Click **Browse** and select the NovatOS `.iso` file
4. Insert a USB drive (≥4GB), select it from the list
5. Click **Write to USB** — wait for write + verification
6. Boot from the USB

### Option 2: Reassemble split ISO + dd/Ventoy/Rufus

The ISO is split into ~1.9GB chunks (GitHub's 2GB-per-asset limit).

```bash
# Download all .iso.partNN files, then:
cat novatos-*.iso.part* > novatos.iso
sha256sum -c novatos-*.sha256sum

# Flash with dd (Linux/macOS):
sudo dd if=novatos.iso of=/dev/sdX bs=4M status=progress && sync

# Or use Ventoy / balenaEtcher / Rufus (DD mode)
```

---

## Boot Menu (2 options)

| Option | Description |
|---|---|
| **NovatOS Live** | Boot to Hyprland desktop (for testing — works on any GPU) |
| **NovatOS Install** | Boot directly to installer (archinstall) |

### Login (Live Mode)
- **Username:** `novatos`
- **Password:** `novatos`

---

## GPU Fallback System

NovatOS Live boots on **any** hardware, even if the GPU driver is missing:

1. **Try Hyprland** with GPU acceleration (AMD/Intel/NVIDIA)
2. **Fall back to Sway** with pixman (software rendering — works on any GPU)
3. **Fall back to TTY console** (ultimate fallback)

This ensures the Live Mode always boots, from 2009-era Intel GMA to 2026 NVIDIA RTX.

---

## App Store: bauh

After first boot, `bauh` is automatically installed (via the first-run script).
Open it from the desktop shortcut or run `bauh` in a terminal.

bauh lets you install with one click:
- **AUR** packages (Steam, games, dev tools, etc.)
- **Flatpak** apps (from Flathub)
- **Snap** packages
- **AppImage** files

---

## Project Layout

```
NovatOS/
├── .github/workflows/        # CI: build-iso.yml, build-burner.yml, validate.yml, auto-fix.yml
├── archiso/novatos/          # archiso profile
│   ├── profiledef.sh         # ISO metadata
│   ├── packages.x86_64       # package list (Hyprland + bauh + core)
│   ├── airootfs/             # live system root overlay
│   │   ├── etc/skel/.config/ # Hyprland, waybar, wofi, dunst configs
│   │   ├── root/customize_airootfs.sh
│   │   └── usr/local/bin/    # novatos-session, novatos-install, novatos-first-run
│   ├── efiboot/              # UEFI (systemd-boot) configs — 2 entries
│   └── syslinux/             # BIOS syslinux configs — 2 entries
├── burner/                   # Windows .exe USB burner
│   ├── src/main.py           # Python source (tkinter GUI + raw disk I/O)
│   └── build.bat             # PyInstaller build script
└── docs/                     # design docs
```

---

## Build It Yourself

The ISO + .exe build 100% in GitHub Actions — no local build host required.

- **ISO build**: runs on `ubuntu-latest` in an `archlinux:latest` container via `mkarchiso`
- **Burner build**: runs on `windows-latest` via PyInstaller → `NovatOSBurner.exe`

Both publish to the same GitHub Release automatically on every push to `main`.

---

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
