/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <resources/AbstractResource.h>
#include <QStringList>

class PortageResource : public AbstractResource
{
    Q_OBJECT
    Q_PROPERTY(QStringList installedUseFlags READ installedUseFlags NOTIFY useFlagsChanged)
    Q_PROPERTY(QStringList availableUseFlags READ availableUseFlags NOTIFY useFlagsChanged)
    Q_PROPERTY(QStringList configuredUseFlags READ configuredUseFlags WRITE setConfiguredUseFlags NOTIFY useFlagsChanged)
    Q_PROPERTY(QVariantList useFlagsInformation READ useFlagsInformation NOTIFY useFlagsChanged)
    Q_PROPERTY(QStringList availableVersions READ availableVersions NOTIFY metadataChanged)
    Q_PROPERTY(QString requestedVersion READ requestedVersion WRITE setRequestedVersion NOTIFY metadataChanged)
    Q_PROPERTY(QString slot READ slot NOTIFY metadataChanged)
    Q_PROPERTY(QString repository READ repository NOTIFY metadataChanged)
    
public:
    explicit PortageResource(const QString &atom,
                           const QString &name,
                           const QString &summary,
                           AbstractResourcesBackend *parent);

    // Display full atom (category/package) as the resource name so UI shows
    QString name() const override { return m_atom; }
    QString packageName() const override { return m_packageName; }
    QString comment() override { return m_summary; }
    QString longDescription() override;
    QVariant icon() const override;
    QString section() override { return m_category; }
    QString origin() const override { return QStringLiteral("Portage"); }
    
    QString availableVersion() const override { return m_availableVersion; }
    QString installedVersion() const override { return m_installedVersion; }
    
    quint64 size() override { return m_size; }
    
    State state() override { return m_state; }
    
    bool hasCategory(const QString &category) const override;
    
    QUrl homepage() override;
    QUrl helpURL() override { return QUrl(); }
    QUrl bugURL() override;
    QUrl donationURL() override { return QUrl(); }
    QUrl contributeURL() override { return QUrl(); }
    QUrl url() const override;
    
    QJsonArray licenses() override;
    
    QString author() const override;
    
    AbstractResource::Type type() const override { return AbstractResource::Application; }
    
    bool canExecute() const override { return m_state == AbstractResource::Installed; }
    void invokeApplication() const override;
    
    void fetchChangelog() override;
    void fetchScreenshots() override;
    QDate releaseDate() const override { return QDate(); }
    QString sourceIcon() const override { return QStringLiteral("application-x-archive"); }
    QList<PackageState> addonsInformation() override;
    
    QStringList topObjects() const override;
    
    QVariantList useFlagsInformation();
    
    void setState(State state);
    void setAvailableVersion(const QString &version) { m_availableVersion = version; }
    void setInstalledVersion(const QString &version) { m_installedVersion = version; }
    void setSize(quint64 size) { m_size = size; }
    void setRepository(const QString &repo);
    void setSlot(const QString &slot);

    QStringList availableVersions();
    void setAvailableVersions(const QStringList &versions) { m_availableVersions = versions; Q_EMIT metadataChanged(); }

    QString requestedVersion() const { return m_requestedVersion; }
    void setRequestedVersion(const QString &v) { m_requestedVersion = v; Q_EMIT metadataChanged(); }

    Q_INVOKABLE void requestInstallVersion(const QString &version);
    Q_INVOKABLE void requestReinstall();
    
    // USE flag management
    QStringList installedUseFlags() const { return m_installedUseFlags; }
    void setInstalledUseFlags(const QStringList &flags);
    
    QStringList availableUseFlags() const { return m_availableUseFlags; }
    void setAvailableUseFlags(const QStringList &flags);
    
    QStringList configuredUseFlags() const { return m_configuredUseFlags; }
    void setConfiguredUseFlags(const QStringList &flags);
    
    Q_INVOKABLE bool saveUseFlags(const QStringList &flags);
    
    QMap<QString, QString> useFlagDescriptions() const { return m_useFlagDescriptions; }
    
    QString slot() const { return m_slot; }
    
    QString keyword() const { return m_keyword; }
    void setKeyword(const QString &keyword) { m_keyword = keyword; }
    
    QString atom() const { return m_atom; }
    QString repository() const { return m_repository; }

    void loadMetadata();
    void loadUseFlagInfo();

Q_SIGNALS:
    void useFlagsChanged();
    void metadataChanged();

private:
    QString m_atom;
    QString m_category;
    QString m_packageName;
    QString m_name;
    QString m_summary;
    QString m_availableVersion;
    QString m_installedVersion;
    QString m_repository;
    QString m_slot;
    quint64 m_size;
    AbstractResource::State m_state;
    QSet<QString> m_discoverCategories;
    
    QStringList m_installedUseFlags;    // Currently active USE flags (from /var/db/pkg)
    QStringList m_availableUseFlags;    // All available USE flags (from IUSE)
    QStringList m_configuredUseFlags;   // User-configured USE flags (from /etc/portage/package.use)
    
    QString m_keyword;

    QStringList m_availableVersions;
    QString m_requestedVersion;

    QString m_longDescription;
    QString m_ebuildDescription;
    QStringList m_maintainerNames;
    QStringList m_maintainerEmails;
    QMap<QString, QString> m_useFlagDescriptions; // flag name -> description
};
