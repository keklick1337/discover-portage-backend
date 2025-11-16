/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.discover as Discover
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root

    required property Discover.AbstractResource resource
    
    readonly property var useFlags: resource.useFlagsInformation
    readonly property bool isInstalled: resource.state === 1 || resource.state === 2
    
    Discover.Activatable.active: isInstalled && useFlags && useFlags.length > 0
    Layout.fillWidth: true

    spacing: Kirigami.Units.smallSpacing

    Component.onCompleted: {
        console.log("USE Flags component loaded for", resource.name)
        console.log("  Found", useFlags.length, "USE flags")
        console.log("  Root width:", width)
    }

    QQC2.Frame {
        Layout.fillWidth: true
        background: Rectangle {
            color: Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.1)
            border.color: Kirigami.Theme.highlightColor
            border.width: 1
            radius: 4
        }
        
        RowLayout {
            width: parent.width
            spacing: Kirigami.Units.largeSpacing
            
            Kirigami.Icon {
                Layout.alignment: Qt.AlignTop
                source: "documentinfo"
                implicitWidth: Kirigami.Units.iconSizes.smallMedium
                implicitHeight: Kirigami.Units.iconSizes.smallMedium
                color: Kirigami.Theme.highlightColor
            }
            
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                
                QQC2.Label {
                    Layout.fillWidth: true
                    text: i18nd("libdiscover", "USE Flags")
                    font.bold: true
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
                    color: Kirigami.Theme.textColor
                    wrapMode: Text.Wrap
                }
                
                Flow {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing
                    
                    Repeater {
                        id: flagsRepeater
                        model: root.useFlags

                        delegate: QQC2.Label {
                            required property var modelData
                            required property int index

                            readonly property bool isInstalled: modelData !== null && modelData !== undefined && modelData.installed === true

                            text: isInstalled ? modelData.name : ("-" + (modelData ? modelData.name : ""))
                            color: isInstalled ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                            font.bold: isInstalled
                            font.weight: isInstalled ? Font.Bold : Font.Light
                            
                            QQC2.ToolTip {
                                visible: parent.hovered && modelData !== null && modelData !== undefined
                                text: modelData ? modelData.description : ""
                                delay: Kirigami.Units.toolTipDelay
                            }
                            
                            HoverHandler {
                                id: hoverHandler
                                cursorShape: Qt.ArrowCursor
                            }
                        }
                    }
                }
            }
        }
    }
}
