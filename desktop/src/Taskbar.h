/*
 * NovatOS Desktop — Taskbar.h
 * Windows 11-style taskbar at the bottom of the screen
 */

#pragma once

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QVector>
#include <QToolButton>

class Taskbar : public QWidget {
    Q_OBJECT

public:
    explicit Taskbar(QWidget *parent = nullptr);
    ~Taskbar();

    int height() const { return 48; }

signals:
    void startMenuClicked();
    void lockClicked();

protected:
    void paintEvent(QPaintEvent *event) override;

private slots:
    void onStartClicked();
    void onLockClicked();
    void updateClock();

private:
    void setupUI();
    
    QPushButton *m_startButton;
    QLabel *m_clockLabel;
    QToolButton *m_networkButton;
    QToolButton *m_volumeButton;
    QToolButton *m_batteryButton;
    QToolButton *m_lockButton;
};

// Taskbar application button (for running apps)
class TaskbarAppButton : public QToolButton {
    Q_OBJECT
public:
    explicit TaskbarAppButton(const QString &appName, QWidget *parent = nullptr);
};
