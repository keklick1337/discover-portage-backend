/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <resources/AbstractSourcesBackend.h>
#include <QStandardItemModel>
#include <QXmlStreamReader>

class DiscoverAction;
class PortageBackend;

struct RepositoryInfo {
    QString name;
    QString description;
    QString homepage;
    QString ownerEmail;
    QString ownerName;
    QString sourceUrl;  // First git/mercurial source URL
    QString feed;
    QString quality;    // "experimental", etc.
    QString status;     // "official", "unofficial"
    bool enabled = false;
};

class PortageSourcesBackend : public AbstractSourcesBackend
{
    Q_OBJECT
public:
    explicit PortageSourcesBackend(AbstractResourcesBackend *parent);
    ~PortageSourcesBackend() override;

    enum Roles {
        EnabledRole = AbstractSourcesBackend::LastRole + 1,
        DescriptionRole,
        HomepageRole,
        OwnerRole,
        QualityRole,
        StatusRole,
            DeletableRole,
    };

    QAbstractItemModel *sources() override;
    bool addSource(const QString &id) override;
    bool removeSource(const QString &id) override;
    QString idDescription() override;
    QVariantList actions() const override;
    bool supportsAdding() const override { return true; }
    bool canFilterSources() const override { return false; }

    void loadOfficialRepositories();
    QList<RepositoryInfo> officialRepositories() const { return m_officialRepos; }
    
    // Add manual repository with custom sync settings
    Q_INVOKABLE bool addManualSource(const QString &name, const QString &syncType, const QString &syncUri);
    
private Q_SLOTS:
    void refreshSources();
    void handleOfficialReposDownloaded();
    void showAddOverlayDialog();
    
private:
    void loadEnabledRepositories();
    void parseRepositoriesXml(const QByteArray &xmlData);
    QStandardItem *findSourceByName(const QString &name) const;
    void syncRepository(const QString &id);
    void loadRepositoriesFromEselect();
    QList<RepositoryInfo> loadAvailableRepositoriesFromEselect();
    
    QStandardItemModel *m_sources;
    DiscoverAction *m_refreshAction;
    DiscoverAction *m_addOverlayAction;
    QList<RepositoryInfo> m_officialRepos;
    QStandardItem *m_noSourcesItem;
};
