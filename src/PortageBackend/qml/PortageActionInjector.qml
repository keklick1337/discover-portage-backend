/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

import QtQuick
import org.kde.discover as Discover
import org.kde.discover.portage 1.0

// This component auto-injects into ApplicationPage when it loads
Item {
    id: root
    visible: false
    
    // Required by topObjects() API
    required property Discover.AbstractResource resource
    
    Component.onCompleted: {
        console.log("PortageActionInjector: Component created for", resource.name)
        
        // Wait a bit for ApplicationPage to be fully created
        injectionTimer.start()
    }
    
    Timer {
        id: injectionTimer
        interval: 100
        repeat: true
        running: false
        triggeredOnStart: true
        
        property int attempts: 0
        
        onTriggered: {
            attempts++
            
            // Try to find ApplicationPage by walking up the parent chain
            var appPage = findApplicationPageParent(root)
            
            if (appPage) {
                console.log("PortageActionInjector: Found ApplicationPage after", attempts, "attempts")
                console.log("PortageActionInjector: Calling PortageInjector.injectIntoApplicationPage()")
                
                var success = PortageInjector.injectIntoApplicationPage(appPage)
                console.log("PortageActionInjector: Injection", success ? "SUCCESS" : "FAILED")
                
                stop()
            } else if (attempts > 50) {
                console.warn("PortageActionInjector: ApplicationPage not found after 50 attempts, giving up")
                stop()
            }
        }
    }
    
    function findApplicationPageParent(item) {
        if (!item) return null
        
        var current = item.parent
        while (current) {
            var className = current.toString()
            
            // Check for ApplicationPage
            if (className.indexOf("ApplicationPage") >= 0) {
                console.log("PortageActionInjector: Found via parent chain:", className)
                return current
            }
            
            // Also check if it has the properties we expect
            if (current.hasOwnProperty("application") && 
                current.hasOwnProperty("actions")) {
                console.log("PortageActionInjector: Found via properties:", className)
                return current
            }
            
            current = current.parent
        }
        
        return null
    }
}
