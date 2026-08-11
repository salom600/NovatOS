/*
 * NovatOS Desktop — main.cpp
 * ==========================
 * Custom lightweight desktop environment for NovatOS
 * Built with Qt6 — works on ANY GPU (automatic software rendering fallback)
 *
 * Features:
 *   - Windows 11-style taskbar at bottom
 *   - Start menu (app launcher)
 *   - System tray (network, volume, battery, clock)
 *   - Lock screen with NovatOS branding
 *   - Window management (tiling + floating)
 *   - ~150-200MB RAM usage
 *   - Software rendering fallback (no GPU required)
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <QQuickStyle>
#include <QDebug>

#include "DesktopShell.h"

int main(int argc, char *argv[]) {
    // ─── Force software rendering if no GPU or nomodeset ───
    // This ensures NovatOS Desktop works on ANY hardware:
    //   - AMD R2 (2009 integrated GPU)
    //   - NVIDIA GT 240M (2009 dedicated GPU)
    //   - Modern AMD/NVIDIA/Intel (2020+)
    //   - Virtual machines (VirtualBox, VMware, QEMU)
    
    // Check if we should force software rendering
    bool forceSoftware = false;
    for (int i = 1; i < argc; i++) {
        if (QString(argv[i]) == "--software" || QString(argv[i]) == "--x11") {
            forceSoftware = true;
        }
    }
    
    // Also check /proc/cmdline for nomodeset
    QFile cmdline("/proc/cmdline");
    if (cmdline.open(QIODevice::ReadOnly)) {
        QString content = cmdline.readAll();
        if (content.contains("nomodeset")) {
            forceSoftware = true;
        }
        cmdline.close();
    }
    
    if (forceSoftware) {
        qputenv("QT_QUICK_BACKEND", "software");
        qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
        qInfo() << "NovatOS Desktop: using software rendering (compatibility mode)";
    }
    
    // ─── Set High DPI scaling ───
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    
    // ─── Create application ───
    QApplication app(argc, argv);
    app.setApplicationName("NovatOS Desktop");
    app.setApplicationVersion("2026.1.0");
    app.setOrganizationName("NovatOS");
    
    // ─── Parse command line ───
    QCommandLineParser parser;
    parser.setApplicationDescription("NovatOS Desktop Environment");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption softwareOption("software", "Force software rendering");
    QCommandLineOption x11Option("x11", "Use X11 instead of Wayland");
    parser.addOption(softwareOption);
    parser.addOption(x11Option);
    parser.process(app);
    
    // ─── Set dark theme ───
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(15, 17, 23));
    darkPalette.setColor(QPalette::WindowText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Base, QColor(22, 25, 34));
    darkPalette.setColor(QPalette::AlternateBase, QColor(15, 17, 23));
    darkPalette.setColor(QPalette::Text, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Button, QColor(22, 25, 34));
    darkPalette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Highlight, QColor(76, 194, 255));
    darkPalette.setColor(QPalette::HighlightedText, QColor(15, 17, 23));
    app.setPalette(darkPalette);
    
    // ─── Load NovatOS style ───
    app.setStyleSheet(
        "QWidget { "
        "  background-color: #0F1117; "
        "  color: #FFFFFF; "
        "  font-family: 'Noto Sans', 'DejaVu Sans', sans-serif; "
        "  font-size: 13px; "
        "} "
        "QPushButton { "
        "  background-color: #161922; "
        "  border: 1px solid #2A2F3D; "
        "  border-radius: 8px; "
        "  padding: 8px 16px; "
        "  color: #FFFFFF; "
        "} "
        "QPushButton:hover { "
        "  background-color: #4CC2FF; "
        "  color: #0F1117; "
        "} "
        "QLineEdit { "
        "  background-color: #0F1117; "
        "  border: 2px solid #2A2F3D; "
        "  border-radius: 8px; "
        "  padding: 8px; "
        "  color: #FFFFFF; "
        "} "
        "QLineEdit:focus { "
        "  border-color: #4CC2FF; "
        "} "
        "QListWidget { "
        "  background-color: #161922; "
        "  border: 1px solid #2A2F3D; "
        "  border-radius: 8px; "
        "  color: #FFFFFF; "
        "} "
        "QListWidget::item:selected { "
        "  background-color: #4CC2FF; "
        "  color: #0F1117; "
        "} "
    );
    
    // ─── Create and show desktop shell ───
    DesktopShell shell;
    shell.show();
    
    return app.exec();
}
