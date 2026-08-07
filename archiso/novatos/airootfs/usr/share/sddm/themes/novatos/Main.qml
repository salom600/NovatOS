/*
 * NovatOS SDDM theme — modern, minimal, dark with cyan accents
 */
import QtQuick 2.15
import QtQuick.Layouts 1.15
import SddmComponents 2.0

Item {
    id: root
    width: 1920
    height: 1080

    // Background image
    Image {
        anchors.fill: parent
        source: "background.png"
        fillMode: Image.PreserveAspectCrop
    }

    // Dim overlay for readability
    Rectangle {
        anchors.fill: parent
        color: "#0F1117"
        opacity: 0.45
    }

    // Center login card
    Rectangle {
        id: card
        width: 420
        height: 460
        anchors.centerIn: parent
        color: "#161922"
        radius: 16
        border.color: "#2A2F3D"
        border.width: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 32
            spacing: 18

            // Logo
            Image {
                source: "background.png"
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 96
                Layout.preferredHeight: 96
                fillMode: Image.PreserveAspectCrop
                visible: false
            }

            // Title
            Text {
                text: "NovatOS"
                color: "#FFFFFF"
                font.pixelSize: 42
                font.weight: Font.Bold
                Layout.alignment: Qt.AlignHCenter
            }
            Text {
                text: "Aurora Edition - 2026"
                color: "#9DB7E0"
                font.pixelSize: 16
                Layout.alignment: Qt.AlignHCenter
            }

            // Username
            TextField {
                id: userField
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                placeholderText: "Username"
                text: userModel.lastUser
                color: "#FFFFFF"
                font.pixelSize: 16
                background: Rectangle {
                    color: "#0F1117"
                    radius: 8
                    border.color: "#2A2F3D"
                    border.width: 1
                }
                onAccepted: pwField.focus = true
            }

            // Password
            TextField {
                id: pwField
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                placeholderText: "Password"
                echoMode: TextInput.Password
                color: "#FFFFFF"
                font.pixelSize: 16
                background: Rectangle {
                    color: "#0F1117"
                    radius: 8
                    border.color: "#2A2F3D"
                    border.width: 1
                }
                onAccepted: login()
            }

            // Login button
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 48
                color: loginMouseArea.containsMouse ? "#5DD2FF" : "#4CC2FF"
                radius: 8

                Text {
                    anchors.centerIn: parent
                    text: "Sign in"
                    color: "#0F1117"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }

                MouseArea {
                    id: loginMouseArea
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: login()
                }
            }

            // Session selector
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Text {
                    text: "Session:"
                    color: "#9DB7E0"
                    font.pixelSize: 13
                }

                ComboBox {
                    id: sessionCombo
                    Layout.fillWidth: true
                    model: sessionModel
                    currentIndex: sessionModel.lastIndex
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
        }
    }

    // Bottom toolbar (power, reboot)
    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 32

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
                radius: 8
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
                    onClicked: sddm.powerOff()
                }
            }
        }
    }

    // Clock top-right
    Text {
        anchors.top: parent.top
        anchors.topMargin: 32
        anchors.right: parent.right
        anchors.rightMargin: 32
        text: Qt.formatDateTime(new Date(), "ddd MMM d  hh:mm")
        color: "#FFFFFF"
        font.pixelSize: 18
    }

    function login() {
        sddm.login(userField.text, pwField.text, sessionCombo.currentIndex)
    }

    Connections {
        target: sddm
        function onLoginFailed() {
            errorMsg.text = "Login failed. Check your credentials."
            pwField.text = ""
            pwField.focus = true
        }
    }
}
