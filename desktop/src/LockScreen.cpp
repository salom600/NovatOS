/*
 * NovatOS Desktop — LockScreen.cpp
 * Implements the lock screen with clock, NovatOS logo, and unlock field.
 */

#include "LockScreen.h"
#include <QVBoxLayout>
#include <QPainter>
#include <QTimer>
#include <QDateTime>
#include <QFont>
#include <QFile>
#include <QPixmap>

LockScreen::LockScreen(QWidget *parent)
    : QWidget(parent)
    , m_clockLabel(nullptr)
    , m_dateLabel(nullptr)
    , m_logoLabel(nullptr)
    , m_passwordField(nullptr)
    , m_unlockButton(nullptr)
    , m_clockTimer(nullptr)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_StyledBackground);
    
    setStyleSheet(
        "LockScreen { "
        "  background-color: rgba(15, 17, 23, 0.95); "
        "} "
        "QLabel { "
        "  color: #FFFFFF; "
        "  background: transparent; "
        "} "
        "QLineEdit { "
        "  background-color: #0F1117; "
        "  border: 2px solid #2A2F3D; "
        "  border-radius: 10px; "
        "  padding: 10px 14px; "
        "  color: #FFFFFF; "
        "  font-size: 14px; "
        "  min-width: 250px; "
        "} "
        "QLineEdit:focus { "
        "  border-color: #4CC2FF; "
        "} "
        "QPushButton { "
        "  background-color: #4CC2FF; "
        "  color: #0F1117; "
        "  border: none; "
        "  border-radius: 10px; "
        "  padding: 10px 20px; "
        "  font-size: 14px; "
        "  font-weight: bold; "
        "} "
        "QPushButton:hover { "
        "  background-color: #5DD2FF; "
        "} "
    );
    
    setupUI();
    
    // Update clock every second
    m_clockTimer = new QTimer(this);
    connect(m_clockTimer, &QTimer::timeout, this, [this]() {
        QDateTime now = QDateTime::currentDateTime();
        m_clockLabel->setText(now.toString("hh:mm"));
        m_dateLabel->setText(now.toString("dddd, MMMM d"));
    });
    m_clockTimer->start(1000);
}

LockScreen::~LockScreen() {}

void LockScreen::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setAlignment(Qt::AlignCenter);
    layout->setSpacing(20);
    
    // NovatOS logo
    m_logoLabel = new QLabel("NovatOS", this);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setStyleSheet(
        "QLabel { color: #4CC2FF; font-size: 48px; font-weight: bold; }"
    );
    layout->addWidget(m_logoLabel);
    
    // Subtitle
    QLabel *subtitle = new QLabel("Aurora Edition — 2026", this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet(
        "QLabel { color: #9DB7E0; font-size: 16px; }"
    );
    layout->addWidget(subtitle);
    
    // Spacer
    layout->addSpacing(40);
    
    // Clock
    m_clockLabel = new QLabel(this);
    m_clockLabel->setAlignment(Qt::AlignCenter);
    m_clockLabel->setStyleSheet(
        "QLabel { color: #FFFFFF; font-size: 72px; font-weight: bold; }"
    );
    layout->addWidget(m_clockLabel);
    
    // Date
    m_dateLabel = new QLabel(this);
    m_dateLabel->setAlignment(Qt::AlignCenter);
    m_dateLabel->setStyleSheet(
        "QLabel { color: #9DB7E0; font-size: 18px; }"
    );
    layout->addWidget(m_dateLabel);
    
    // Spacer
    layout->addSpacing(60);
    
    // Password field (hidden — we auto-login, but show field for visual consistency)
    m_passwordField = new QLineEdit(this);
    m_passwordField->setPlaceholderText("Press Enter to unlock (no password needed)");
    m_passwordField->setEchoMode(QLineEdit::Password);
    m_passwordField->setCursor(Qt::IBeamCursor);
    connect(m_passwordField, &QLineEdit::returnPressed, this, &LockScreen::onUnlockClicked);
    layout->addWidget(m_passwordField, 0, Qt::AlignCenter);
    
    // Unlock button
    m_unlockButton = new QPushButton("Unlock", this);
    m_unlockButton->setCursor(Qt::PointingHandCursor);
    connect(m_unlockButton, &QPushButton::clicked, this, &LockScreen::onUnlockClicked);
    layout->addWidget(m_unlockButton, 0, Qt::AlignCenter);
    
    // Trigger initial clock update
    m_clockTimer->start(1000);
}

void LockScreen::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QWidget::paintEvent(event);
}

void LockScreen::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Escape) {
        onUnlockClicked();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void LockScreen::onUnlockClicked() {
    // Auto-unlock (no password required on live ISO)
    hide();
    QWidget *parent = parentWidget();
    if (parent) {
        QMetaObject::invokeMethod(parent, "toggleLockScreen");
    }
}
