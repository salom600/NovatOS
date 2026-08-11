/*
 * NovatOS Desktop — DesktopShell.cpp
 * Implements the main desktop shell with taskbar, start menu, and wallpaper.
 */

#include "DesktopShell.h"
#include "Taskbar.h"
#include "StartMenu.h"
#include "LockScreen.h"
#include "WindowManager.h"

#include <QPainter>
#include <QScreen>
#include <QApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QKeyEvent>
#include <QCloseEvent>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QStandardPaths>

DesktopShell::DesktopShell(QWidget *parent)
    : QWidget(parent)
    , m_taskbar(nullptr)
    , m_startMenu(nullptr)
    , m_lockScreen(nullptr)
    , m_windowManager(nullptr)
    , m_trayIcon(nullptr)
    , m_startMenuVisible(false)
    , m_locked(false)
{
    setWindowTitle("NovatOS Desktop");
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnBottomHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    
    // Cover the full screen
    QScreen *screen = QApplication::primaryScreen();
    if (screen) {
        setGeometry(screen->geometry());
    }
    
    setupUI();
    loadWallpaper();
    createTrayIcon();
}

DesktopShell::~DesktopShell() {
    // Children are automatically deleted by Qt parent-child mechanism
}

void DesktopShell::setupUI() {
    // Create window manager
    m_windowManager = new WindowManager(this);
    
    // Create taskbar at bottom of screen
    m_taskbar = new Taskbar(this);
    m_taskbar->move(0, height() - m_taskbar->height());
    m_taskbar->resize(width(), m_taskbar->height());
    
    // Create start menu (hidden by default)
    m_startMenu = new StartMenu(this);
    m_startMenu->hide();
    
    // Create lock screen (hidden by default)
    m_lockScreen = new LockScreen(this);
    m_lockScreen->hide();
    
    // Connect signals
    connect(m_taskbar, &Taskbar::startMenuClicked, this, &DesktopShell::toggleStartMenu);
    connect(m_taskbar, &Taskbar::lockClicked, this, &DesktopShell::toggleLockScreen);
}

void DesktopShell::loadWallpaper() {
    // Try to load NovatOS wallpaper
    QStringList wallpaperPaths = {
        "/usr/share/novatos/wallpapers/aurora.png",
        "/usr/share/novatos/wallpapers/novatos.png",
        "/usr/share/wallpapers/novatos/NovatOS-Aurora.png",
    };
    
    for (const QString &path : wallpaperPaths) {
        if (QFile::exists(path) && m_wallpaper.load(path)) {
            qDebug() << "Loaded wallpaper:" << path;
            return;
        }
    }
    
    // Fallback: create a gradient wallpaper
    qDebug() << "No wallpaper found, using gradient fallback";
    m_wallpaper = QPixmap(width(), height());
    QPainter painter(&m_wallpaper);
    QLinearGradient gradient(0, 0, 0, height());
    gradient.setColorAt(0, QColor(10, 12, 18));
    gradient.setColorAt(1, QColor(15, 17, 23));
    painter.fillRect(m_wallpaper.rect(), gradient);
    
    // Add NovatOS text in center
    painter.setPen(QColor(76, 194, 255, 80));
    QFont font = painter.font();
    font.setPointSize(48);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(m_wallpaper.rect(), Qt::AlignCenter, "NovatOS");
}

void DesktopShell::createTrayIcon() {
    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setToolTip("NovatOS Desktop");
    
    // Create tray menu
    QMenu *menu = new QMenu(this);
    menu->setStyleSheet(
        "QMenu { background-color: #161922; color: #FFFFFF; border: 1px solid #2A2F3D; }"
        "QMenu::item:selected { background-color: #4CC2FF; color: #0F1117; }"
    );
    
    QAction *lockAction = menu->addAction("Lock Screen");
    connect(lockAction, &QAction::triggered, this, &DesktopShell::toggleLockScreen);
    
    QAction *settingsAction = menu->addAction("Settings");
    connect(settingsAction, &QAction::triggered, []() {
        system("novatos-settings &");
    });
    
    menu->addSeparator();
    
    QAction *restartAction = menu->addAction("Restart");
    connect(restartAction, &QAction::triggered, []() {
        system("systemctl reboot");
    });
    
    QAction *shutdownAction = menu->addAction("Shut Down");
    connect(shutdownAction, &QAction::triggered, []() {
        system("systemctl poweroff");
    });
    
    m_trayIcon->setContextMenu(menu);
    m_trayIcon->show();
    
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &DesktopShell::onTrayActivated);
}

void DesktopShell::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Draw wallpaper (scaled to fit)
    if (!m_wallpaper.isNull()) {
        painter.drawPixmap(rect(), m_wallpaper.scaled(size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    } else {
        painter.fillRect(rect(), QColor(15, 17, 23));
    }
}

void DesktopShell::keyPressEvent(QKeyEvent *event) {
    // Super key (Meta) toggles start menu
    if (event->key() == Qt::Key_Super_L || event->key() == Qt::Key_Super_R) {
        toggleStartMenu();
    }
    // Ctrl+Alt+L locks the screen
    else if (event->key() == Qt::Key_L && (event->modifiers() & Qt::ControlModifier) && (event->modifiers() & Qt::AltModifier)) {
        toggleLockScreen();
    }
    // Escape closes start menu
    else if (event->key() == Qt::Key_Escape && m_startMenuVisible) {
        toggleStartMenu();
    }
}

void DesktopShell::closeEvent(QCloseEvent *event) {
    // Prevent closing — this is the desktop shell
    event->ignore();
}

void DesktopShell::toggleStartMenu() {
    if (m_locked) return;
    
    m_startMenuVisible = !m_startMenuVisible;
    if (m_startMenuVisible) {
        // Position start menu above taskbar, left-aligned
        int x = 20;
        int y = height() - m_taskbar->height() - m_startMenu->height() - 10;
        m_startMenu->move(x, y);
        m_startMenu->show();
        m_startMenu->raise();
    } else {
        m_startMenu->hide();
    }
}

void DesktopShell::toggleLockScreen() {
    m_locked = !m_locked;
    if (m_locked) {
        m_lockScreen->setGeometry(rect());
        m_lockScreen->show();
        m_lockScreen->raise();
    } else {
        m_lockScreen->hide();
    }
}

void DesktopShell::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger) {
        toggleStartMenu();
    }
}
