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

    implicitWidth: 520
    implicitHeight: 520

    ShaderEffect {
        id: auraLayer
        anchors.centerIn: parent
        width: parent.width * 1.18
        height: width
        blending: true

        property real time: root.time * 0.68 + 8.0
        property real level: root.audioLevel * 0.75
        property real speaking: root.speakingLevel * 0.7
        property real mode: root.stateIndex
        property real distortion: root.distortion * 1.45
        property vector2d resolution: Qt.vector2d(width, height)
        property color colorA: root.stateName === "SPEAKING" ? "#f0c8ff" : "#b8fbff"
        property color colorB: root.stateName === "PROCESSING" ? "#7b74ff" : "#53b4ff"
        property color colorC: root.stateName === "SPEAKING" ? "#4c1296" : "#07172c"

        fragmentShader: "qrc:/qt/qml/JARVIS/gui/shaders/src/gui/shaders/orb.frag.qsb"
        opacity: 0.24 + root.glow * 0.22
        scale: 1.02 + root.glow * 0.2
        rotation: root.orbitalRotation * 0.18
    }

    ShaderEffect {
        id: plasmaLayer
        anchors.centerIn: parent
        width: parent.width * 0.86
        height: width
        blending: true

        property real time: root.time * 0.97
        property real level: root.audioLevel * 0.92
        property real speaking: root.speakingLevel * 0.9
        property real mode: root.stateIndex
        property real distortion: root.distortion * 1.08
        property vector2d resolution: Qt.vector2d(width, height)
        property color colorA: root.stateName === "SPEAKING" ? "#f7e4ff" : "#ddffff"
        property color colorB: root.stateName === "PROCESSING" ? "#6a6cff" : "#2da3ff"
        property color colorC: root.stateName === "SPEAKING" ? "#7927e0" : "#0d2551"

        fragmentShader: "qrc:/qt/qml/JARVIS/gui/shaders/src/gui/shaders/orb.frag.qsb"
        opacity: 0.92
        scale: root.orbScale
        rotation: root.orbitalRotation * -0.32
    }

    ShaderEffect {
        id: coreLayer
        anchors.centerIn: parent
        width: parent.width * 0.56
        height: width
        blending: true

        property real time: root.time * 1.34 + 17.0
        property real level: root.audioLevel * 0.62
        property real speaking: root.speakingLevel * 0.88
        property real mode: root.stateIndex
        property real distortion: root.distortion * 0.74
        property vector2d resolution: Qt.vector2d(width, height)
        property color colorA: "#f7ffff"
        property color colorB: root.stateName === "SPEAKING" ? "#f39cff" : "#90e3ff"
        property color colorC: root.stateName === "SPEAKING" ? "#6a21bd" : "#163864"

        fragmentShader: "qrc:/qt/qml/JARVIS/gui/shaders/src/gui/shaders/orb.frag.qsb"
        opacity: 0.98
        scale: 0.92 + root.orbScale * 0.08
        rotation: root.orbitalRotation * 0.46
    }
}
