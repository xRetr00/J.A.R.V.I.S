import QtQuick

Item {
    id: root

    property string stateName: "IDLE"
    property real time: 0.0
    property real audioLevel: 0.0
    property real speakingLevel: 0.0
    property real distortion: 0.12
    property real glow: 0.3
    property real orbScale: 1.0
    property real orbitalRotation: 0.0

    readonly property real stateIndex: stateName === "LISTENING" ? 1.0
        : stateName === "PROCESSING" ? 2.0
        : stateName === "SPEAKING" ? 3.0
        : 0.0

    implicitWidth: 360
    implicitHeight: 360

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * (0.8 + root.glow * 0.18)
        height: width
        radius: width / 2
        color: "#101625"
        opacity: 0.16 + root.glow * 0.12
        scale: root.orbScale * 1.18

        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * (0.7 + root.glow * 0.14)
        height: width
        radius: width / 2
        color: root.stateName === "SPEAKING" ? "#5036d8ff" : "#38269dff"
        opacity: 0.12 + root.glow * 0.12
        scale: root.orbScale * 1.1

        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * (0.56 + root.glow * 0.14)
        height: width
        radius: width / 2
        color: root.stateName === "LISTENING" ? "#2f8ee6ff" : "#2b3fa8ff"
        opacity: 0.2 + root.glow * 0.14
        scale: root.orbScale

        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    }

    ShaderEffect {
        id: orbShader
        anchors.centerIn: parent
        width: parent.width * 0.68
        height: width
        blending: true
        mesh: Qt.size(1, 1)

        property real time: root.time
        property real level: root.audioLevel
        property real speaking: root.speakingLevel
        property real mode: root.stateIndex
        property real distortion: root.distortion
        property vector2d resolution: Qt.vector2d(width, height)
        property color colorA: root.stateName === "SPEAKING" ? "#a6f2ff" : "#8ff4ff"
        property color colorB: root.stateName === "PROCESSING" ? "#5b5cff" : "#35bcff"
        property color colorC: root.stateName === "SPEAKING" ? "#a03dff" : "#0e2349"

        scale: root.orbScale
        fragmentShader: "qrc:/qt/qml/JARVIS/gui/shaders/src/gui/shaders/orb.frag.qsb"

        Behavior on scale { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
    }

    Rectangle {
        anchors.centerIn: orbShader
        width: orbShader.width * 0.28
        height: width
        radius: width / 2
        color: "#ecfbff"
        opacity: 0.12 + root.glow * 0.12 + root.speakingLevel * 0.08
        scale: 0.92 + root.orbScale * 0.08

        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutQuad } }
    }

    Repeater {
        model: 3

        delegate: Item {
            width: root.width
            height: root.height
            opacity: root.stateName === "PROCESSING" ? 0.78 : 0.0
            rotation: root.orbitalRotation + index * 120

            Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            Rectangle {
                width: 12
                height: 12
                radius: 6
                color: index === 0 ? "#89f4ff" : index === 1 ? "#7c91ff" : "#c06cff"
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: -root.height * 0.18
                opacity: 0.78
            }
        }
    }

    Rectangle {
        anchors.centerIn: parent
        width: parent.width * 0.76
        height: width
        radius: width / 2
        color: "transparent"
        border.width: 1
        border.color: root.stateName === "SPEAKING" ? "#68c4ff" : "#3c6fff"
        opacity: 0.18 + root.glow * 0.16
        scale: 1.0 + root.glow * 0.08

        Behavior on opacity { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }
        Behavior on scale { NumberAnimation { duration: 240; easing.type: Easing.OutCubic } }
    }
}
