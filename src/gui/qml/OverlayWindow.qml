import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "." as JarvisUi

Window {
    id: root

    width: 760
    height: 820
    visible: false
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.NoDropShadowWindowHint

    onClosing: function(close) {
        close.accepted = false
        hide()
    }

    function compactText(rawText, fallbackText) {
        let text = (rawText || "").toString().replace(/\s+/g, " ").trim()
        if (text.length === 0) {
            text = fallbackText || ""
        }
        if (text.length > 84) {
            text = text.slice(0, 81) + "..."
        }
        return text
    }

    function greetingLine() {
        const hour = new Date().getHours()
        const period = hour < 12 ? "Good morning" : hour >= 18 ? "Good evening" : "Good afternoon"
        return backend.userName.length > 0 ? period + ", " + backend.userName + "." : period + "."
    }

    function primaryLine() {
        if (backend.responseText.length > 0) {
            return compactText(backend.responseText, "")
        }
        if (backend.transcript.length > 0) {
            return compactText(backend.transcript, "")
        }
        if (backend.stateName === "IDLE") {
            return greetingLine()
        }
        return compactText(backend.statusText, "Ready.")
    }

    function secondaryLine() {
        if (backend.stateName === "LISTENING") {
            return "Listening for your next phrase."
        }
        if (backend.stateName === "PROCESSING") {
            return "Working through it."
        }
        if (backend.stateName === "SPEAKING") {
            return "Delivering the response."
        }
        return compactText(backend.statusText, "Standing by.")
    }

    JarvisUi.AnimationController {
        id: motion
        stateName: backend.stateName
        inputLevel: backend.audioLevel
        overlayVisible: backend.overlayVisible
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Rectangle {
            anchors.centerIn: parent
            width: 640
            height: 760
            radius: 48
            color: "#04070e10"
            border.width: 1
            border.color: "#1d2d4a"
            opacity: 0.98
        }

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 42
            width: 220
            height: 34
            radius: 17
            color: "#88060b14"
            border.width: 1
            border.color: "#203454"
            opacity: 0.92

            Row {
                anchors.centerIn: parent
                spacing: 10

                Rectangle {
                    width: 8
                    height: 8
                    radius: 4
                    color: backend.stateName === "LISTENING" ? "#75f0ff"
                        : backend.stateName === "PROCESSING" ? "#8ba5ff"
                        : backend.stateName === "SPEAKING" ? "#cf8fff"
                        : "#6a84aa"
                }

                Text {
                    text: backend.assistantName + "  ·  " + backend.stateName
                    color: "#dcedff"
                    font.pixelSize: 12
                    font.letterSpacing: 1.6
                }
            }
        }

        Item {
            anchors.centerIn: parent
            width: 560
            height: 620

            JarvisUi.OrbRenderer {
                id: orb
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 36
                width: 360
                height: 360
                stateName: backend.stateName
                time: motion.time
                audioLevel: motion.inputBoost
                speakingLevel: motion.speakingSignal
                distortion: motion.distortion
                glow: motion.glow
                orbScale: motion.orbScale
                orbitalRotation: motion.orbitalRotation
            }

            MouseArea {
                anchors.fill: orb
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if (backend.stateName === "LISTENING" || backend.stateName === "PROCESSING") {
                        backend.cancelRequest()
                    } else {
                        backend.startListening()
                    }
                }
            }

            Column {
                anchors.top: orb.bottom
                anchors.topMargin: 26
                anchors.horizontalCenter: parent.horizontalCenter
                width: 500
                spacing: 8

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: primaryLine()
                    color: "#eef8ff"
                    font.pixelSize: 30
                    font.weight: Font.Medium
                    wrapMode: Text.Wrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: secondaryLine()
                    color: "#7e95b9"
                    font.pixelSize: 15
                    wrapMode: Text.Wrap
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            Rectangle {
                id: inputRibbon
                anchors.bottom: parent.bottom
                anchors.horizontalCenter: parent.horizontalCenter
                width: 520
                height: 74
                radius: 37
                color: "#99071018"
                border.width: 1
                border.color: backend.stateName === "LISTENING" ? "#3c94d9" : "#213753"

                Behavior on border.color { ColorAnimation { duration: 220 } }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 54
                        Layout.preferredHeight: 54
                        radius: 27
                        color: backend.stateName === "LISTENING" ? "#1f70b8" : "#13253b"
                        border.width: 1
                        border.color: backend.stateName === "LISTENING" ? "#90e7ff" : "#28445f"

                        Text {
                            anchors.centerIn: parent
                            text: backend.stateName === "LISTENING" ? "■" : "●"
                            color: "#effbff"
                            font.pixelSize: 16
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                if (backend.stateName === "LISTENING" || backend.stateName === "PROCESSING") {
                                    backend.cancelRequest()
                                } else {
                                    backend.startListening()
                                }
                            }
                        }
                    }

                    TextField {
                        id: promptField
                        Layout.fillWidth: true
                        color: "#eff7ff"
                        font.pixelSize: 16
                        placeholderText: "Ask naturally"
                        placeholderTextColor: "#6884a3"
                        selectByMouse: true
                        background: Item {}
                        onAccepted: {
                            const prompt = text.trim()
                            if (prompt.length === 0) {
                                return
                            }
                            backend.submitText(prompt)
                            text = ""
                        }
                    }

                    Rectangle {
                        Layout.preferredWidth: 54
                        Layout.preferredHeight: 54
                        radius: 27
                        color: "#182b45"
                        border.width: 1
                        border.color: "#32557d"

                        Text {
                            anchors.centerIn: parent
                            text: "↗"
                            color: "#effbff"
                            font.pixelSize: 18
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                const prompt = promptField.text.trim()
                                if (prompt.length === 0) {
                                    return
                                }
                                backend.submitText(prompt)
                                promptField.text = ""
                            }
                        }
                    }
                }
            }
        }

        JarvisUi.ToastManager {
            id: toastManager
            anchors.right: parent.right
            anchors.rightMargin: 36
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 34
        }
    }

    Connections {
        target: backend

        function onStatusTextChanged() {
            if (backend.statusText.length === 0) {
                return
            }
            toastManager.pushToast(backend.statusText, backend.statusText.toLowerCase().indexOf("error") >= 0 ? "error" : "status")
        }

        function onResponseTextChanged() {
            if (backend.responseText.length === 0) {
                return
            }
            toastManager.pushToast(backend.responseText, "response")
        }
    }
}
