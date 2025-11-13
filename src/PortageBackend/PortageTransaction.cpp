/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageTransaction.h"
#include "PortageResource.h"
#include "PortageBackend.h"
#include <KLocalizedString>
#include <QTimer>
#include <QDebug>

PortageTransaction::PortageTransaction(PortageResource *app, Role role)
    : Transaction(app, app, role)
    , m_resource(app)
    , m_progress(0)
{
    qDebug() << "Portage: Transaction created for" << app->name();
    setCancellable(true);
    setStatus(QueuedStatus);
}

PortageTransaction::PortageTransaction(PortageResource *app, const AddonList &addons, Role role)
    : Transaction(app, app, role, addons)
    , m_resource(app)
    , m_addons(addons)
    , m_progress(0)
{
    qDebug() << "Portage: Transaction with addons created for" << app->name();
    setCancellable(true);
    setStatus(QueuedStatus);
}

void PortageTransaction::cancel()
{
    qDebug() << "Portage: Transaction cancelled";
    setStatus(CancelledStatus);
}

void PortageTransaction::proceed()
{
    qDebug() << "Portage: Transaction proceeding";
    setStatus(CommittingStatus);
    
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &PortageTransaction::simulateProgress);
    timer->start(100);
}

void PortageTransaction::simulateProgress()
{
    m_progress += 5;
    setProgress(m_progress);
    
    if (m_progress >= 100) {
        finishTransaction();
    }
}

void PortageTransaction::finishTransaction()
{
    qDebug() << "Portage: Transaction finished";
    
    if (role() == InstallRole) {
        m_resource->setState(AbstractResource::Installed);
        m_resource->setInstalledVersion(m_resource->availableVersion());
    } else if (role() == RemoveRole) {
        m_resource->setState(AbstractResource::None);
        m_resource->setInstalledVersion(QString());
    }
    
    setStatus(DoneStatus);
}

#include "moc_PortageTransaction.cpp"
