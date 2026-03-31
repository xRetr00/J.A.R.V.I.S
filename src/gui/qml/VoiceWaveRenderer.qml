import QtQuick

Item {
    id: root

    implicitWidth: 320
    implicitHeight: 72

    property real time: 0.0
    property real audioLevel: 0.0
    property real speakingLevel: 0.0
    property real glow: 0.0
    property int uiState: -1
    property string diagnosticsTag: ""
    property bool geometryReadyLogged: false

    readonly property real reactiveAudio: clamp(Math.max(audioLevel, speakingLevel * 0.9), 0.0, 1.0)
    readonly property real reactiveGlow: clamp(glow, 0.0, 1.0)
    readonly property real amplitudeUniform: clamp(0.28 + reactiveAudio * 0.92 + reactiveGlow * 0.12, 0.0, 1.25)
    readonly property real timeScaleUniform: 1.0
        + reactiveAudio * 0.55
        + Math.max(0, uiState - 1) * 0.08
    readonly property real yScaleUniform: clamp(1.0 - reactiveAudio * 0.16, 0.82, 1.0)
    readonly property real exposureUniform: 0.9
        + reactiveGlow * 0.22
        + speakingLevel * 0.18
    readonly property string resolvedDiagnosticsTag: diagnosticsTag.length > 0
        ? diagnosticsTag
        : "voice_wave_" + Math.round(width) + "x" + Math.round(height)

    function clamp(value, minValue, maxValue) {
        return Math.max(minValue, Math.min(maxValue, value))
    }

    function diagnosticsPayload(extra) {
        const payload = {
            tag: resolvedDiagnosticsTag,
            time: Number(time.toFixed(3)),
            audioLevel: Number(reactiveAudio.toFixed(3)),
            speakingLevel: Number(speakingLevel.toFixed(3)),
            glow: Number(reactiveGlow.toFixed(3)),
            timeScale: Number(timeScaleUniform.toFixed(3)),
            amplitude: Number(amplitudeUniform.toFixed(3)),
            yScale: Number(yScaleUniform.toFixed(3)),
            exposure: Number(exposureUniform.toFixed(3)),
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
        backend.logOrbRendererStatus("voice_wave_component_completed", diagnosticsPayload({
            shader: "qrc:/qt/qml/VAXIL/gui/shaders/src/gui/shaders/voice_wave.frag.qsb"
        }))
        logGeometryReady()
    }

    Timer {
        interval: 12000
        running: root.visible && root.width > 0 && root.height > 0
        repeat: true
        onTriggered: backend.logOrbRendererStatus("voice_wave_heartbeat", root.diagnosticsPayload({}))
    }

    onWidthChanged: logGeometryReady()
    onHeightChanged: logGeometryReady()

    function logGeometryReady() {
        if (geometryReadyLogged || width <= 0 || height <= 0) {
            return
        }
        geometryReadyLogged = true
        backend.logOrbRendererStatus("voice_wave_geometry_ready", diagnosticsPayload({}))
    }

    ShaderEffect {
        anchors.fill: parent
        blending: true
        fragmentShader: "qrc:/qt/qml/VAXIL/gui/shaders/src/gui/shaders/voice_wave.frag.qsb"
        property real time: root.time
        property vector2d resolution: Qt.vector2d(width, height)
        property real timeScale: root.timeScaleUniform
        property real amplitude: root.amplitudeUniform
        property real yScale: root.yScaleUniform
        property real exposure: root.exposureUniform
    }
}
