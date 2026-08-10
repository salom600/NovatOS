#!/usr/bin/env bash
#
# /root/customize_airootfs.sh
# ===========================
# Run by archiso INSIDE the airootfs chroot, AFTER all packages are installed.
# This is the master customization script for the NovatOS Hyprland live ISO.
#
set -euo pipefail

echo "==> [NovatOS] customize_airootfs.sh starting..."

# ---------- Locale ----------
echo "==> [NovatOS] Generating locales..."
sed -i 's/^#\(en_US\.UTF-8\)/\1/' /etc/locale.gen
locale-gen
ln -sf /usr/share/zoneinfo/UTC /etc/localtime

# ---------- Users ----------
echo "==> [NovatOS] Creating novatos user (no password — auto-login)..."
groupadd -f wheel
if ! id -u novatos >/dev/null 2>&1; then
    useradd -m -G wheel,storage,optical,network,video,audio,input -s /usr/bin/zsh -u 1000 novatos
fi
# Set empty password (login without entering a password)
echo 'novatos:' | chpasswd -e 2>/dev/null || echo 'novatos:U6aMy0wojraho' | chpasswd -e
# Lock root account (no root login needed on live ISO)
passwd -l root 2>/dev/null || true

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
reflector --protocol https --latest 30 --sort rate \
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
)
for svc in "${SERVICES[@]}"; do
    systemctl enable "${svc}" 2>/dev/null || \
        echo "  [warn] could not enable ${svc}"
done
for sock in cups sshd avahi-daemon; do
    systemctl enable "${sock}.socket" 2>/dev/null || true
done

# ---------- SDDM autologin (Hyprland on live boot — no password) ----------
echo "==> [NovatOS] Enabling SDDM autologin for Hyprland (no password)..."
mkdir -p /etc/sddm.conf.d
cat > /etc/sddm.conf.d/autologin.conf <<'EOF'
[Autologin]
User=novatos
Session=hyprland

[Users]
SkipPassword=true
EOF

# ---------- SDDM theme (modern NovatOS theme) ----------
cat > /etc/sddm.conf.d/theme.conf <<'EOF'
[Theme]
ThemeDir=/usr/share/sddm/themes
Current=novatos

[Wayland]
EnableHiDPI=true
CompositorCommand=kwin_wayland --no-lockscreen

[X11]
EnableHiDPI=true
EOF

# ---------- PAM config: allow passwordless login ----------
cat > /etc/pam.d/sddm-autologin <<'EOF'
# PAM config for SDDM autologin — no password required
auth       sufficient   pam_succeed_if.so user = novatos
auth       required     pam_permit.so
account    include      sddm
password   include      sddm
session    include      sddm
EOF

# ---------- Polkit ----------
mkdir -p /etc/polkit-1/rules.d
cat > /etc/polkit-1/rules.d/10-novatos-live.rules <<'EOF'
polkit.addRule(function(action, subject) {
    if (subject.isInGroup("wheel")) {
        return polkit.Result.YES;
    }
});
EOF

# ---------- NetworkManager ----------
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

# ---------- ZSH default ----------
cat > /etc/skel/.zshrc <<'EOF'
# ~/.zshrc — NovatOS default
autoload -Uz compinit && compinit -d ~/.cache/zcompdump
zstyle ':completion:*' menu select
zstyle ':completion:*' matcher-list 'm:{a-zA-Z}={A-Za-z}'

HISTFILE=~/.cache/.zsh_history
HISTSIZE=10000
SAVEHIST=10000
setopt appendhistory sharehistory hist_ignore_dups hist_ignore_space

source /usr/share/zsh/plugins/zsh-autosuggestions/zsh-autosuggestions.zsh 2>/dev/null
source /usr/share/zsh/plugins/zsh-syntax-highlighting/zsh-syntax-highlighting.zsh 2>/dev/null

alias ls='ls --color=auto --group-directories-first'
alias ll='ls -la --color=auto --group-directories-first'
alias la='ls -A --color=auto'
alias grep='grep --color=auto'
alias ..='cd ..'
alias ...='cd ../..'
alias update='sudo pacman -Syu'
alias install='sudo pacman -S'
alias remove='sudo pacman -Rns'
alias search='pacman -Ss'
alias store='bauh'
alias fastfetch='fastfetch'

PROMPT='%F{cyan}%~%f %F{green}❯%f '
RPROMPT='%(?..%F{red}[%?] %f)'

command -v fastfetch >/dev/null && [ -z "$NOVATOS_NOBANNER" ] && fastfetch
EOF

# ---------- Bash fallback ----------
cat > /etc/skel/.bashrc <<'EOF'
[[ $- != *i* ]] && return

alias ls='ls --color=auto --group-directories-first'
alias ll='ls -la --color=auto --group-directories-first'
alias la='ls -A --color=auto'
alias grep='grep --color=auto'
alias ..='cd ..'
alias ...='cd ../..'
alias update='sudo pacman -Syu'
alias install='sudo pacman -S'
alias remove='sudo pacman -Rns'
alias search='pacman -Ss'
alias store='bauh'

PS1='\[\e[1;36m\]\u@\h\[\e[0m\] \[\e[1;34m\]\w\[\e[0m\] \$ '

command -v fastfetch >/dev/null && [ -z "$NOVATOS_NOBANNER" ] && fastfetch
EOF

# ---------- Install Mode auto-launch ----------
# When novatos.install=1 is passed as kernel param, auto-launch the installer
# instead of the desktop.
mkdir -p /etc/systemd/system
cat > /etc/systemd/system/novatos-install.service <<'EOF'
[Unit]
Description=NovatOS Installer (auto-launch in Install Mode)
After=systemd-user-sessions.service
ConditionKernelCommandLine=novatos.install

[Service]
Type=idle
ExecStart=/usr/local/bin/novatos-install
StandardInput=tty
TTYPath=/dev/tty1
TTYReset=yes
TTYVHangup=yes
User=root
WorkingDirectory=/root

[Install]
WantedBy=multi-user.target
EOF
systemctl enable novatos-install.service 2>/dev/null || true

# ---------- First-run check service ----------
cat > /usr/local/bin/novatos-first-run-check <<'EOF'
#!/usr/bin/env bash
# Runs novatos-first-run on first boot if not already done
if [ ! -f /etc/novatos-first-run-done ]; then
    sleep 3  # wait for desktop to settle
    sudo -E novatos-first-run 2>/dev/null &
fi
EOF
chmod +x /usr/local/bin/novatos-first-run-check

# ---------- Desktop entries ----------
mkdir -p /etc/skel/Desktop

cat > /etc/skel/Desktop/novatos-install.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=Install NovatOS
Name[ar]=تثبيت NovatOS
Comment=Launch the NovatOS installer
Icon=novatos-installer
Exec=novatos-install
Terminal=true
Categories=System;
StartupNotify=true
EOF
chmod +x /etc/skel/Desktop/novatos-install.desktop

cat > /etc/skel/Desktop/novatos-store.desktop <<'EOF'
[Desktop Entry]
Type=Application
Name=App Store (bauh)
Comment=Install apps from AUR, Flatpak, Snap, AppImage
Icon=novatos
Exec=bauh
Terminal=false
Categories=System;
StartupNotify=true
EOF
chmod +x /etc/skel/Desktop/novatos-store.desktop

# ---------- Ensure executable bits ----------
chmod +x /usr/local/bin/* 2>/dev/null || true

# ---------- Final cleanup ----------
echo "==> [NovatOS] Cleaning up..."
rm -f /var/cache/pacman/pkg/*.pkg.tar.zst 2>/dev/null || true

# ---------- Banner ----------
cat > /etc/novatos-release <<'EOF'
NovatOS 2026 "Aurora" (Hyprland Edition)
Built on Arch Linux
Desktop: Hyprland (Wayland) + Sway (fallback)
Installer: archinstall
App Store: bauh (AUR + Flatpak + Snap + AppImage)
Homepage: https://github.com/salom600/NovatOS
EOF
ln -sf /etc/novatos-release /etc/os-release 2>/dev/null || true

echo "==> [NovatOS] customize_airootfs.sh done."
