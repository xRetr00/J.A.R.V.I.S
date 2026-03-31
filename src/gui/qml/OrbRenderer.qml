import QtQuick
Item {
    id: root

    implicitWidth: 320
    implicitHeight: 320

    readonly property int qualityLow: 0
    readonly property int qualityMedium: 1
    readonly property int qualityHigh: 2

    property string stateName: "IDLE"
    property int uiState: -1
    property real time: 0.0
    property real audioLevel: 0.0
    property real speakingLevel: 0.0
    readonly property real orbState: uiState >= 0 ? uiState : mapState(stateName)
    property int quality: qualityMedium

    property real distortion: 0.0
    property real glow: 0.0
    property real orbScale: 1.0
    property real orbitalRotation: 0.0
    property real auraPulse: 0.0
    property real flicker: 0.0
    property string diagnosticsTag: ""

    readonly property real reactiveAudio: clamp(Math.max(audioLevel, speakingLevel * 0.92), 0.0, 1.0)
    readonly property real reactiveGlow: clamp(glow, 0.0, 1.0)
    readonly property real reactiveDistortion: clamp(distortion, 0.0, 1.0)
    readonly property real reactivePulse: clamp(auraPulse, 0.0, 1.0)
    readonly property real reactiveSpeaking: clamp(speakingLevel, 0.0, 1.0)
    readonly property real reactiveFlicker: clamp(flicker, 0.0, 1.0)
    readonly property real effectScale: Math.max(0.72, orbScale * (0.98 + reactivePulse * 0.05 + reactiveGlow * 0.02))
    readonly property real effectOpacity: clamp(0.86 + reactiveGlow * 0.16, 0.0, 1.0)
    readonly property real timeScaleUniform: 1.0
        + reactivePulse * 0.18
        + reactiveAudio * 0.08
        + reactiveSpeaking * 0.12
    readonly property real zoomUniform: clamp(1.0 - reactivePulse * 0.07 - reactiveAudio * 0.04, 0.86, 1.08)
    readonly property real exposureUniform: 1.0
        + reactiveGlow * 0.24
        + reactiveSpeaking * 0.14
        + reactiveFlicker * 0.08
    readonly property string resolvedDiagnosticsTag: diagnosticsTag.length > 0
        ? diagnosticsTag
        : "orb_" + Math.round(width) + "x" + Math.round(height)

    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value))
    }

    function mapState(name) {
        const normalized = (name || "").toString().trim().toUpperCase()
        if (normalized === "LISTENING") {
            return 1
        }
        if (normalized === "PROCESSING" || normalized === "THINKING") {
            return 2
        }
        if (normalized === "SPEAKING" || normalized === "EXECUTING") {
            return 3
        }
        return 0
    }

    function orbDiagnosticsPayload(extra) {
        const payload = {
            tag: resolvedDiagnosticsTag,
            stateName: stateName,
            uiState: uiState,
            time: Number(time.toFixed(3)),
            timeScale: Number(timeScaleUniform.toFixed(3)),
            zoom: Number(zoomUniform.toFixed(3)),
            exposure: Number(exposureUniform.toFixed(3)),
            effectScale: Number(effectScale.toFixed(3)),
            effectOpacity: Number(effectOpacity.toFixed(3)),
            width: Math.round(width),
            height: Math.round(height),
            visible: root.visible
        }
        for (const key in extra) {
            payload[key] = extra[key]
        }
        return payload
    }

    Component.onCompleted: {
        backend.logOrbRendererStatus("component_completed", orbDiagnosticsPayload({
            shader: "qrc:/qt/qml/VAXIL/gui/shaders/src/gui/shaders/orb.frag.qsb"
        }))
    }

    Timer {
        interval: 12000
        running: root.visible && root.width > 0 && root.height > 0
        repeat: true
        onTriggered: backend.logOrbRendererStatus("heartbeat", root.orbDiagnosticsPayload({
            orbState: Number(root.orbState.toFixed(3)),
            audioLevel: Number(root.reactiveAudio.toFixed(3)),
            speakingLevel: Number(root.reactiveSpeaking.toFixed(3)),
            glow: Number(root.reactiveGlow.toFixed(3))
        }))
    }

    ShaderEffect {
        anchors.fill: parent
        blending: true
        fragmentShader: "qrc:/qt/qml/VAXIL/gui/shaders/src/gui/shaders/orb.frag.qsb"
        property real time: root.time
        property vector2d resolution: Qt.vector2d(width, height)
        property real timeScale: root.timeScaleUniform
        property real zoom: root.zoomUniform
        property real exposure: root.exposureUniform
        scale: root.effectScale
        opacity: root.effectOpacity
        rotation: root.orbitalRotation
        transformOrigin: Item.Center
    }
}
