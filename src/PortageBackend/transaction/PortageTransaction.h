/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <Transaction/Transaction.h>
#include "../emerge/EmergeRunner.h"

class PortageResource;
class UnmaskManager;

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
    void onEmergeOutput(const QString &line);
    void onEmergeError(const QString &line);
    void onEmergeFinished(bool success, int exitCode);
    void onDependenciesChecked(const EmergeRunner::EmergeResult &result);

private:
    PortageResource *m_resource;
    AddonList m_addons;
    int m_progress;
    EmergeRunner *m_emergeRunner;
    UnmaskManager *m_unmaskManager;
    
    void handleUnmaskRequest(const EmergeRunner::EmergeResult &result);
};
