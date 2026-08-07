#!/usr/bin/env bash
#
# /root/customize_airootfs.sh
# ===========================
# Run by archiso INSIDE the airootfs chroot, AFTER all packages are installed.
# This is the master customization script for the NovatOS live ISO.
#
# All paths are absolute from the chroot's root.
set -euo pipefail

echo "==> [NovatOS] customize_airootfs.sh starting..."

# ---------- Locale ----------
echo "==> [NovatOS] Generating locales..."
sed -i 's/^#\(en_US\.UTF-8\)/\1/' /etc/locale.gen
locale-gen

echo "==> [NovatOS] Setting timezone to UTC..."
ln -sf /usr/share/zoneinfo/UTC /etc/localtime

# ---------- Users ----------
echo "==> [NovatOS] Creating novatos user..."
# Default live password is 'novatos' for both root and novatos user.
groupadd -f wheel
if ! id -u novatos >/dev/null 2>&1; then
    useradd -m -G wheel,storage,optical,network,video,audio,input -s /usr/bin/zsh -u 1000 novatos
fi
echo 'novatos:novatos' | chpasswd
echo 'root:novatos'    | chpasswd

# ---------- Sudo ----------
echo "==> [NovatOS] Configuring sudoers..."
chmod 440 /etc/sudoers
mkdir -p /etc/sudoers.d
cat > /etc/sudoers.d/10-novatos-live <<'EOF'
# Live ISO: passwordless sudo for novatos. Removed post-install.
novatos ALL=(ALL:ALL) NOPASSWD: ALL
EOF
chmod 440 /etc/sudoers.d/10-novatos-live

# ---------- pacman mirrorlist ----------
echo "==> [NovatOS] Refreshing mirrorlist..."
reflector --protocol https --latest 50 --sort rate \
          --save /etc/pacman.d/mirrorlist --download-timeout 5 2>/dev/null || \
    echo "  [warn] reflector failed; keeping default mirrorlist"

# ---------- Services ----------
echo "==> [NovatOS] Enabling systemd services..."
SERVICES=(
    NetworkManager
    ModemManager
    bluetooth
    sddm
    cups
    avahi-daemon
    reflector.timer
    systemd-timesyncd
    systemd-resolved
    haveged
    fwupd
    power-profiles-daemon
    libvirtd
    docker
)
for svc in "${SERVICES[@]}"; do
    systemctl enable "${svc}" 2>/dev/null || \
        echo "  [warn] could not enable ${svc} — package may be missing"
done

# Socket-activated services (preferred over .service for these)
for sock in cups sshd avahi-daemon; do
    systemctl enable "${sock}.socket" 2>/dev/null || true
done

# ---------- SDDM autologin (Plasma Wayland on live boot) ----------
echo "==> [NovatOS] Enabling SDDM autologin for Plasma Wayland..."
mkdir -p /etc/sddm.conf.d
cat > /etc/sddm.conf.d/autologin.conf <<'EOF'
[Autologin]
User=novatos
Session=plasmawayland
EOF

# ---------- Plymouth / boot splash ----------
echo "==> [NovatOS] Configuring plymouth..."
mkdir -p /etc/plymouth
cat > /etc/plymouth/plymouthd.conf <<'EOF'
[Daemon]
Theme=breeze
ShowDelay=2
DeviceTimeout=8
EOF

# ---------- mkinitcpio ----------
echo "==> [NovatOS] Configuring mkinitcpio..."
cat > /etc/mkinitcpio.conf <<'EOF'
MODULES=(vmd nvme ahci sd_mod usb_storage uas btrfs ext4 vfat exfat overlay)
BINARIES=()
FILES=()
HOOKS=(base udev modconf kms keyboard keymap consolefont block filesystems fsck)
COMPRESSION="zstd"
COMPRESSION_OPTIONS=(-T0 -19)
EOF

# Regenerate initramfs for both kernels
echo "==> [NovatOS] Regenerating initramfs..."
mkinitcpio -P || echo "  [warn] mkinitcpio regen failed"

# ---------- pacman init ----------
echo "==> [NovatOS] Initializing pacman keyring..."
pacman-key --init
pacman-key --populate archlinux

# ---------- NetworkManager ----------
echo "==> [NovatOS] Configuring NetworkManager..."
mkdir -p /etc/NetworkManager/system-connections
cat > /etc/NetworkManager/conf.d/10-novatos.conf <<'EOF'
[main]
plugins=keyfile
dns=systemd-resolved
rc-manager=unmanaged

[connectivity]
uri=https://www.archlinux.org/check_network_status.txt
interval=300
EOF

# ---------- Polkit: allow wheel to manage without password on live ----------
mkdir -p /etc/polkit-1/rules.d
cat > /etc/polkit-1/rules.d/10-novatos-live.rules <<'EOF'
// Allow members of the wheel group to perform administrative actions
// without authentication on the live ISO.
polkit.addRule(function(action, subject) {
    if (subject.isInGroup("wheel")) {
        return polkit.Result.YES;
    }
});
EOF

# ---------- ZSH default for new users ----------
mkdir -p /etc/skel
cat > /etc/skel/.zshrc <<'EOF'
# ~/.zshrc — NovatOS default
autoload -Uz compinit && compinit -d ~/.cache/zcompdump
zstyle ':completion:*' menu select
zstyle ':completion:*' matcher-list 'm:{a-zA-Z}={A-Za-z}'

HISTFILE=~/.cache/.zsh_history
HISTSIZE=10000
SAVEHIST=10000
setopt appendhistory sharehistory hist_ignore_dups hist_ignore_space

# Plugins (installed via system packages)
source /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh 2>/dev/null
source /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh 2>/dev/null

# Aliases
alias ls='exa --group-directories-first'
alias ll='exa -la --group-directories-first --git'
alias la='exa -a --group-directories-first'
alias cat='bat --paging=never'
alias grep='ripgrep'
alias find='fd'
alias ..='cd ..'
alias ...='cd ../..'
alias update='sudo pacman -Syu'
alias install='sudo pacman -S'
alias remove='sudo pacman -Rns'
alias search='pacman -Ss'
alias aur='paru'
alias flatinstall='flatpak install'
alias flatrun='flatpak run'
alias fastfetch='fastfetch'

# Prompt
PROMPT='%F{cyan}%~%f %F{green}❯%f '
RPROMPT='%(?..%F{red}[%?] %f)'

# Welcome
if [ -z "$NOVATOS_NOBANNER" ]; then
    command -v fastfetch >/dev/null && fastfetch
fi
EOF

# Bash config (fallback)
cat > /etc/skel/.bashrc <<'EOF'
# ~/.bashrc — NovatOS default
[[ $- != *i* ]] && return

alias ls='ls --color=auto --group-directories-first'
alias ll='ls -la --color=auto --group-directories-first'
alias la='ls -A --color=auto'
alias l='ls -CF --color=auto'
alias grep='grep --color=auto'
alias egrep='egrep --color=auto'
alias fgrep='fgrep --color=auto'
alias cp='cp -i'
alias mv='mv -i'
alias rm='rm -i'
alias ..='cd ..'
alias ...='cd ../..'
alias update='sudo pacman -Syu'
alias install='sudo pacman -S'
alias remove='sudo pacman -Rns'
alias search='pacman -Ss'

PS1='\[\e[1;36m\]\u@\h\[\e[0m\] \[\e[1;34m\]\w\[\e[0m\] \$ '

command -v fastfetch >/dev/null && [ -z "$NOVATOS_NOBANNER" ] && fastfetch
EOF

# ---------- Welcome desktop entry (Install NovatOS) ----------
mkdir -p /etc/skel/Desktop
cat > /etc/skel/Desktop/novatos-install.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Install NovatOS
Name[ar]=تثبيت NovatOS
Comment=Launch the NovatOS system installer
Comment[ar]=ابدأ مثبت نظام NovatOS
Icon=novatos-installer
Exec=sudo calamares
Terminal=false
Categories=System;
StartupNotify=true
EOF
chmod +x /etc/skel/Desktop/novatos-install.desktop

# ---------- Ensure executable bits ----------
chmod +x /usr/local/bin/* 2>/dev/null || true

# ---------- Final cleanup ----------
echo "==> [NovatOS] Cleaning up build artifacts..."
rm -f /var/cache/pacman/pkg/*.pkg.tar.zst 2>/dev/null || true

# ---------- Banner ----------
cat > /etc/novatos-release <<'EOF'
NovatOS 2026 "Aurora" (Live ISO)
Built on Arch Linux
Edition: Plasma 6 / Wayland / x86_64
Homepage: https://github.com/salom600/NovatOS
EOF

ln -sf /etc/novatos-release /etc/os-release 2>/dev/null || true

echo "==> [NovatOS] customize_airootfs.sh done."
