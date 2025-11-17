/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageRepositoryReader.h"
#include "../backend/PortageBackend.h"
#include "../resources/PortageResource.h"
#include "../utils/AtomParser.h"

#include <QDir>
#include <QDebug>

PortageRepositoryReader::PortageRepositoryReader(PortageBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_repoPath(QStringLiteral("/var/db/repos"))
{
}

void PortageRepositoryReader::loadRepository()
{
    qDebug() << "Portage: RepositoryReader loading from" << m_repoPath;

    QDir reposDir(m_repoPath);
    if (!reposDir.exists()) {
        qDebug() << "Portage: repository path does not exist:" << m_repoPath;
        Q_EMIT packagesLoaded(0);
        return;
    }

    // Iterate over repositories (e.g., gentoo)
    const QFileInfoList repoEntries = reposDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &repoInfo : repoEntries) {
        const QString repoPath = repoInfo.absoluteFilePath();
        scanRepositoryPath(repoPath);
    }

    qDebug() << "Portage: RepositoryReader found" << m_packages.size() << "packages (scan only)";
    Q_EMIT packagesLoaded(m_packages.size());
}

void PortageRepositoryReader::scanRepositoryPath(const QString &path)
{
    QDir repo(path);
    const QString repoName = QFileInfo(path).fileName();
    
    const QFileInfoList categories = repo.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &catInfo : categories) {
        const QString catPath = catInfo.absoluteFilePath();
        const QString category = catInfo.fileName();
        QDir catDir(catPath);
        const QFileInfoList packages = catDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &pkgInfo : packages) {
            const QString pkg = pkgInfo.fileName();
            const QString atom = category + QLatin1Char('/') + pkg;
            if (m_packages.contains(atom))
                continue;
            
            PortageResource *res = new PortageResource(atom, pkg, QString(), m_backend);
            res->setRepository(repoName);
            
            // Don't load versions here
            // Versions will be loaded lazily when user opens package page
            // TODO: Add versions caching mechanism if needed
            
            m_packages.insert(atom, res);
        }
    }
}

QStringList PortageRepositoryReader::findAvailableVersions(const QString &pkgPath, const QString &pkgName)
{
    QDir pkgDir(pkgPath);
    QStringList ebuilds = pkgDir.entryList(QStringList() << QStringLiteral("*.ebuild"), QDir::Files, QDir::Name);

    QStringList versions;
    for (const QString &ebuild : ebuilds) {
        QString version = ebuild;
        version.remove(0, pkgName.length() + 1);
        if (version.endsWith(QLatin1String(".ebuild"))) {
            version.chop(7);
        }
        if (!version.isEmpty()) {
            versions << version;
        }
    }

    // Sort descending (latest first)
    std::sort(versions.begin(), versions.end(), std::greater<QString>());
    versions.erase(std::unique(versions.begin(), versions.end()), versions.end());
    return versions;
}

// Static helper: Get all repository names from /var/db/repos
QStringList PortageRepositoryReader::getAllRepositories()
{
    QDir reposDir(QStringLiteral("/var/db/repos"));
    if (!reposDir.exists()) {
        return QStringList();
    }
    
    return reposDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

// Static helper: Find which repository contains a package
QString PortageRepositoryReader::findPackageRepository(const QString &atom)
{
    QDir reposDir(QStringLiteral("/var/db/repos"));
    if (!reposDir.exists()) {
        return QString();
    }
    
    const QStringList repos = reposDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &repo : repos) {
        QString testPath = QStringLiteral("/var/db/repos/%1/%2").arg(repo, atom);
        if (QDir(testPath).exists()) {
            return repo;
        }
    }
    
    return QString();
}

// Static helper: Get full path to package in repository
QString PortageRepositoryReader::findPackagePath(const QString &atom, const QString &repository)
{
    QString repo = repository;
    
    // If no repository specified, find it
    if (repo.isEmpty()) {
        repo = findPackageRepository(atom);
        if (repo.isEmpty()) {
            return QString();
        }
    }
    
    QString pkgPath = QStringLiteral("/var/db/repos/%1/%2").arg(repo, atom);
    if (QDir(pkgPath).exists()) {
        return pkgPath;
    }
    
    return QString();
}

// Static helper: Check if package exists in repository
bool PortageRepositoryReader::packageExistsInRepo(const QString &atom, const QString &repository)
{
    return !findPackagePath(atom, repository).isEmpty();
}

// Static helper: Get available versions for a package
QStringList PortageRepositoryReader::getAvailableVersions(const QString &atom, const QString &repository)
{
    QString pkgPath = findPackagePath(atom, repository);
    if (pkgPath.isEmpty()) {
        return QStringList();
    }
    
    QString pkgName = AtomParser::extractPackageName(atom);
    
    QDir pkgDir(pkgPath);
    QStringList ebuilds = pkgDir.entryList(QStringList() << QStringLiteral("*.ebuild"), QDir::Files, QDir::Name);
    
    QStringList versions;
    for (const QString &ebuild : ebuilds) {
        QString version = ebuild;
        version.remove(0, pkgName.length() + 1);
        if (version.endsWith(QLatin1String(".ebuild"))) {
            version.chop(7);
        }
        if (!version.isEmpty()) {
            versions << version;
        }
    }
    
    // Sort descending (latest first)
    std::sort(versions.begin(), versions.end(), std::greater<QString>());
    versions.erase(std::unique(versions.begin(), versions.end()), versions.end());
    
    return versions;
}

