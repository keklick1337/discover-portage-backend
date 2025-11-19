/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kitemmodels as KItemModels

Kirigami.Dialog {
    id: root
    
    title: ""
    standardButtons: Kirigami.Dialog.NoButton
    
    // Properties for compatibility with SourcesPage
    property var source: null  // AbstractSourcesBackend
    property string displayName: ""
    
    width: Kirigami.Units.gridUnit * 40
    height: Kirigami.Units.gridUnit * 20
    
    contentItem: ColumnLayout {
        anchors.fill: parent
        spacing: 0
        
        QQC2.TabBar {
            id: tabBar
            Layout.fillWidth: true
            
            QQC2.TabButton {
                text: "Official Repositories"
            }
            QQC2.TabButton {
                text: "Manual"
            }
        }
        
        Kirigami.Separator {
            Layout.fillWidth: true
        }
        
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex
            
            // Official repositories tab
            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                
                Kirigami.SearchField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: "Search repositories..."
                    onTextChanged: searchTimer.restart()
                }
                
                Timer {
                    id: searchTimer
                    interval: 300
                    onTriggered: filterModel.update()
                }
                
                QQC2.ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: Kirigami.Units.gridUnit * 10
                    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff
                    clip: true
                        
                        ListView {
                            id: officialListView
                            model: filterModel
                            spacing: Kirigami.Units.smallSpacing
                            
                            delegate: QQC2.ItemDelegate {
                                width: officialListView.width
                                padding: Kirigami.Units.smallSpacing
                            
                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.largeSpacing
                                    
                                    QQC2.CheckBox {
                                        id: checkbox
                                        checked: model.selected || false
                                        onToggled: {
                                            filterModel.setProperty(index, "selected", checked)
                                        }
                                    }
                                    
                                    QQC2.Label {
                                        Layout.fillWidth: true
                                        text: {
                                            var parts = []
                                            parts.push("<b>" + (model.name || "") + "</b>")
                                            
                                            if (model.status && model.status !== "") {
                                                var statusColor = model.status === "official" ? "#27ae60" : "#95a5a6"
                                                parts.push("<font color='" + statusColor + "'>" + model.status + "</font>")
                                            }
                                            
                                            if (model.quality && model.quality !== "") {
                                                parts.push("<i>" + model.quality + "</i>")
                                            }
                                            
                                            if (model.sourceUrl && model.sourceUrl !== "") {
                                                parts.push("<font color='#7f8c8d'>" + model.sourceUrl + "</font>")
                                            }
                                            
                                            return parts.join(" | ")
                                        }
                                        textFormat: Text.RichText
                                        elide: Text.ElideRight
                                        font.pointSize: Kirigami.Theme.defaultFont.pointSize
                                    }
                                }
                            }
                    }
                }
                
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.largeSpacing
                    Layout.margins: Kirigami.Units.largeSpacing
                    
                    QQC2.Label {
                        id: selectedCountLabel
                        text: getSelectedCount() === 1 ? "1 repository selected" : getSelectedCount() + " repositories selected"
                        opacity: 0.7
                    }
                    
                    Item { Layout.fillWidth: true }
                    
                    QQC2.Button {
                        text: "Cancel"
                        icon.name: "dialog-cancel"
                        onClicked: root.close()
                    }
                    
                    QQC2.Button {
                        text: "Add Selected"
                        icon.name: "list-add"
                        enabled: getSelectedCount() > 0
                        onClicked: {
                            addSelectedRepositories()
                            root.close()
                        }
                    }
                }
            }
            
            // Manual tab
            ColumnLayout {
                spacing: Kirigami.Units.largeSpacing
                
                Kirigami.FormLayout {
                    Layout.fillWidth: true
                    
                    QQC2.TextField {
                        id: repoNameField
                        Kirigami.FormData.label: "Repository name:"
                        placeholderText: "e.g., my-overlay"
                    }
                    
                    QQC2.ComboBox {
                        id: syncTypeCombo
                        Kirigami.FormData.label: "Sync type:"
                        model: ["git", "rsync", "svn", "mercurial"]
                        currentIndex: 0
                    }
                    
                    QQC2.TextField {
                        id: syncUriField
                        Kirigami.FormData.label: "Sync URI:"
                        placeholderText: "e.g., https://github.com/user/overlay.git"
                        Layout.fillWidth: true
                    }
                }
                
                Item { Layout.fillHeight: true }
                
                RowLayout {
                    Layout.fillWidth: true
                    
                    Item { Layout.fillWidth: true }
                    
                    QQC2.Button {
                        text: "Cancel"
                        icon.name: "dialog-cancel"
                        onClicked: root.close()
                    }
                    
                    QQC2.Button {
                        text: "Add Repository"
                        icon.name: "list-add"
                        enabled: repoNameField.text.length > 0 && syncUriField.text.length > 0
                        onClicked: {
                            addManualRepository()
                            root.close()
                        }
                    }
                }
            }
        }
    }  // contentItem
    
    ListModel {
        id: filterModel
        
        function update() {
            clear()
            const search = searchField.text.toLowerCase()
            
            if (!officialRepositories) {
                return
            }
            
            for (let i = 0; i < officialRepositories.length; i++) {
                const repo = officialRepositories[i]
                if (!search || 
                    repo.name.toLowerCase().includes(search) || 
                    repo.description.toLowerCase().includes(search)) {
                    append({
                        name: repo.name,
                        description: repo.description,
                        ownerName: repo.ownerName || "",
                        ownerEmail: repo.ownerEmail || "",
                        quality: repo.quality || "",
                        status: repo.status || "",
                        homepage: repo.homepage || "",
                        sourceUrl: repo.sourceUrl || "",
                        selected: false
                    })
                }
            }
        }
    }
    
    function getSelectedCount() {
        let count = 0
        for (let i = 0; i < filterModel.count; i++) {
            if (filterModel.get(i).selected) {
                count++
            }
        }
        return count
    }
    
    function addSelectedRepositories() {
        for (let i = 0; i < filterModel.count; i++) {
            const item = filterModel.get(i)
            if (item.selected && sourcesBackend) {
                console.log("Adding repository:", item.name)
                sourcesBackend.addSource(item.name)
            }
        }
    }
    
    function addManualRepository() {
        if (sourcesBackend) {
            const syncType = syncTypeCombo.currentText

            console.log("Adding manual repository:", repoNameField.text)
            // For manual repos, we need to call repositoryAdd with parameters
            sourcesBackend.addManualSource(
                repoNameField.text,
                syncType,
                syncUriField.text
            )
        }
    }
    
    onOpened: {
        filterModel.update()
    }
}
