/*
 * NovatOS Desktop — WindowManager.h
 * Basic window management (tracks running applications)
 */

#pragma once

#include <QObject>
#include <QVector>
#include <QTimer>

struct WindowInfo {
    int id;
    QString title;
    QString app;
    bool focused;
};

class WindowManager : public QObject {
    Q_OBJECT

public:
    explicit WindowManager(QObject *parent = nullptr);
    ~WindowManager();

    QVector<WindowInfo> windows() const { return m_windows; }

signals:
    void windowAdded(const WindowInfo &info);
    void windowRemoved(int id);
    void windowFocused(int id);

private slots:
    void pollWindows();

private:
    QVector<WindowInfo> m_windows;
    QTimer *m_pollTimer;
};
