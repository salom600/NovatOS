/*
 * NovatOS Desktop — StartMenu.cpp
 * Implements the Start menu with app search + launch + power controls.
 */

#include "StartMenu.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPainter>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QProcess>
#include <QIcon>
#include <QDebug>

StartMenu::StartMenu(QWidget *parent)
    : QWidget(parent)
    , m_searchBox(nullptr)
    , m_appList(nullptr)
    , m_powerOffButton(nullptr)
    , m_rebootButton(nullptr)
    , m_lockButton(nullptr)
    , m_settingsButton(nullptr)
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setFixedSize(500, 500);
    
    setStyleSheet(
        "StartMenu { "
        "  background-color: rgba(22, 25, 34, 0.98); "
        "  border: 1px solid rgba(76, 194, 255, 0.3); "
        "  border-radius: 16px; "
        "} "
        "QLineEdit { "
        "  background-color: #0F1117; "
        "  border: 2px solid #2A2F3D; "
        "  border-radius: 10px; "
        "  padding: 10px 14px; "
        "  color: #FFFFFF; "
        "  font-size: 14px; "
        "} "
        "QLineEdit:focus { "
        "  border-color: #4CC2FF; "
        "} "
        "QListWidget { "
        "  background-color: transparent; "
        "  border: none; "
        "  color: #FFFFFF; "
        "  font-size: 14px; "
        "} "
        "QListWidget::item { "
        "  padding: 8px 12px; "
        "  border-radius: 8px; "
        "} "
        "QListWidget::item:selected { "
        "  background-color: rgba(76, 194, 255, 0.2); "
        "  color: #4CC2FF; "
        "} "
        "QPushButton { "
        "  background-color: transparent; "
        "  border: none; "
        "  border-radius: 8px; "
        "  padding: 8px 12px; "
        "  color: #9DB7E0; "
        "  font-size: 12px; "
        "} "
        "QPushButton:hover { "
        "  background-color: rgba(255, 255, 255, 0.1); "
        "  color: #FFFFFF; "
        "} "
    );
    
    setupUI();
    loadApplications();
}

StartMenu::~StartMenu() {}

void StartMenu::setupUI() {
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(12);
    
    // Header
    QLabel *header = new QLabel("NovatOS", this);
    header->setStyleSheet(
        "QLabel { color: #4CC2FF; font-size: 24px; font-weight: bold; }"
    );
    layout->addWidget(header);
    
    // Search box
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Search applications...");
    m_searchBox->setCursor(Qt::IBeamCursor);
    connect(m_searchBox, &QLineEdit::textChanged, this, &StartMenu::onSearchChanged);
    layout->addWidget(m_searchBox);
    
    // App list
    m_appList = new QListWidget(this);
    m_appList->setCursor(Qt::PointingHandCursor);
    connect(m_appList, &QListWidget::itemDoubleClicked, this, &StartMenu::onAppSelected);
    connect(m_appList, &QListWidget::itemActivated, this, &StartMenu::onAppSelected);
    layout->addWidget(m_appList);
    
    // Bottom buttons (power controls)
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(8);
    
    m_settingsButton = new QPushButton("⚙ Settings", this);
    connect(m_settingsButton, &QPushButton::clicked, this, &StartMenu::onSettings);
    bottomLayout->addWidget(m_settingsButton);
    
    bottomLayout->addStretch();
    
    m_lockButton = new QPushButton("🔒 Lock", this);
    connect(m_lockButton, &QPushButton::clicked, this, &StartMenu::onLock);
    bottomLayout->addWidget(m_lockButton);
    
    m_rebootButton = new QPushButton("↻ Restart", this);
    connect(m_rebootButton, &QPushButton::clicked, this, &StartMenu::onReboot);
    bottomLayout->addWidget(m_rebootButton);
    
    m_powerOffButton = new QPushButton("⏻ Shut Down", this);
    m_powerOffButton->setStyleSheet(
        "QPushButton { color: #FF6B6B; } QPushButton:hover { background-color: rgba(255, 107, 107, 0.2); }"
    );
    connect(m_powerOffButton, &QPushButton::clicked, this, &StartMenu::onPowerOff);
    bottomLayout->addWidget(m_powerOffButton);
    
    layout->addLayout(bottomLayout);
}

void StartMenu::loadApplications() {
    m_apps.clear();
    
    // Scan .desktop files in standard locations
    QStringList appDirs = {
        "/usr/share/applications",
        "/usr/local/share/applications",
        QDir::homePath() + "/.local/share/applications"
    };
    
    for (const QString &dirPath : appDirs) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;
        
        QStringList filters;
        filters << "*.desktop";
        dir.setNameFilters(filters);
        
        QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::Readable);
        for (const QFileInfo &fileInfo : files) {
            QFile file(fileInfo.filePath());
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            
            QTextStream in(&file);
            QString name, exec, icon;
            bool noDisplay = false;
            
            while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.startsWith("Name=") && name.isEmpty()) {
                    name = line.mid(5).trimmed();
                } else if (line.startsWith("Exec=") && exec.isEmpty()) {
                    exec = line.mid(5).trimmed().split(' ').first();
                } else if (line.startsWith("Icon=") && icon.isEmpty()) {
                    icon = line.mid(5).trimmed();
                } else if (line.startsWith("NoDisplay=true")) {
                    noDisplay = true;
                    break;
                }
            }
            
            if (!noDisplay && !name.isEmpty() && !exec.isEmpty()) {
                m_apps.append({name, exec, icon});
            }
        }
    }
    
    // Sort alphabetically
    std::sort(m_apps.begin(), m_apps.end(), [](const AppEntry &a, const AppEntry &b) {
        return a.name.toLower() < b.name.toLower();
    });
    
    // Populate list
    m_appList->clear();
    for (const AppEntry &app : m_apps) {
        m_appList->addItem(app.name);
    }
}

void StartMenu::onSearchChanged(const QString &text) {
    m_appList->clear();
    
    QString lowerText = text.toLower();
    for (const AppEntry &app : m_apps) {
        if (app.name.toLower().contains(lowerText) || lowerText.isEmpty()) {
            m_appList->addItem(app.name);
        }
    }
}

void StartMenu::onAppSelected() {
    QListWidgetItem *item = m_appList->currentItem();
    if (!item) return;
    
    QString appName = item->text();
    for (const AppEntry &app : m_apps) {
        if (app.name == appName) {
            // Launch the application
            QProcess::startDetached(app.exec);
            hide();
            break;
        }
    }
}

void StartMenu::onPowerOff() {
    system("systemctl poweroff");
}

void StartMenu::onReboot() {
    system("systemctl reboot");
}

void StartMenu::onLock() {
    // Emit signal to parent to show lock screen
    QWidget *parent = parentWidget();
    if (parent) {
        // Find the DesktopShell and call its lock method
        QMetaObject::invokeMethod(parent, "toggleLockScreen");
    }
}

void StartMenu::onSettings() {
    system("novatos-settings &");
    hide();
}

void StartMenu::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);
    QWidget::paintEvent(event);
}

void StartMenu::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        onAppSelected();
    } else if (event->key() == Qt::Key_Escape) {
        hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}
