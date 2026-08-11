/*
 * NovatOS Desktop — DesktopShell.h
 * The main desktop shell: background, taskbar, start menu, windows
 */

#pragma once

#include <QWidget>
#include <QPixmap>
#include <QSystemTrayIcon>

class Taskbar;
class StartMenu;
class LockScreen;
class WindowManager;

class DesktopShell : public QWidget {
    Q_OBJECT

public:
    explicit DesktopShell(QWidget *parent = nullptr);
    ~DesktopShell();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private slots:
    void toggleStartMenu();
    void toggleLockScreen();
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);

private:
    void setupUI();
    void loadWallpaper();
    void createTrayIcon();
    
    QPixmap m_wallpaper;
    Taskbar *m_taskbar;
    StartMenu *m_startMenu;
    LockScreen *m_lockScreen;
    WindowManager *m_windowManager;
    QSystemTrayIcon *m_trayIcon;
    bool m_startMenuVisible;
    bool m_locked;
};
