/*
 * NovatOS Desktop — WindowManager.cpp
 * Tracks running applications (basic implementation).
 */

#include "WindowManager.h"
#include <QProcess>
#include <QFile>
#include <QTextStream>

WindowManager::WindowManager(QObject *parent)
    : QObject(parent)
    , m_pollTimer(nullptr)
{
    // Poll for window list every 2 seconds
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &WindowManager::pollWindows);
    m_pollTimer->start(2000);
}

WindowManager::~WindowManager() {}

void WindowManager::pollWindows() {
    // TODO: Use wlr-foreign-toplevel-management for Wayland
    // For now, this is a placeholder that could use xdotool or wmctrl on X11
    // The actual window list is updated when applications are launched
}
