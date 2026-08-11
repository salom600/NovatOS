/*
 * NovatOS Desktop — Taskbar.cpp
 * Implements the Windows 11-style taskbar with Start button, clock, system tray.
 */

#include "Taskbar.h"
#include <QHBoxLayout>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QFont>
#include <QFile>
#include <QProcess>
#include <QDBusInterface>
#include <QDBusConnection>

Taskbar::Taskbar(QWidget *parent)
    : QWidget(parent)
    , m_startButton(nullptr)
    , m_clockLabel(nullptr)
    , m_networkButton(nullptr)
    , m_volumeButton(nullptr)
    , m_batteryButton(nullptr)
    , m_lockButton(nullptr)
{
    setAttribute(Qt::WA_StyledBackground);
    setStyleSheet(
        "Taskbar { "
        "  background-color: rgba(15, 17, 23, 0.92); "
        "  border-top: 1px solid rgba(76, 194, 255, 0.2); "
        "} "
        "QPushButton { "
        "  background-color: transparent; "
        "  border: none; "
        "  border-radius: 6px; "
        "  padding: 6px 12px; "
        "  color: #9DB7E0; "
        "  font-size: 13px; "
        "} "
        "QPushButton:hover { "
        "  background-color: rgba(76, 194, 255, 0.15); "
        "  color: #FFFFFF; "
        "} "
        "QToolButton { "
        "  background-color: transparent; "
        "  border: none; "
        "  border-radius: 6px; "
        "  padding: 6px; "
        "  color: #9DB7E0; "
        "} "
        "QToolButton:hover { "
        "  background-color: rgba(255, 255, 255, 0.1); "
        "  color: #FFFFFF; "
        "} "
        "QLabel { "
        "  color: #FFFFFF; "
        "  font-size: 13px; "
        "  padding: 0 12px; "
        "} "
    );
    
    setupUI();
    
    // Update clock every second
    QTimer *clockTimer = new QTimer(this);
    connect(clockTimer, &QTimer::timeout, this, &Taskbar::updateClock);
    clockTimer->start(1000);
    updateClock();
}

Taskbar::~Taskbar() {}

void Taskbar::setupUI() {
    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(4);
    
    // ─── Left: Start button ───
    m_startButton = new QPushButton("⊞  Start", this);
    m_startButton->setCursor(Qt::PointingHandCursor);
    m_startButton->setStyleSheet(
        "QPushButton { "
        "  background-color: rgba(76, 194, 255, 0.1); "
        "  color: #4CC2FF; "
        "  font-weight: bold; "
        "  padding: 8px 16px; "
        "  border-radius: 8px; "
        "} "
        "QPushButton:hover { "
        "  background-color: rgba(76, 194, 255, 0.25); "
        "} "
    );
    connect(m_startButton, &QPushButton::clicked, this, &Taskbar::onStartClicked);
    layout->addWidget(m_startButton);
    
    layout->addStretch();
    
    // ─── Right: System tray ───
    
    // Network button
    m_networkButton = new QToolButton(this);
    m_networkButton->setText("🌐");
    m_networkButton->setToolTip("Network");
    m_networkButton->setCursor(Qt::PointingHandCursor);
    connect(m_networkButton, &QToolButton::clicked, []() {
        system("nm-connection-editor &");
    });
    layout->addWidget(m_networkButton);
    
    // Volume button
    m_volumeButton = new QToolButton(this);
    m_volumeButton->setText("🔊");
    m_volumeButton->setToolTip("Volume");
    m_volumeButton->setCursor(Qt::PointingHandCursor);
    connect(m_volumeButton, &QToolButton::clicked, []() {
        system("pavucontrol &");
    });
    layout->addWidget(m_volumeButton);
    
    // Battery button
    m_batteryButton = new QToolButton(this);
    m_batteryButton->setText("🔋");
    m_batteryButton->setToolTip("Power");
    m_batteryButton->setCursor(Qt::PointingHandCursor);
    connect(m_batteryButton, &QToolButton::clicked, []() {
        system("novatos-power &");
    });
    layout->addWidget(m_batteryButton);
    
    // Clock
    m_clockLabel = new QLabel(this);
    m_clockLabel->setStyleSheet(
        "QLabel { "
        "  background-color: rgba(76, 194, 255, 0.1); "
        "  color: #4CC2FF; "
        "  padding: 4px 12px; "
        "  border-radius: 8px; "
        "  font-weight: 600; "
        "} "
    );
    layout->addWidget(m_clockLabel);
    
    // Lock button
    m_lockButton = new QToolButton(this);
    m_lockButton->setText("🔒");
    m_lockButton->setToolTip("Lock Screen");
    m_lockButton->setCursor(Qt::PointingHandCursor);
    connect(m_lockButton, &QToolButton::clicked, this, &Taskbar::onLockClicked);
    layout->addWidget(m_lockButton);
}

void Taskbar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    // The stylesheet handles the background
    QWidget::paintEvent(event);
}

void Taskbar::onStartClicked() {
    emit startMenuClicked();
}

void Taskbar::onLockClicked() {
    emit lockClicked();
}

void Taskbar::updateClock() {
    QDateTime now = QDateTime::currentDateTime();
    m_clockLabel->setText(now.toString("ddd MMM d  hh:mm"));
}

// ─── TaskbarAppButton ───
TaskbarAppButton::TaskbarAppButton(const QString &appName, QWidget *parent)
    : QToolButton(parent)
{
    setText(appName);
    setCursor(Qt::PointingHandCursor);
    setStyleSheet(
        "QToolButton { "
        "  background-color: rgba(76, 194, 255, 0.1); "
        "  color: #4CC2FF; "
        "  padding: 6px 12px; "
        "  border-radius: 6px; "
        "  border: none; "
        "} "
        "QToolButton:hover { "
        "  background-color: rgba(76, 194, 255, 0.2); "
        "} "
    );
}
