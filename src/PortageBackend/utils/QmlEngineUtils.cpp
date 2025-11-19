/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "QmlEngineUtils.h"

#include <QQmlEngine>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QWindow>
#include <QQuickWindow>
#include <QQuickItem>
#include <QDebug>

namespace QmlEngineUtils
{

QQmlEngine *findQmlEngine()
{
    qDebug() << "QmlEngineUtils: Starting QML engine search...";
    QQmlEngine *engine = nullptr;
    
    // Method 1: Search all QML engines in entire app
    auto engines = QCoreApplication::instance()->findChildren<QQmlEngine*>();
    qDebug() << "QmlEngineUtils: findChildren() found" << engines.size() << "QML engine(s)";
    if (!engines.isEmpty()) {
        engine = engines.first();
        qDebug() << "QmlEngineUtils: Returning first QML engine from findChildren";
        return engine;
    }
    
    // Method 2: If not found, try top-level windows
    const auto topLevelObjects = qApp->topLevelWindows();
    qDebug() << "QmlEngineUtils: Checking" << topLevelObjects.size() << "top-level windows";
    
    int windowIndex = 0;
    for (QWindow *window : topLevelObjects) {
        qDebug() << "QmlEngineUtils: Checking window" << windowIndex << ":" << window;
        
        QObject *obj = qobject_cast<QObject*>(window);
        if (!obj) {
            qDebug() << "QmlEngineUtils: Window" << windowIndex << "is not a QObject";
            windowIndex++;
            continue;
        }
        
        // Try direct property
        engine = qvariant_cast<QQmlEngine*>(obj->property("engine"));
        if (engine) {
            qDebug() << "QmlEngineUtils: Found QML engine via window" << windowIndex << "property";
            return engine;
        }

        // Try to get engine from QQuickWindow's contentItem
        QQuickWindow *quickWindow = qobject_cast<QQuickWindow*>(window);
        if (quickWindow) {
            qDebug() << "QmlEngineUtils: Window" << windowIndex << "is a QQuickWindow";
            
            // Get engine via qmlEngine() from the content item
            QQuickItem *contentItem = quickWindow->contentItem();
            if (contentItem) {
                qDebug() << "QmlEngineUtils: Found contentItem:" << contentItem;
                engine = qmlEngine(contentItem);
                if (engine) {
                    qDebug() << "QmlEngineUtils: Found QML engine via QQuickWindow contentItem";
                    return engine;
                }
            } else {
                qDebug() << "QmlEngineUtils: QQuickWindow has no contentItem";
            }
        }

        // Try findChildren as fallback
        const auto children = obj->findChildren<QQmlEngine*>();
        qDebug() << "QmlEngineUtils: Window" << windowIndex << "has" << children.size() << "QML engine children";
        if (!children.isEmpty()) {
            engine = children.first();
            qDebug() << "QmlEngineUtils: Returning first QML engine from window" << windowIndex << "children";
            return engine;
        }
        
        windowIndex++;
    }
    
    qWarning() << "QmlEngineUtils: QML engine not found after all search methods!";
    return nullptr;
}

} // namespace QmlEngineUtils
