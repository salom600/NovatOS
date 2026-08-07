# NovatOS — Feature List

## Modern Desktop Experience
- **KDE Plasma 6** on Wayland with SDDM
- Modern "Aurora" theme: dark with cyan + violet accents
- Windows 11-like taskbar at bottom, K Menu (Start menu equivalent)
- Split-screen tiling via KWin window rules (drag to screen edges)
- Multiple virtual desktops (2 by default)
- Custom NovatOS wallpaper set by default
- Reduced motion / accessibility options available

## Boot Experience
- BIOS + UEFI hybrid ISO (boots on any machine)
- Custom GRUB theme (dark + cyan)
- Custom SDDM login screen with NovatOS branding
- Plymouth boot splash (breeze theme)
- 3 live boot entries:
  - Default (zen kernel)
  - LTS kernel fallback
  - NVIDIA proprietary (blacklists nouveau)

## Hardware Support
- **All GPU vendors preinstalled:**
  - AMD: `mesa`, `vulkan-radeon`, `libva-mesa-driver`
  - Intel: `mesa`, `vulkan-intel`, `intel-media-driver`
  - NVIDIA: `nvidia`, `nvidia-utils`, `nvidia-dkms`, `lib32-nvidia-utils`
- **CPU microcode:** `amd-ucode` + `intel-ucode` (auto-selected at boot)
- **Wi-Fi:** `iwd` + `wpa_supplicant` + NetworkManager
- **Bluetooth:** `bluez` + `bluedevil`
- **Printers:** CUPS + system-config-printer + foomatic
- **Scanners:** SANE + simple-scan
- **Audio:** PipeWire (drop-in PulseAudio replacement)
- **Webcams:** `v4l2` + `pipewire-v4l2`
- **Tablets:** `solaar` (Logitech), `wacom` kernel drivers

## Software Preinstalled

### System
- File manager: Dolphin
- Terminal: Konsole + Yakuake (drop-down)
- Text editor: Kate, KWrite, nano, vim, micro
- Archive: Ark (handles .zip, .7z, .rar, .tar.*)
- Partition manager: GParted
- System monitor: Plasma System Monitor, btop, htop

### Office
- LibreOffice Fresh (Writer, Calc, Impress, Draw, Base, Math)
- Document viewer: Okular
- Image viewer: Gwenview
- Screenshot tool: Spectacle

### Internet
- Browser: Firefox + uBlock Origin + Dark Reader
- BitTorrent: KTorrent
- Download manager: KGet

### Media
- Video player: VLC + MPV
- Audio player: Elisa
- Video editor: Kdenlive
- Audio editor: Audacity
- Screen recorder / streaming: OBS Studio
- Image editor: GIMP + Krita + Inkscape

### Gaming
- Steam (with Proton)
- Lutris (game launcher for any platform)
- Wine + Winetricks + Protontricks
- Gamescope (micro-compositor for games)
- Mangohud (overlay: FPS, frame time, GPU/CPU usage)
- Gamemode (performance mode)
- RetroArch (multi-system emulator)
- mGBA (Game Boy Advance emulator)

### Development
- base-devel (gcc, make, etc.)
- git + git-lfs
- docker + docker-compose + podman + buildah
- distrobox (containerized dev environments)
- QEMU + virt-manager + libvirt (virtualization)

### System Tools
- `fastfetch` / `neofetch` — system info
- `btop`, `htop`, `iotop` — process monitors
- `nvtop` — GPU monitor
- `inxi` — hardware probe
- `dmidecode`, `lshw`, `lscpu`, `lsblk` — hardware info
- `gdb`, `strace`, `ltrace` — debugging

## App Store (One-Click Installs)
- **Discover** — KDE's graphical app store
- **Flatpak** + Flathub remote (auto-enabled on install)
- **AUR** access via `paru` (auto-installed by `novatos-first-run`)
- **Arch repos** — full access via pacman
- **Snap** support available (`snapd` preinstalled)

## Live USB Features
- Hybrid ISO: same image boots live AND installs
- Persistent storage optional (via Ventoy or manual setup)
- All apps work in live mode
- "Install NovatOS" desktop icon launches Calamares
- Welcome app on first login (KDE Plasma Welcome + NovatOS Welcome)

## Security
- UFW + Gufw firewall (graphical)
- firewalld also available (default in installer config)
- ClamAV + ClamTk (antivirus, optional scan)
- fail2ban (SSH brute-force protection)
- rkhunter (rootkit scanner)
- fprintd (fingerprint reader support)
- GNOME Keyring + Seahorse (password manager)

## Localization
- 18 locales pre-generated (en_US, ar_SA, fr_FR, de_DE, es_ES, it_IT, pt_BR,
  ru_RU, zh_CN, ja_JP, ko_KR, tr_TR, nl_NL, pl_PL, id_ID, etc.)
- Arabic keyboard layouts supported
- RTL (right-to-left) text rendering for Arabic/Hebrew users
- CJK fonts preinstalled (Noto Sans CJK)
