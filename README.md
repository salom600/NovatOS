# NovatOS — Aurora Edition 2026

> A modern, lightweight **Debian 12 (Bookworm)**-based distribution with a
> **custom-built desktop environment** (Qt6), designed to run on **any hardware
> from 2009 to 2026** — AMD R2, NVIDIA GT 240M, modern GPUs, and VMs.

[![Build ISO](https://github.com/salom600/NovatOS/actions/workflows/build-iso.yml/badge.svg)](https://github.com/salom600/NovatOS/actions/workflows/build-iso.yml)
[![Build Burner](https://github.com/salom600/NovatOS/actions/workflows/build-burner.yml/badge.svg)](https://github.com/salom600/NovatOS/releases)
[![Validate](https://github.com/salom600/NovatOS/actions/workflows/validate.yml/badge.svg)](https://github.com/salom600/NovatOS/actions/workflows/validate.yml)

---

## What is NovatOS?

NovatOS is a **complete rebuild** from the ground up — not based on Arch Linux
anymore. The new foundation is **Debian 12 (Bookworm)** for maximum stability
and hardware support. On top of that sits a **custom-built desktop environment**
(we wrote it ourselves in C++/Qt6) — not Hyprland, not KDE, not GNOME.

### Why Debian 12?
- **Rock-solid stable** — packages are tested for months before release
- **Best hardware support** — Debian supports more hardware than any other distro
- ** LTS-level support** — Debian 12 supported until 2028
- **Huge package repository** — 60,000+ packages

### Custom Desktop (not a fork)
We built our own desktop environment from scratch using **Qt6 + C++**:
- **Windows 11-style taskbar** at the bottom with Start menu
- **Custom lock screen** with NovatOS branding + clock
- **Software rendering fallback** — works on ANY GPU (no KMS required)
- **~150-200MB RAM** usage (lighter than KDE, GNOME, even Hyprland)
- **Dark NovatOS theme** with cyan (#4CC2FF) accents

### Highlights

| Feature | Details |
|---|---|
| **Base** | Debian 12 (Bookworm) — stable, well-tested |
| **Desktop** | **NovatOS Desktop** — custom Qt6 compositor (~150MB RAM) |
| **Installer** | **NovatOS Installer** — modern PyQt6 graphical installer |
| **App Store** | GNOME Software + Flatpak (one-click installs) |
| **GPU Support** | AMD + Intel + NVIDIA + **software rendering fallback** |
| **Hardware** | 2009-2026 (AMD R2, NVIDIA GT 240M, modern, VMs) |
| **Boot** | BIOS + UEFI hybrid, GRUB bootloader |
| **Live USB** | Test before installing — no changes to your disk |
| **USB Burner** | Windows `.exe` included (NovatOSBurner.exe) |

---

## Download & Use

### Option 1: Windows .exe Burner (easiest)
1. Download `NovatOSBurner.exe` from [Releases](https://github.com/salom600/NovatOS/releases)
2. Right-click → **Run as Administrator**
3. Browse → select the NovatOS `.iso`
4. Insert USB → select it → **Write to USB**
5. Boot from USB

### Option 2: Reassemble split ISO + dd/Ventoy
```bash
cat novatos-*.iso.part* > novatos.iso
sudo dd if=novatos.iso of=/dev/sdX bs=4M status=progress && sync
```

---

## Boot Menu (2 options)
| Option | Description |
|---|---|
| **NovatOS Live** | Boot to desktop (test without installing) |
| **NovatOS Install** | Launch graphical installer |

### Login (Live Mode)
- **Username:** `novatos`
- **Password:** (empty — just press Enter)

---

## Project Layout

```
NovatOS/
├── .github/workflows/     # CI: build-iso.yml, build-burner.yml, validate.yml
├── build/                 # Debian live-build configuration
│   ├── config/
│   │   ├── package-lists/ # All packages to install
│   │   └── hooks/normal/  # Post-install customization
│   └── includes.chroot/   # Files copied into the ISO
│       └── usr/local/bin/ # novatos-desktop launcher
├── desktop/               # Custom NovatOS Desktop (Qt6/C++)
│   ├── src/               # main.cpp, DesktopShell, Taskbar, StartMenu, LockScreen
│   └── assets/            # Session files, config
├── installer/             # NovatOS graphical installer (PyQt6)
│   └── src/novatos_installer.py
├── burner/                # Windows .exe USB burner (C + Win32)
│   └── src/main.c
└── docs/                  # Documentation
```

---

## Key Design Decisions

### 1. Custom Desktop (not Hyprland/KDE/GNOME)
- **Why?** Every existing DE has GPU dependencies that break on old hardware
- **Solution:** Built our own with Qt6, which has automatic software rendering
- **Result:** Works on AMD R2 (2009), NVIDIA GT 240M (2009), modern GPUs, VMs

### 2. Debian 12 (not Arch)
- **Why?** Arch broke too often (rolling release = constant changes)
- **Solution:** Debian 12 Stable — packages tested for months
- **Result:** Rock-solid, supports all hardware, huge package repo

### 3. Graphical Installer (not archinstall/text)
- **Why?** Text-based installers are "poor and outdated"
- **Solution:** Custom PyQt6 installer with modern Windows 11-style UI
- **Result:** Beautiful, easy to use, 5-step wizard

### 4. Software Rendering Fallback
- **Why?** Old GPUs (AMD R2, GT 240M) crash with KMS drivers
- **Solution:** Qt6 detects `nomodeset` and uses software rendering
- **Result:** Always boots, even on unsupported GPUs

---

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
