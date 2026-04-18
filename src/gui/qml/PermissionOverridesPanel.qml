import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "." as JarvisUi

JarvisUi.VisionGlassPanel {
    id: root

    property var viewModel
    property string statusText: ""
    readonly property var capabilityOptions: viewModel ? (viewModel.permissionCapabilityOptions || []) : []

    width: parent ? parent.width : 720
    implicitHeight: contentColumn.implicitHeight + 44
    radius: 30
    panelColor: "#16192022"
    innerColor: "#1d212b2f"
    outlineColor: "#20ffffff"
    highlightColor: "#16ffffff"
    shadowOpacity: 0.22

    function capabilityIndex(capabilityId) {
        for (let i = 0; i < capabilityOptions.length; i += 1) {
            if (capabilityOptions[i].capabilityId === capabilityId) {
                return i
            }
        }
        return -1
    }

    function capabilityLabel(capabilityId) {
        const optionIndex = capabilityIndex(capabilityId)
        return optionIndex >= 0 ? capabilityOptions[optionIndex].label : capabilityId
    }

    function capabilityLabels() {
        const labels = []
        for (let i = 0; i < capabilityOptions.length; i += 1) {
            labels.push(capabilityOptions[i].label)
        }
        return labels
    }

    function firstUnusedCapability() {
        for (let i = 0; i < capabilityOptions.length; i += 1) {
            let used = false
            for (let row = 0; row < overrideRows.count; row += 1) {
                if (overrideRows.get(row).capabilityId === capabilityOptions[i].capabilityId) {
                    used = true
                    break
                }
            }
            if (!used) {
                return capabilityOptions[i]
            }
        }
        return null
    }

    function syncFromBackend() {
        overrideRows.clear()
        if (!viewModel) {
            return
        }
        const rows = viewModel.permissionOverrides || []
        for (let i = 0; i < rows.length; i += 1) {
            const row = rows[i]
            const fallbackCapability = capabilityOptions.length > 0 ? capabilityOptions[0].capabilityId : ""
            const decision = ["allow", "deny", "confirm"].indexOf(row.decision) >= 0
                ? row.decision
                : "confirm"
            overrideRows.append({
                capabilityId: row.capabilityId || fallbackCapability,
                decision: decision,
                scope: row.scope || "",
                reasonCode: row.reasonCode || ""
            })
        }
        statusText = rows.length > 0 ? "Loaded " + rows.length + " override rule(s)." : "No override rules saved."
    }

    function rowsForSave() {
        const rows = []
        for (let i = 0; i < overrideRows.count; i += 1) {
            const row = overrideRows.get(i)
            rows.push({
                capabilityId: row.capabilityId,
                decision: row.decision,
                scope: row.scope,
                reasonCode: row.reasonCode
            })
        }
        return rows
    }

    function addRule() {
        const option = firstUnusedCapability()
        if (!option) {
            statusText = "No registry capabilities are available to add."
            return
        }
        overrideRows.append({
            capabilityId: option.capabilityId,
            decision: "confirm",
            scope: "",
            reasonCode: ""
        })
        statusText = "Added " + option.label + " rule. Save to persist."
    }

    function saveRules() {
        if (!viewModel) {
            return
        }
        const ok = viewModel.savePermissionOverrides(rowsForSave())
        statusText = ok ? "Permission override rules saved." : "Permission override save failed."
    }

    ListModel {
        id: overrideRows
    }

    Component.onCompleted: syncFromBackend()

    ColumnLayout {
        id: contentColumn
        anchors.fill: parent
        anchors.margins: 22
        spacing: 14

        RowLayout {
            spacing: 10
            Text {
                text: "Permission Overrides"
                color: "#eef7ff"
                font.pixelSize: 22
                font.weight: Font.Medium
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Add Rule"
                enabled: root.capabilityOptions.length > 0
                onClicked: root.addRule()
            }
            Button { text: "Reset"; onClicked: root.syncFromBackend() }
            Button {
                text: "Clear"
                onClicked: {
                    overrideRows.clear()
                    root.saveRules()
                }
            }
        }

        Text {
            Layout.fillWidth: true
            text: "Choose only from declared capability permissions. Focus Mode, critical alerts, and hard confirmation safety are not tunable here."
            color: "#8099b8"
            font.pixelSize: 13
            wrapMode: Text.Wrap
        }

        Text {
            visible: root.capabilityOptions.length === 0
            Layout.fillWidth: true
            text: "No permission capabilities were exposed by the registry."
            color: "#f4b86a"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        Repeater {
            model: overrideRows

            delegate: JarvisUi.VisionGlassPanel {
                required property int index
                required property string capabilityId
                required property string decision
                required property string scope
                required property string reasonCode

                Layout.fillWidth: true
                radius: 18
                panelColor: "#17202628"
                innerColor: "#1c252d32"
                outlineColor: "#18ffffff"
                highlightColor: "#0cffffff"
                shadowOpacity: 0.12
                implicitHeight: rowLayout.implicitHeight + 24

                RowLayout {
                    id: rowLayout
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    ComboBox {
                        Layout.preferredWidth: 190
                        model: root.capabilityLabels()
                        currentIndex: root.capabilityIndex(capabilityId)
                        onActivated: overrideRows.setProperty(index, "capabilityId",
                                                              root.capabilityOptions[currentIndex].capabilityId)
                    }

                    ComboBox {
                        Layout.preferredWidth: 110
                        model: ["confirm", "allow", "deny"]
                        currentIndex: Math.max(0, ["confirm", "allow", "deny"].indexOf(decision))
                        onActivated: overrideRows.setProperty(index, "decision", currentText)
                    }

                    TextField {
                        Layout.fillWidth: true
                        text: scope
                        placeholderText: "optional scope"
                        onEditingFinished: overrideRows.setProperty(index, "scope", text)
                    }

                    TextField {
                        Layout.fillWidth: true
                        text: reasonCode
                        placeholderText: "optional reason code"
                        onEditingFinished: overrideRows.setProperty(index, "reasonCode", text)
                    }

                    Button {
                        text: "Remove"
                        onClicked: overrideRows.remove(index)
                    }
                }
            }
        }

        Text {
            visible: overrideRows.count === 0
            Layout.fillWidth: true
            text: "No user override rules. Registry defaults and confirmation policy are active."
            color: "#9ab0ca"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Text {
                Layout.fillWidth: true
                text: root.statusText
                color: "#9ab0ca"
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
            Button {
                text: "Save Permission Rules"
                onClicked: root.saveRules()
            }
        }
    }
}
