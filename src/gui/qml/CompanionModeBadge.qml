import QtQuick

Item {
    id: root

    property string text: ""
    property color fillColor: "#16314f"
    property color borderColor: "#3b6d95"
    property color textColor: "#edf6ff"
    property real dpiScale: 1.0

    visible: text.length > 0
    implicitWidth: badgeText.implicitWidth + Math.round(22 * dpiScale)
    implicitHeight: Math.round(28 * dpiScale)

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: root.fillColor
        border.width: 1
        border.color: root.borderColor
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        radius: height / 2
        color: "transparent"
        border.width: 1
        border.color: "#14ffffff"
    }

    Text {
        id: badgeText
        anchors.centerIn: parent
        text: root.text
        color: root.textColor
        opacity: 0.96
        font.pixelSize: Math.round(11 * root.dpiScale)
        font.weight: Font.DemiBold
        renderType: Text.NativeRendering
    }
}
