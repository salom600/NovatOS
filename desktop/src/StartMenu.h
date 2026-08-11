/*
 * NovatOS Desktop — StartMenu.h
 * Windows 11-style Start menu (app launcher)
 */

#pragma once

#include <QWidget>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>

class StartMenu : public QWidget {
    Q_OBJECT

public:
    explicit StartMenu(QWidget *parent = nullptr);
    ~StartMenu();

    QSize sizeHint() const override { return QSize(500, 500); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSearchChanged(const QString &text);
    void onAppSelected();
    void onPowerOff();
    void onReboot();
    void onLock();
    void onSettings();

private:
    void setupUI();
    void loadApplications();
    
    QLineEdit *m_searchBox;
    QListWidget *m_appList;
    QPushButton *m_powerOffButton;
    QPushButton *m_rebootButton;
    QPushButton *m_lockButton;
    QPushButton *m_settingsButton;
    
    struct AppEntry {
        QString name;
        QString exec;
        QString icon;
    };
    QVector<AppEntry> m_apps;
};
