// Qt for MCUs dashboard (800x480). Left-aligned spec-sheet layout, single
// accent colour for warnings; night palette switches from GPS solar elevation.
import QtQuick 2.15

Rectangle {
    id: root
    width: 800; height: 480
    color: VehicleBridge.nightMode ? "#0F1418" : "#E8EDF0"

    readonly property color ink:    VehicleBridge.nightMode ? "#E8EDF0" : "#0F1418"
    readonly property color muted:  "#8B98A3"
    readonly property color accent: "#FF7A1A"

    Column {
        x: 24; y: 24; spacing: 18

        Text { text: "ENGINE"; color: root.muted; font.pixelSize: 16 }
        Text {
            text: VehicleBridge.rpmValid ? VehicleBridge.rpm + " rpm" : "-- rpm"
            color: root.ink; font.pixelSize: 56
        }

        Text { text: "FUEL / RUNTIME LEFT"; color: root.muted; font.pixelSize: 16 }
        Row {
            spacing: 24
            Text {
                text: VehicleBridge.fuelValid ? VehicleBridge.fuelPct + " %" : "-- %"
                color: VehicleBridge.lowFuel ? root.accent : root.ink
                font.pixelSize: 48
            }
            Text {
                text: VehicleBridge.runtimeValid
                      ? Math.floor(VehicleBridge.runtimeLeftMin / 60) + " h "
                        + (VehicleBridge.runtimeLeftMin % 60) + " min"
                        + (VehicleBridge.runtimeEstimated ? " est." : "")
                      : "calculating"
                color: root.ink; font.pixelSize: 48
            }
        }

        Text { text: "JOB"; color: root.muted; font.pixelSize: 16 }
        Text {
            text: (VehicleBridge.jobAreaM2 / 10000).toFixed(2) + " ha   overlap "
                  + VehicleBridge.overlapPct + " %"
            color: root.ink; font.pixelSize: 32
        }

        Text {
            visible: VehicleBridge.dtcCount > 0 || VehicleBridge.anomalyAlert || VehicleBridge.outsideFence
            text: VehicleBridge.outsideFence ? "OUTSIDE WORK AREA"
                  : VehicleBridge.anomalyAlert ? "ENGINE BEHAVIOUR ABNORMAL — CHECK"
                  : VehicleBridge.dtcCount + " ACTIVE FAULT(S)"
            color: root.accent; font.pixelSize: 28
        }
    }

    Text {
        anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: 16
        text: (VehicleBridge.engineHoursX10 / 10).toFixed(1) + " h   service in "
              + (VehicleBridge.serviceInHoursX10 / 10).toFixed(0) + " h"
        color: root.muted; font.pixelSize: 18
    }
}
