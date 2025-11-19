/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageRepositoryReader.h"
#include "PortageRepositoryConfig.h"
#include "../backend/PortageBackend.h"
#include "../resources/PortageResource.h"
#include "../utils/AtomParser.h"

#include <QDir>
#include <QDebug>

PortageRepositoryReader::PortageRepositoryReader(PortageBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_repoPath()
{
    // Reload config on init
    PortageRepositoryConfig::instance().reload();
}

void PortageRepositoryReader::loadRepository()
{
    const QStringList allRepos = PortageRepositoryConfig::instance().getAllRepositoryNames();
    qDebug() << "Portage: RepositoryReader loading from" << allRepos.size() << "repositories";

    if (allRepos.isEmpty()) {
        qDebug() << "Portage: no repositories found in configuration";
        Q_EMIT packagesLoaded(0);
        return;
    }

    // Iterate over all configured repositories
    for (const QString &repoName : allRepos) {
        const QString repoPath = PortageRepositoryConfig::instance().getRepositoryLocation(repoName);
        if (!repoPath.isEmpty() && QDir(repoPath).exists()) {
            scanRepositoryPath(repoPath);
        } else {
            qDebug() << "Portage: Repository" << repoName << "path not found:" << repoPath;
        }
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

// Static helper: Get all repository names from configuration
QStringList PortageRepositoryReader::getAllRepositories()
{
    return PortageRepositoryConfig::instance().getAllRepositoryNames();
}

// Static helper: Find which repository contains a package
QString PortageRepositoryReader::findPackageRepository(const QString &atom)
{
    const QStringList repos = PortageRepositoryConfig::instance().getAllRepositoryNames();
    for (const QString &repo : repos) {
        const QString repoPath = PortageRepositoryConfig::instance().getRepositoryLocation(repo);
        const QString testPath = repoPath + QLatin1Char('/') + atom;
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
    
    const QString repoPath = PortageRepositoryConfig::instance().getRepositoryLocation(repo);
    if (repoPath.isEmpty()) {
        return QString();
    }
    
    const QString pkgPath = repoPath + QLatin1Char('/') + atom;
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

