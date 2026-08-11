/*
 * NovatOS Desktop — LockScreen.h
 * Custom lock screen with NovatOS branding
 */

#pragma once

#include <QWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class LockScreen : public QWidget {
    Q_OBJECT

public:
    explicit LockScreen(QWidget *parent = nullptr);
    ~LockScreen();

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onUnlockClicked();

private:
    void setupUI();
    
    QLabel *m_clockLabel;
    QLabel *m_dateLabel;
    QLabel *m_logoLabel;
    QLineEdit *m_passwordField;
    QPushButton *m_unlockButton;
    
    QTimer *m_clockTimer;
};
