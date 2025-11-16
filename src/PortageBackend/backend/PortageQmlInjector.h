/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QQmlContext>
#include <QQuickItem>

class PortageQmlInjector : public QObject
{
    Q_OBJECT
public:
    explicit PortageQmlInjector(QObject *parent = nullptr);

    void setQmlEngine(QQmlEngine *engine);

    Q_INVOKABLE bool injectIntoApplicationPage(QObject *page);
    
private:
    QQmlEngine *m_engine = nullptr;

    bool isApplicationPage(QObject *obj) const;

    QObject *createReinstallAction(QObject *page);
};
