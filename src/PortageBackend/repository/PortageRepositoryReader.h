/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QHash>

class PortageBackend;
class PortageResource;

class PortageRepositoryReader : public QObject
{
    Q_OBJECT
public:
    explicit PortageRepositoryReader(PortageBackend *backend, QObject *parent = nullptr);

    /**
     * Load repository package list from disk (synchronous, simple parsing)
     * # TODO: implement actual parsing of ebuilds to get versions, use flags, etc.
     */
    void loadRepository();

    QHash<QString, PortageResource *> packages() const { return m_packages; }

private:
    QStringList findAvailableVersions(const QString &pkgPath, const QString &pkgName);

Q_SIGNALS:
    void packagesLoaded(int count);

private:
    void scanRepositoryPath(const QString &path);
    QString findLatestVersion(const QString &pkgPath, const QString &pkgName);

    PortageBackend *m_backend;
    QHash<QString, PortageResource *> m_packages;
    QString m_repoPath;
};
