/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageInstalledReader.h"
#include "../PortageBackend.h"
#include "../PortageUseFlags.h"

#include <QDir>
#include <QDebug>
#include <QFile>

PortageInstalledReader::PortageInstalledReader(PortageBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_pkgDbPath(QStringLiteral("/var/db/pkg"))
{
}

void PortageInstalledReader::loadInstalledPackages()
{
    qDebug() << "Portage: InstalledReader loading from" << m_pkgDbPath;
    QDir pkgdir(m_pkgDbPath);
    if (!pkgdir.exists()) {
        qDebug() << "Portage: pkg db path does not exist:" << m_pkgDbPath;
        Q_EMIT packagesLoaded(0);
        return;
    }

    scanPkgDb(m_pkgDbPath);
    qDebug() << "Portage: InstalledReader found" << m_installedVersions.size() << "installed packages";
    Q_EMIT packagesLoaded(m_installedVersions.size());
}

void PortageInstalledReader::scanPkgDb(const QString &path)
{
    QDir top(path);
    const QFileInfoList categories = top.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &catInfo : categories) {
        const QString category = catInfo.fileName();
        QDir catDir(catInfo.absoluteFilePath());
        const QFileInfoList pkgDirs = catDir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &pkgInfo : pkgDirs) {
            const QString dirname = pkgInfo.fileName();
            // dirname is like package-version; split at last '-'
            int idx = dirname.lastIndexOf(QLatin1Char('-'));
            if (idx <= 0)
                continue;
            const QString pkg = dirname.left(idx);
            const QString ver = dirname.mid(idx + 1);
            const QString atom = category + QLatin1Char('/') + pkg;
            
            // Read additional information from the package directory
            InstalledPackageInfo info;
            info.version = ver;
            
            const QString pkgPath = pkgInfo.absoluteFilePath();
            info.repository = readFileContent(pkgPath + QStringLiteral("/repository"));
            info.slot = readFileContent(pkgPath + QStringLiteral("/SLOT"));
            
            // Read USE flags
            const QString useContent = readFileContent(pkgPath + QStringLiteral("/USE"));
            info.useFlags = PortageUseFlags::parseUSE(useContent);
            
            // Read available USE flags (IUSE)
            const QString iuseContent = readFileContent(pkgPath + QStringLiteral("/IUSE"));
            info.availableUseFlags = PortageUseFlags::parseIUSE(iuseContent);
            
            m_installedVersions.insert(atom.toLower(), ver);
            m_installedInfo.insert(atom.toLower(), info);
        }
    }
}

QString PortageInstalledReader::readFileContent(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QString content = QString::fromUtf8(file.readAll()).trimmed();
    file.close();
    return content;
}
