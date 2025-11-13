/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <Transaction/Transaction.h>

class PortageResource;

class PortageTransaction : public Transaction
{
    Q_OBJECT
public:
    PortageTransaction(PortageResource *app, Role role);
    PortageTransaction(PortageResource *app, const AddonList &addons, Role role);

    void cancel() override;
    void proceed() override;

private Q_SLOTS:
    void simulateProgress();
    void finishTransaction();

private:
    PortageResource *m_resource;
    AddonList m_addons;
    int m_progress;
};
