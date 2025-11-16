/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageQmlInjector.h"
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlProperty>
#include <QJSValue>
#include <QVariant>
#include <QVariantList>
#include <QTimer>
#include <QDebug>

PortageQmlInjector::PortageQmlInjector(QObject *parent)
    : QObject(parent)
{
}

void PortageQmlInjector::setQmlEngine(QQmlEngine *engine)
{
    m_engine = engine;
    if (m_engine) {
        qDebug() << "PortageQmlInjector: QML engine set";

        QQmlContext *rootContext = m_engine->rootContext();
        if (rootContext) {
            rootContext->setContextProperty(QStringLiteral("PortageInjector"), this);
            qDebug() << "PortageQmlInjector: Registered as global QML object 'PortageInjector'";
        }
    }
}

bool PortageQmlInjector::isApplicationPage(QObject *obj) const
{
    if (!obj) {
        return false;
    }
    
    QString className = QString::fromLatin1(obj->metaObject()->className());
    if (className.contains(QLatin1String("ApplicationPage"))) {
        return true;
    }
    
    if (obj->property("application").isValid() && obj->property("actions").isValid()) {
        return true;
    }
    
    return false;
}

QObject *PortageQmlInjector::createReinstallAction(QObject *page)
{
    if (!m_engine || !page) {
        return nullptr;
    }
    
    // Load ReinstallAction.qml - it returns an Item with TransactionListener and Action
    QQmlComponent component(m_engine, QUrl(QStringLiteral("qrc:/qml/ReinstallAction.qml")));
    
    if (component.isError()) {
        qWarning() << "PortageQmlInjector: Failed to load ReinstallAction.qml:" << component.errors();
        return nullptr;
    }
    
    qDebug() << "PortageQmlInjector: Component loaded successfully, creating instance...";
    
    QObject *wrapper = component.create();
    if (!wrapper) {
        qWarning() << "PortageQmlInjector: Failed to instantiate ReinstallAction wrapper";
        if (component.isError()) {
            qWarning() << "PortageQmlInjector: Errors during creation:" << component.errors();
        }
        return nullptr;
    }
    
    qDebug() << "PortageQmlInjector: Wrapper instance created:" << wrapper;
    
    wrapper->setParent(page);
    
    QObject *application = qvariant_cast<QObject*>(page->property("application"));
    if (!application) {
        qWarning() << "PortageQmlInjector: Cannot get application from page";
        wrapper->deleteLater();
        return nullptr;
    }
    
    qDebug() << "PortageQmlInjector: Setting application property:" << application;
    wrapper->setProperty("application", QVariant::fromValue(application));
    
    // Get the actual Action from the wrapper's "action" property
    QVariant actionVar = wrapper->property("action");
    QObject *action = qvariant_cast<QObject*>(actionVar);
    
    if (!action) {
        qWarning() << "PortageQmlInjector: Failed to get action from wrapper";
        wrapper->deleteLater();
        return nullptr;
    }
    
    qDebug() << "PortageQmlInjector: Got Action from wrapper:" << action;
    
    return action;
}

bool PortageQmlInjector::injectIntoApplicationPage(QObject *page)
{
    if (!page || !m_engine) {
        qDebug() << "PortageQmlInjector: No page or engine";
        return false;
    }
    
    qDebug() << "PortageQmlInjector: Attempting injection into" << page;
    
    QObject *application = qvariant_cast<QObject*>(page->property("application"));
    if (!application) {
        qDebug() << "PortageQmlInjector: Page has no application property";
        return false;
    }
    
    // Check if this is a Portage package - multiple methods
    QString className = QString::fromLatin1(application->metaObject()->className());
    qDebug() << "PortageQmlInjector: Application class:" << className;
    
    // Method 1: Check class name
    bool isPortage = className.contains(QLatin1String("Portage"), Qt::CaseInsensitive);
    
    // Method 2: Check if requestReinstall() method exists (only in PortageResource)
    if (!isPortage) {
        isPortage = (application->metaObject()->indexOfMethod("requestReinstall()") >= 0);
        qDebug() << "PortageQmlInjector: Has requestReinstall():" << isPortage;
    }
    
    // Method 3: Try to get backend property as object
    if (!isPortage) {
        QVariant backendVar = application->property("backend");
        if (backendVar.isValid()) {
            QObject *backendObj = qvariant_cast<QObject*>(backendVar);
            if (backendObj) {
                QString backendClass = QString::fromLatin1(backendObj->metaObject()->className());
                qDebug() << "PortageQmlInjector: Backend class:" << backendClass;
                isPortage = backendClass.contains(QLatin1String("Portage"), Qt::CaseInsensitive);
            } else {
                QString backendStr = backendVar.toString();
                qDebug() << "PortageQmlInjector: Backend string:" << backendStr;
                isPortage = backendStr.contains(QLatin1String("Portage"), Qt::CaseInsensitive);
            }
        }
    }
    
    if (!isPortage) {
        qDebug() << "PortageQmlInjector: Not a Portage package, skipping";
        return false;
    }
    
    qDebug() << "PortageQmlInjector: Confirmed Portage package, creating Reinstall action...";
    QObject *reinstallAction = createReinstallAction(page);
    if (!reinstallAction) {
        qDebug() << "PortageQmlInjector: Failed to create action";
        return false;
    }
    
    qDebug() << "PortageQmlInjector: Action created:" << reinstallAction;
    
    // Use QQmlProperty to access QML list property
    QQmlProperty actionsProp(page, QStringLiteral("actions"));
    if (!actionsProp.isValid()) {
        qWarning() << "PortageQmlInjector: actions property not valid";
        return false;
    }
    
    qDebug() << "PortageQmlInjector: actions property type:" << actionsProp.propertyTypeName();
    
    // Read existing actions using QMetaObject
    QVariantList actionsList;
    QVariant actionsVar = actionsProp.read();
    
    // QQmlListProperty doesn't convert to QVariantList, need to use QML engine
    if (!actionsVar.canConvert<QVariantList>()) {
        // Try to get list using JavaScript evaluation
        QJSValue jsActions = m_engine->evaluate(QStringLiteral("(function(page){ var arr = []; for(var i=0; i<page.actions.length; i++) arr.push(page.actions[i]); return arr; })")).call(QJSValueList() << m_engine->newQObject(page));
        
        if (jsActions.isArray()) {
            int length = jsActions.property(QStringLiteral("length")).toInt();
            qDebug() << "PortageQmlInjector: Found" << length << "existing actions via JS";
            
            for (int i = 0; i < length; ++i) {
                QJSValue item = jsActions.property(i);
                QObject *obj = item.toQObject();
                if (obj) {
                    actionsList.append(QVariant::fromValue(obj));
                }
            }
        } else {
            qDebug() << "PortageQmlInjector: Could not read existing actions, starting fresh";
        }
    } else {
        actionsList = actionsVar.toList();
    }
    
    qDebug() << "PortageQmlInjector: Current actions count:" << actionsList.size();
    
    // Print existing actions
    for (int i = 0; i < actionsList.size(); ++i) {
        QObject *act = qvariant_cast<QObject*>(actionsList[i]);
        if (act) {
            QString actionText = act->property("text").toString();
            QString actionIcon = act->property("icon.name").toString();
            qDebug() << "  Existing action" << i << ":" << actionText << actionIcon;
        }
    }
    
    // Find position to insert (after Launch/Invoke action if exists)
    int insertPos = actionsList.size();
    
    for (int i = 0; i < actionsList.size(); ++i) {
        QObject *act = qvariant_cast<QObject*>(actionsList[i]);
        if (act) {
            QString actionText = act->property("text").toString();
            QString actionIcon = act->property("icon.name").toString();
            
            // Insert after Launch/Invoke action
            if (actionText.contains(QLatin1String("Launch")) || 
                actionText.contains(QLatin1String("Invoke")) ||
                actionIcon.contains(QLatin1String("launch")) ||
                actionIcon.contains(QLatin1String("invoke"))) {
                insertPos = i + 1;
                qDebug() << "  Found launch action at" << i << ", will insert at position" << insertPos;
                break;
            }
        }
    }
    
    // Insert the new action
    actionsList.insert(insertPos, QVariant::fromValue(reinstallAction));
    
    qDebug() << "PortageQmlInjector: Writing" << actionsList.size() << "actions back (including new Reinstall)";
    
    // Try to write back using JavaScript
    QJSValue jsPage = m_engine->newQObject(page);
    QJSValue jsArray = m_engine->newArray(actionsList.size());
    for (int i = 0; i < actionsList.size(); ++i) {
        QObject *obj = qvariant_cast<QObject*>(actionsList[i]);
        if (obj) {
            jsArray.setProperty(i, m_engine->newQObject(obj));
        }
    }
    
    QJSValue setActions = m_engine->evaluate(QStringLiteral("(function(page, actions){ page.actions = actions; })"));
    setActions.call(QJSValueList() << jsPage << jsArray);
    
    qDebug() << "PortageQmlInjector: Successfully injected Reinstall action at position" << insertPos;
    
    return true;
}

#include "moc_PortageQmlInjector.cpp"
