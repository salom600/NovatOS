/*
 * NovatOS SDDM theme — modern, Windows 11-style login screen
 * Dark background with cyan accents, centered card with username/password.
 * Auto-login is configured in sddm.conf.d/autologin.conf.
 */

import QtQuick 2.15
import QtQuick.Layouts 1.15
import SddmComponents 2.0

Item {
    id: root
    width: 1920
    height: 1080

    // Background image (NovatOS Aurora wallpaper)
    Image {
        anchors.fill: parent
        source: "background.png"
        fillMode: Image.PreserveAspectCrop
    }

    // Dim overlay for readability
    Rectangle {
        anchors.fill: parent
        color: "#0F1117"
        opacity: 0.55
    }

    // Centered login card
    Rectangle {
        id: loginCard
        width: 420
        height: 440
        anchors.centerIn: parent
        color: "#161922"
        radius: 20
        border.color: "#2A2F3D"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 16

            // NovatOS logo (using text since we don't have a logo image here)
            Text {
                text: "NovatOS"
                color: "#4CC2FF"
                font.pixelSize: 36
                font.weight: Font.Bold
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "Aurora Edition — 2026"
                color: "#9DB7E0"
                font.pixelSize: 14
                Layout.alignment: Qt.AlignHCenter
            }

            // Spacer
            Item { Layout.fillHeight: true; Layout.preferredHeight: 20 }

            // Username
            TextField {
                id: userField
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                placeholderText: "Username"
                text: "novatos"
                color: "#FFFFFF"
                font.pixelSize: 16
                selectByMouse: true

                background: Rectangle {
                    color: "#0F1117"
                    radius: 10
                    border.color: userField.activeFocus ? "#4CC2FF" : "#2A2F3D"
                    border.width: 2
                }

                onAccepted: pwField.focus = true
                Keys.onReleased: {
                    if (text === "") {
                        // Allow empty password
                    }
                }
            }

            // Password (can be empty — auto-login)
            TextField {
                id: pwField
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                placeholderText: "Password (leave empty for no password)"
                echoMode: TextInput.Password
                color: "#FFFFFF"
                font.pixelSize: 16
                selectByMouse: true

                background: Rectangle {
                    color: "#0F1117"
                    radius: 10
                    border.color: pwField.activeFocus ? "#4CC2FF" : "#2A2F3D"
                    border.width: 2
                }

                onAccepted: login()
            }

            // Session selector
            ComboBox {
                id: sessionCombo
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                model: sessionModel
                currentIndex: sessionModel.lastIndex
                font.pixelSize: 14
            }

            // Login button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: loginMouse.containsMouse ? "#5DD2FF" : "#4CC2FF"
                radius: 10

                Text {
                    anchors.centerIn: parent
                    text: "Sign in"
                    color: "#0F1117"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }

                MouseArea {
                    id: loginMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: login()
                }
            }

            // Error message
            Text {
                id: errorMsg
                Layout.fillWidth: true
                color: "#FF6B6B"
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                visible: text.length > 0
            }

            Item { Layout.fillHeight: true }
        }
    }

    // Bottom toolbar (power controls)
    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 16

        Repeater {
            model: [
                { name: "Sleep",    action: "suspend" },
                { name: "Restart",  action: "reboot"  },
                { name: "Shutdown", action: "poweroff" }
            ]
            Rectangle {
                width: 120
                height: 40
                color: powerMouse.containsMouse ? "#2A2F3D" : "#161922"
                radius: 10
                border.color: "#2A2F3D"
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: modelData.name
                    color: "#FFFFFF"
                    font.pixelSize: 13
                }

                MouseArea {
                    id: powerMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (modelData.action === "suspend") sddm.suspend()
                        else if (modelData.action === "reboot") sddm.reboot()
                        else if (modelData.action === "poweroff") sddm.powerOff()
                    }
                }
            }
        }
    }

    // Clock (top right)
    Text {
        anchors.top: parent.top
        anchors.topMargin: 32
        anchors.right: parent.right
        anchors.rightMargin: 32
        text: Qt.formatDateTime(new Date(), "ddd MMM d  hh:mm")
        color: "#FFFFFF"
        font.pixelSize: 18
    }

    // Login function
    function login() {
        sddm.login(userField.text, pwField.text, sessionCombo.currentIndex)
    }

    // Handle login failure
    Connections {
        target: sddm
        function onLoginFailed() {
            errorMsg.text = "Login failed. Try again or leave password empty."
            pwField.text = ""
            pwField.focus = true
        }
    }
}
