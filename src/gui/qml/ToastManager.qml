import QtQuick
import QtQuick.Controls

Item {
    id: root

    property string lastMessage: ""

    width: 320
    height: 240

    ListModel {
        id: toastModel
    }

    function normalizeMessage(message) {
        if (!message) {
            return ""
        }

        let cleaned = message.toString().replace(/\s+/g, " ").trim()
        if (cleaned.length > 90) {
            cleaned = cleaned.slice(0, 87) + "..."
        }
        return cleaned
    }

    function pushToast(message, tone) {
        const cleaned = normalizeMessage(message)
        if (cleaned.length === 0 || cleaned === lastMessage) {
            return
        }

        lastMessage = cleaned
        toastModel.insert(0, {
            "message": cleaned,
            "tone": tone || "info"
        })

        while (toastModel.count > 4) {
            toastModel.remove(toastModel.count - 1, 1)
        }
    }

    Column {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        spacing: 10

        Repeater {
            model: toastModel

            delegate: Rectangle {
                required property int index
                required property string message
                required property string tone

                width: 300
                height: 64
                radius: 22
                color: "#8f09101a"
                border.width: 1
                border.color: tone === "error" ? "#7d4b6d" : tone === "response" ? "#3567aa" : "#274c6a"
                opacity: 0.94

                Rectangle {
                    x: 14
                    y: parent.height / 2 - height / 2
                    width: 8
                    height: 8
                    radius: 4
                    color: tone === "error" ? "#ff8fb5" : tone === "response" ? "#8ae6ff" : "#8da7ff"
                }

                Label {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 34
                    anchors.rightMargin: 18
                    text: message
                    color: "#ecf7ff"
                    font.pixelSize: 14
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                transform: Translate { y: index * 0 }

                SequentialAnimation on opacity {
                    running: true
                    loops: 1
                    PauseAnimation { duration: 4200 }
                    NumberAnimation { to: 0.0; duration: 260; easing.type: Easing.InCubic }
                    onFinished: {
                        if (index < toastModel.count) {
                            toastModel.remove(index, 1)
                        }
                    }
                }

                Behavior on y { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
            }
        }
    }
}
