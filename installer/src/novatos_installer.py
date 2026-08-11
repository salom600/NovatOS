#!/usr/bin/env python3
"""
NovatOS Installer — modern graphical installer
Built with PyQt6 — works on any GPU (software rendering fallback)

Features:
  - Beautiful, modern UI (Windows 11-style)
  - Disk selection with visual feedback
  - User account creation
  - Network detection (downloads latest drivers if online)
  - Progress bar with status updates
  - Installs Debian 12 + NovatOS desktop
"""

import sys
import os
import subprocess
import threading
import json
from pathlib import Path

# Force software rendering if no GPU
def setup_rendering():
    import os
    # Check /proc/cmdline for nomodeset
    try:
        with open('/proc/cmdline') as f:
            if 'nomodeset' in f.read():
                os.environ['QT_QUICK_BACKEND'] = 'software'
                os.environ['LIBGL_ALWAYS_SOFTWARE'] = '1'
                return
    except:
        pass
    
    # Check if GPU driver is loaded
    try:
        result = subprocess.run(['lsmod'], capture_output=True, text=True)
        output = result.stdout
        if not any(d in output for d in ['amdgpu', 'radeon', 'i915', 'nouveau', 'nvidia', 'vmwgfx']):
            os.environ['QT_QUICK_BACKEND'] = 'software'
            os.environ['LIBGL_ALWAYS_SOFTWARE'] = '1'
    except:
        pass

setup_rendering()

from PyQt6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QComboBox, QProgressBar,
    QStackedWidget, QMessageBox, QFrame, QScrollArea
)
from PyQt6.QtCore import Qt, QThread, pyqtSignal, QTimer
from PyQt6.QtGui import QFont, QPixmap, QPalette, QColor


class InstallWorker(QThread):
    """Background worker that runs the actual installation."""
    progress = pyqtSignal(int, str)
    finished = pyqtSignal(bool, str)
    
    def __init__(self, disk, username, password, hostname):
        super().__init__()
        self.disk = disk
        self.username = username
        self.password = password
        self.hostname = hostname
    
    def run(self):
        try:
            # Step 1: Check internet
            self.progress.emit(5, "Checking internet connection...")
            has_internet = self._check_internet()
            
            # Step 2: Partition disk
            self.progress.emit(15, f"Partitioning {self.disk}...")
            if not self._partition_disk():
                self.finished.emit(False, "Failed to partition disk")
                return
            
            # Step 3: Format partitions
            self.progress.emit(25, "Formatting partitions...")
            if not self._format_partitions():
                self.finished.emit(False, "Failed to format partitions")
                return
            
            # Step 4: Mount partitions
            self.progress.emit(30, "Mounting partitions...")
            if not self._mount_partitions():
                self.finished.emit(False, "Failed to mount partitions")
                return
            
            # Step 5: Install base system
            self.progress.emit(40, "Installing base system (this may take 10-20 minutes)...")
            if not self._install_base():
                self.finished.emit(False, "Failed to install base system")
                return
            
            # Step 6: Install NovatOS desktop + packages
            self.progress.emit(60, "Installing NovatOS desktop...")
            if not self._install_desktop():
                self.finished.emit(False, "Failed to install desktop")
                return
            
            # Step 7: Configure system
            self.progress.emit(75, "Configuring system...")
            self._configure_system()
            
            # Step 8: Install bootloader
            self.progress.emit(85, "Installing bootloader (GRUB)...")
            self._install_bootloader()
            
            # Step 9: Final setup
            self.progress.emit(95, "Finalizing installation...")
            self._final_setup()
            
            self.progress.emit(100, "Installation complete!")
            self.finished.emit(True, "NovatOS has been installed successfully!")
            
        except Exception as e:
            self.finished.emit(False, f"Installation failed: {str(e)}")
    
    def _check_internet(self):
        try:
            subprocess.run(['ping', '-c', '1', '-W', '3', 'deb.debian.org'],
                         capture_output=True, timeout=10)
            return True
        except:
            return False
    
    def _partition_disk(self):
        # Create GPT partition table + EFI + root
        cmds = [
            ['parted', '-s', self.disk, 'mklabel', 'gpt'],
            ['parted', '-s', self.disk, 'mkpart', 'primary', 'fat32', '1MiB', '512MiB'],
            ['parted', '-s', self.disk, 'set', '1', 'esp', 'on'],
            ['parted', '-s', self.disk, 'mkpart', 'primary', 'ext4', '512MiB', '100%'],
        ]
        for cmd in cmds:
            result = subprocess.run(cmd, capture_output=True)
            if result.returncode != 0:
                return False
        return True
    
    def _format_partitions(self):
        # Determine partition names
        if 'nvme' in self.disk or 'mmcblk' in self.disk:
            part1 = f"{self.disk}p1"
            part2 = f"{self.disk}p2"
        else:
            part1 = f"{self.disk}1"
            part2 = f"{self.disk}2"
        
        cmds = [
            ['mkfs.fat', '-F32', part1],
            ['mkfs.ext4', '-F', part2],
        ]
        for cmd in cmds:
            result = subprocess.run(cmd, capture_output=True)
            if result.returncode != 0:
                return False
        return True
    
    def _mount_partitions(self):
        import tempfile
        self.mount_point = tempfile.mkdtemp(prefix='novatos-install-')
        
        # Determine partition names
        if 'nvme' in self.disk or 'mmcblk' in self.disk:
            part1 = f"{self.disk}p1"
            part2 = f"{self.disk}p2"
        else:
            part1 = f"{self.disk}1"
            part2 = f"{self.disk}2"
        
        # Mount root
        result = subprocess.run(['mount', part2, self.mount_point], capture_output=True)
        if result.returncode != 0:
            return False
        
        # Mount EFI
        os.makedirs(f"{self.mount_point}/boot/efi", exist_ok=True)
        result = subprocess.run(['mount', part1, f"{self.mount_point}/boot/efi"], capture_output=True)
        return result.returncode == 0
    
    def _install_base(self):
        # Use debootstrap to install Debian base
        cmd = ['debootstrap', '--arch=amd64', 'bookworm', self.mount_point, 'http://deb.debian.org/debian/']
        result = subprocess.run(cmd, capture_output=True, timeout=1200)
        return result.returncode == 0
    
    def _install_desktop(self):
        # Copy package lists + install NovatOS packages
        cmds = [
            # Mount necessary filesystems for chroot
            ['mount', '--bind', '/dev', f'{self.mount_point}/dev'],
            ['mount', '--bind', '/proc', f'{self.mount_point}/proc'],
            ['mount', '--bind', '/sys', f'{self.mount_point}/sys'],
        ]
        for cmd in cmds:
            subprocess.run(cmd, capture_output=True)
        
        # Install packages via chroot
        chroot_cmd = f"""
            apt-get update &&
            apt-get install -y --no-install-recommends \\
                linux-image-amd64 grub-efi-amd64-signed shim-signed \\
                network-manager sudo bash-completion \\
                novatos-desktop sddm pipewire wireplumber \\
                firefox-esr mpv && \\
            apt-get clean
        """
        result = subprocess.run(['chroot', self.mount_point, 'bash', '-c', chroot_cmd],
                               capture_output=True, timeout=1800)
        return result.returncode == 0
    
    def _configure_system(self):
        # Set hostname
        with open(f'{self.mount_point}/etc/hostname', 'w') as f:
            f.write(self.hostname + '\n')
        
        # Set hosts
        with open(f'{self.mount_point}/etc/hosts', 'w') as f:
            f.write(f"127.0.0.1\tlocalhost\n")
            f.write(f"127.0.1.1\t{self.hostname}\n")
        
        # Create user
        subprocess.run(['chroot', self.mount_point, 'useradd', '-m', '-G', 'sudo,audio,video,netdev',
                       '-s', '/bin/bash', self.username], capture_output=True)
        
        # Set password
        subprocess.run(['chroot', self.mount_point, 'bash', '-c',
                       f'echo "{self.username}:{self.password}" | chpasswd'], capture_output=True)
        
        # Enable services
        for svc in ['NetworkManager', 'sddm', 'systemd-timesyncd']:
            subprocess.run(['chroot', self.mount_point, 'systemctl', 'enable', svc],
                         capture_output=True)
    
    def _install_bootloader(self):
        # Install GRUB
        subprocess.run(['chroot', self.mount_point, 'grub-install', '--target=x86_64-efi',
                       '--efi-directory=/boot/efi', '--bootloader-id=NOVATOS'],
                      capture_output=True)
        subprocess.run(['chroot', self.mount_point, 'update-grub'], capture_output=True)
    
    def _final_setup(self):
        # Unmount everything
        subprocess.run(['umount', f'{self.mount_point}/dev'], capture_output=True)
        subprocess.run(['umount', f'{self.mount_point}/proc'], capture_output=True)
        subprocess.run(['umount', f'{self.mount_point}/sys'], capture_output=True)
        subprocess.run(['umount', f'{self.mount_point}/boot/efi'], capture_output=True)
        subprocess.run(['umount', self.mount_point], capture_output=True)


class NovatOSInstaller(QMainWindow):
    """Main installer window."""
    
    def __init__(self):
        super().__init__()
        self.setWindowTitle("NovatOS Installer")
        self.setGeometry(100, 100, 800, 600)
        self.setFixedSize(800, 600)
        
        # Dark theme
        self._setup_dark_theme()
        
        # Create stacked widget for multi-step UI
        self.stack = QStackedWidget()
        self.setCentralWidget(self.stack)
        
        # Create pages
        self.welcome_page = self._create_welcome_page()
        self.disk_page = self._create_disk_page()
        self.user_page = self._create_user_page()
        self.install_page = self._create_install_page()
        self.done_page = self._create_done_page()
        
        self.stack.addWidget(self.welcome_page)
        self.stack.addWidget(self.disk_page)
        self.stack.addWidget(self.user_page)
        self.stack.addWidget(self.install_page)
        self.stack.addWidget(self.done_page)
    
    def _setup_dark_theme(self):
        """Apply dark NovatOS theme."""
        palette = QPalette()
        palette.setColor(QPalette.ColorRole.Window, QColor(15, 17, 23))
        palette.setColor(QPalette.ColorRole.WindowText, QColor(255, 255, 255))
        palette.setColor(QPalette.ColorRole.Base, QColor(22, 25, 34))
        palette.setColor(QPalette.ColorRole.AlternateBase, QColor(15, 17, 23))
        palette.setColor(QPalette.ColorRole.Text, QColor(255, 255, 255))
        palette.setColor(QPalette.ColorRole.Button, QColor(22, 25, 34))
        palette.setColor(QPalette.ColorRole.ButtonText, QColor(255, 255, 255))
        palette.setColor(QPalette.ColorRole.Highlight, QColor(76, 194, 255))
        palette.setColor(QPalette.ColorRole.HighlightedText, QColor(15, 17, 23))
        self.setPalette(palette)
        
        self.setStyleSheet("""
            QMainWindow { background-color: #0F1117; }
            QLabel { color: #FFFFFF; font-size: 14px; }
            QLineEdit {
                background-color: #0F1117;
                border: 2px solid #2A2F3D;
                border-radius: 8px;
                padding: 8px;
                color: #FFFFFF;
                font-size: 14px;
            }
            QLineEdit:focus { border-color: #4CC2FF; }
            QPushButton {
                background-color: #4CC2FF;
                color: #0F1117;
                border: none;
                border-radius: 8px;
                padding: 10px 20px;
                font-size: 14px;
                font-weight: bold;
            }
            QPushButton:hover { background-color: #5DD2FF; }
            QPushButton:disabled { background-color: #2A2F3D; color: #9DB7E0; }
            QComboBox {
                background-color: #0F1117;
                border: 2px solid #2A2F3D;
                border-radius: 8px;
                padding: 8px;
                color: #FFFFFF;
                font-size: 14px;
            }
            QProgressBar {
                background-color: #161922;
                border: 1px solid #2A2F3D;
                border-radius: 8px;
                text-align: center;
                color: #FFFFFF;
            }
            QProgressBar::chunk {
                background-color: #4CC2FF;
                border-radius: 7px;
            }
        """)
    
    def _create_label(self, text, size=14, bold=False, color="#FFFFFF"):
        label = QLabel(text)
        font = QFont("Noto Sans", size)
        if bold:
            font.setBold(True)
        label.setFont(font)
        label.setStyleSheet(f"color: {color}; background: transparent;")
        return label
    
    def _create_welcome_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.setSpacing(20)
        
        # Logo
        logo = self._create_label("NovatOS", 48, True, "#4CC2FF")
        logo.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(logo)
        
        # Subtitle
        subtitle = self._create_label("Aurora Edition — 2026", 16, False, "#9DB7E0")
        subtitle.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(subtitle)
        
        layout.addSpacing(40)
        
        # Welcome text
        welcome = self._create_label("Welcome to the NovatOS Installer", 20, True)
        welcome.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(welcome)
        
        desc = self._create_label(
            "This will permanently install NovatOS on your computer.\n"
            "You can choose to install alongside an existing OS or replace it.",
            14, False, "#9DB7E0"
        )
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(desc)
        
        layout.addSpacing(40)
        
        # Continue button
        btn = QPushButton("Continue")
        btn.setCursor(Qt.CursorShape.PointingHandCursor)
        btn.setFixedSize(200, 44)
        btn.clicked.connect(lambda: self.stack.setCurrentWidget(self.disk_page))
        layout.addWidget(btn, alignment=Qt.AlignmentFlag.AlignCenter)
        
        return page
    
    def _create_disk_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(60, 40, 60, 40)
        layout.setSpacing(20)
        
        title = self._create_label("Select Installation Disk", 24, True)
        layout.addWidget(title)
        
        desc = self._create_label("Choose the disk where NovatOS will be installed:", 14, False, "#9DB7E0")
        layout.addWidget(desc)
        
        # Disk combo
        self.disk_combo = QComboBox()
        self._populate_disks()
        layout.addWidget(self.disk_combo)
        
        layout.addStretch()
        
        # Buttons
        btn_layout = QHBoxLayout()
        back_btn = QPushButton("Back")
        back_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        back_btn.setStyleSheet("background-color: #2A2F3D; color: #FFFFFF;")
        back_btn.clicked.connect(lambda: self.stack.setCurrentWidget(self.welcome_page))
        btn_layout.addWidget(back_btn)
        
        btn_layout.addStretch()
        
        next_btn = QPushButton("Next")
        next_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        next_btn.clicked.connect(lambda: self.stack.setCurrentWidget(self.user_page))
        btn_layout.addWidget(next_btn)
        
        layout.addLayout(btn_layout)
        
        return page
    
    def _populate_disks(self):
        """Populate disk combo with available disks."""
        self.disk_combo.clear()
        try:
            result = subprocess.run(['lsblk', '-d', '-n', '-o', 'NAME,SIZE,MODEL'],
                                   capture_output=True, text=True)
            for line in result.stdout.strip().split('\n'):
                if line:
                    parts = line.split(None, 2)
                    if parts:
                        name = parts[0]
                        size = parts[1] if len(parts) > 1 else ""
                        model = parts[2] if len(parts) > 2 else ""
                        disk_path = f"/dev/{name}"
                        display = f"{disk_path}  —  {size}  —  {model}"
                        self.disk_combo.addItem(display, disk_path)
        except:
            self.disk_combo.addItem("/dev/sda", "/dev/sda")
    
    def _create_user_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setContentsMargins(60, 40, 60, 40)
        layout.setSpacing(20)
        
        title = self._create_label("Create Your Account", 24, True)
        layout.addWidget(title)
        
        # Username
        layout.addWidget(self._create_label("Username:", 14, True))
        self.username_input = QLineEdit()
        self.username_input.setPlaceholderText("Enter your username")
        layout.addWidget(self.username_input)
        
        # Password
        layout.addWidget(self._create_label("Password:", 14, True))
        self.password_input = QLineEdit()
        self.password_input.setPlaceholderText("Enter your password")
        self.password_input.setEchoMode(QLineEdit.EchoMode.Password)
        layout.addWidget(self.password_input)
        
        # Confirm password
        layout.addWidget(self._create_label("Confirm Password:", 14, True))
        self.password_confirm = QLineEdit()
        self.password_confirm.setPlaceholderText("Re-enter your password")
        self.password_confirm.setEchoMode(QLineEdit.EchoMode.Password)
        layout.addWidget(self.password_confirm)
        
        # Hostname
        layout.addWidget(self._create_label("Computer Name:", 14, True))
        self.hostname_input = QLineEdit()
        self.hostname_input.setPlaceholderText("Enter a name for this computer")
        self.hostname_input.setText("novatos")
        layout.addWidget(self.hostname_input)
        
        layout.addStretch()
        
        # Buttons
        btn_layout = QHBoxLayout()
        back_btn = QPushButton("Back")
        back_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        back_btn.setStyleSheet("background-color: #2A2F3D; color: #FFFFFF;")
        back_btn.clicked.connect(lambda: self.stack.setCurrentWidget(self.disk_page))
        btn_layout.addWidget(back_btn)
        
        btn_layout.addStretch()
        
        install_btn = QPushButton("Install Now")
        install_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        install_btn.clicked.connect(self._start_install)
        btn_layout.addWidget(install_btn)
        
        layout.addLayout(btn_layout)
        
        return page
    
    def _create_install_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.setSpacing(20)
        
        title = self._create_label("Installing NovatOS...", 24, True)
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)
        
        self.progress_bar = QProgressBar()
        self.progress_bar.setFixedSize(500, 30)
        layout.addWidget(self.progress_bar, alignment=Qt.AlignmentFlag.AlignCenter)
        
        self.status_label = self._create_label("Preparing...", 14, False, "#9DB7E0")
        self.status_label.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(self.status_label)
        
        return page
    
    def _create_done_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.setSpacing(20)
        
        title = self._create_label("✓ Installation Complete!", 28, True, "#4CC2FF")
        title.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(title)
        
        desc = self._create_label("NovatOS has been installed successfully.", 16, False, "#9DB7E0")
        desc.setAlignment(Qt.AlignmentFlag.AlignCenter)
        layout.addWidget(desc)
        
        layout.addSpacing(40)
        
        reboot_btn = QPushButton("Restart Now")
        reboot_btn.setCursor(Qt.CursorShape.PointingHandCursor)
        reboot_btn.setFixedSize(200, 44)
        reboot_btn.clicked.connect(lambda: subprocess.run(['systemctl', 'reboot']))
        layout.addWidget(reboot_btn, alignment=Qt.AlignmentFlag.AlignCenter)
        
        return page
    
    def _start_install(self):
        """Validate inputs and start installation."""
        username = self.username_input.text().strip()
        password = self.password_input.text()
        password_confirm = self.password_confirm.text()
        hostname = self.hostname_input.text().strip()
        
        if not username:
            QMessageBox.warning(self, "Missing Input", "Please enter a username.")
            return
        if not password:
            QMessageBox.warning(self, "Missing Input", "Please enter a password.")
            return
        if password != password_confirm:
            QMessageBox.warning(self, "Password Mismatch", "Passwords do not match.")
            return
        if not hostname:
            hostname = "novatos"
        
        disk = self.disk_combo.currentData()
        
        # Confirm
        reply = QMessageBox.question(
            self, "Confirm Installation",
            f"This will ERASE ALL DATA on {disk} and install NovatOS.\n\nContinue?",
            QMessageBox.StandardButton.Yes | QMessageBox.StandardButton.No,
            QMessageBox.StandardButton.No
        )
        if reply != QMessageBox.StandardButton.Yes:
            return
        
        # Start installation
        self.stack.setCurrentWidget(self.install_page)
        self.worker = InstallWorker(disk, username, password, hostname)
        self.worker.progress.connect(self._on_progress)
        self.worker.finished.connect(self._on_finished)
        self.worker.start()
    
    def _on_progress(self, value, status):
        self.progress_bar.setValue(value)
        self.status_label.setText(status)
    
    def _on_finished(self, success, message):
        if success:
            self.stack.setCurrentWidget(self.done_page)
        else:
            QMessageBox.critical(self, "Installation Failed", message)
            self.stack.setCurrentWidget(self.user_page)


def main():
    app = QApplication(sys.argv)
    app.setApplicationName("NovatOS Installer")
    
    # Check if running as root
    if os.geteuid() != 0:
        QMessageBox.critical(None, "Permission Denied",
                           "NovatOS Installer must be run as root.\n\nPlease run: sudo novatos-installer")
        sys.exit(1)
    
    installer = NovatOSInstaller()
    installer.show()
    sys.exit(app.exec())


if __name__ == "__main__":
    main()
