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
    
    readonly property var useFlags: resource.useFlagsInformation()
    
    Discover.Activatable.active: useFlags && useFlags.length > 0
    Layout.fillWidth: true

    spacing: Kirigami.Units.smallSpacing

    Component.onCompleted: {
        console.log("USE Flags component loaded for", resource.name)
        console.log("  Found", useFlags.length, "USE flags")
        console.log("  Root width:", width)
    }

    Kirigami.Heading {
        Layout.fillWidth: true
        text: i18nd("libdiscover", "USE Flags")
        level: 2
        type: Kirigami.Heading.Type.Primary
        wrapMode: Text.Wrap
    }

    Grid {
        Layout.fillWidth: true
        width: root.width
        columns: Math.max(1, Math.floor(root.width / 150))
        columnSpacing: Kirigami.Units.smallSpacing
        rowSpacing: Kirigami.Units.smallSpacing

        Repeater {
            id: flagsRepeater
            model: root.useFlags

            delegate: QQC2.CheckBox {
                required property var modelData
                required property int index

                text: modelData.name
                checked: modelData.installed

                QQC2.ToolTip {
                    visible: parent.hovered
                    text: modelData.description
                    delay: Kirigami.Units.toolTipDelay
                }

                onToggled: {
                    console.log("USE flag", modelData.name, "changed to", checked)
                    // Collect all enabled flags and save
                    var enabledFlags = []
                    var count = flagsRepeater.count
                    for (var i = 0; i < count; i++) {
                        var item = flagsRepeater.itemAt(i)
                        if (item && item.checked) {
                            var flag = root.useFlags[i]
                            if (flag && flag.name) {
                                enabledFlags.push(flag.name)
                            }
                        }
                    }
                    
                    console.log("Saving enabled USE flags:", enabledFlags)
                    var success = root.resource.saveUseFlags(enabledFlags)
                    if (success) {
                        console.log("USE flags saved successfully")
                    } else {
                        console.error("Failed to save USE flags")
                    }
                }
            }
        }
    }
}
