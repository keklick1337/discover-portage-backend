/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.discover as Discover

// Wrapper Item to hold both Action and TransactionListener
Item {
    id: root
    
    property var application: null
    
    // TransactionListener tracks active transactions for this application
    Discover.TransactionListener {
        id: transactionListener
        resource: root.application
    }
    
    // The actual Action that gets injected
    property Kirigami.Action action: Kirigami.Action {
        id: reinstallAction
        
        // Only visible when installed, has requestReinstall method, and NOT during active transaction
        visible: root.application && 
                 root.application.isInstalled && 
                 typeof root.application.requestReinstall === "function" &&
                 !transactionListener.isActive
        
        text: root.application && root.application.state === 3 ? i18n("Upgrade") : i18n("Reinstall")
        icon.name: "view-refresh"
        
        // Disable during transaction (redundant with visible, but safer)
        enabled: !transactionListener.isActive
        
        onTriggered: {
            if (!root.application) {
                console.warn("ReinstallAction: No application set")
                return
            }
            
            if (transactionListener.isActive) {
                console.warn("ReinstallAction: Transaction already active, ignoring click")
                return
            }
            
            console.log("ReinstallAction: Triggered for", root.application.name)
            
            // Step 1: Show version dialog and set requested version
            // requestReinstall() shows dialog but does NOT trigger installation
            if (typeof root.application.requestReinstall === "function") {
                root.application.requestReinstall()
            } else {
                console.error("ReinstallAction: application.requestReinstall is not a function")
                return
            }
            
            // Step 2: Trigger installation via ResourcesModel
            // This creates proper transaction and notifies TransactionListener
            console.log("ReinstallAction: Calling ResourcesModel.installApplication()")
            Discover.ResourcesModel.installApplication(root.application)
        }
    }
}
