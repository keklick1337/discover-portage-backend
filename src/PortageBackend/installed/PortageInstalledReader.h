/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QHash>
#include <QSet>

class PortageBackend;

struct InstalledPackageInfo {
    QString version;
    QString repository;
    QString slot;
    QStringList useFlags;
    QStringList availableUseFlags; // from IUSE
};

class PortageInstalledReader : public QObject
{
    Q_OBJECT
public:
    explicit PortageInstalledReader(PortageBackend *backend, QObject *parent = nullptr);

    void loadInstalledPackages();

    QHash<QString, QString> installedVersions() const { return m_installedVersions; }
    QHash<QString, InstalledPackageInfo> installedPackagesInfo() const { return m_installedInfo; }

    void setKnownPackages(const QSet<QString> &knownAtoms) { m_knownAtoms = knownAtoms; }
    
    // Check if package is installed (exact atom or any version)
    bool isPackageInstalled(const QString &atom) const;
    
    // Find installed package version for atom (returns empty if not found)
    QString findInstalledVersion(const QString &atom) const;
    
    // Static helper: check if package directory exists in /var/db/pkg
    static bool packageExists(const QString &atom);
    
    // Static helper: find any installed version for atom
    static QString findPackageVersion(const QString &atom);

Q_SIGNALS:
    void packagesLoaded(int count);

private:
    void scanPkgDb(const QString &path);
    QString readFileContent(const QString &filePath);

    PortageBackend *m_backend;
    QHash<QString, QString> m_installedVersions; // atom -> version (for backwards compat)
    QHash<QString, InstalledPackageInfo> m_installedInfo; // atom -> full info
    QSet<QString> m_knownAtoms; // Known package atoms from repository
    QString m_pkgDbPath;
};
