/* NovatOS Calamares slideshow
   Shows feature highlights while packages are being copied.
   Calamares slideshow API v1. */

import QtQuick 2.0;

Item {
    id: root
    width: 800
    height: 480

    property var slides: [
        { title: "Welcome to NovatOS",     text: "A modern, lightweight Arch-based distribution featuring KDE Plasma 6." },
        { title: "One-Click Installs",      text: "Discover + Flatpak + AUR (paru). Find and install anything in seconds." },
        { title: "All GPU Drivers",         text: "AMD, Intel, and NVIDIA supported out of the box — including Wayland." },
        { title: "Gaming-Ready",            text: "Steam + Proton + Lutris + Wine + Gamescope + Mangohud preinstalled." },
        { title: "Windows Programs",        text: "Run .exe files with a single click via Wine and Bottles (after install)." },
        { title: "BIOS & UEFI",             text: "Boots on legacy BIOS and modern UEFI systems, including Secure Boot fallback." },
        { title: "Live + Install",          text: "Test it from USB first, install to disk when ready — same image." },
        { title: "Built for 2026",          text: "Wayland, Pipewire, Plasma 6, latest kernels, latest drivers." }
    ]

    property int currentIndex: 0

    Rectangle {
        anchors.fill: parent
        color: "#0F1117"
    }

    Rectangle {
        anchors.centerIn: parent
        width: 720
        height: 360
        color: "#161922"
        radius: 12
        border.color: "#2A2F3D"
        border.width: 1

        Column {
            anchors.centerIn: parent
            spacing: 24
            width: 640

            Text {
                id: titleText
                width: parent.width
                text: slides[currentIndex].title
                color: "#4CC2FF"
                font.pixelSize: 36
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }

            Text {
                id: bodyText
                width: parent.width
                text: slides[currentIndex].text
                color: "#E6E9EF"
                font.pixelSize: 18
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
            }
        }
    }

    // Progress dots
    Row {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        spacing: 8

        Repeater {
            model: slides.length
            Rectangle {
                width: 8
                height: 8
                radius: 4
                color: index === currentIndex ? "#4CC2FF" : "#3A3F4D"
            }
        }
    }

    Timer {
        interval: 4000
        running: true
        repeat: true
        onTriggered: {
            currentIndex = (currentIndex + 1) % slides.length
        }
    }

    Behavior on opacity { NumberAnimation { duration: 250 } }
}
