/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QHash>
#include <resources/AbstractResourcesBackend.h>

class PortageResource;
class StandardBackendUpdater;

class PortageBackend : public AbstractResourcesBackend
{
    Q_OBJECT
public:
    explicit PortageBackend(QObject *parent = nullptr);

    QString displayName() const override;
    bool hasApplications() const override { return true; }
    bool isValid() const override { return true; }
    InlineMessage *explainDysfunction() const override { return nullptr; }

    ResultsStream *search(const AbstractResourcesBackend::Filters &filter) override;
    QList<std::shared_ptr<Category>> category() const override;

    int updatesCount() const override;
    AbstractBackendUpdater *backendUpdater() const override;
    void checkForUpdates() override;
    int fetchingUpdatesProgress() const override { return 100; }

    Transaction *installApplication(AbstractResource *app) override;
    Transaction *installApplication(AbstractResource *app, const AddonList &addons) override;
    Transaction *removeApplication(AbstractResource *app) override;

    AbstractReviewsBackend *reviewsBackend() const override { return nullptr; }

    QHash<QString, PortageResource *> resources() const { return m_resources; }

private:
    void populateTestPackages();

    QHash<QString, PortageResource *> m_resources;
    StandardBackendUpdater *m_updater;
    bool m_initialized;
};
