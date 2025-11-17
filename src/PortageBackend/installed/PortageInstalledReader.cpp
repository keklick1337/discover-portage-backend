/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageInstalledReader.h"
#include "../backend/PortageBackend.h"
#include "../resources/PortageUseFlags.h"
#include "../utils/AtomParser.h"
#include "../utils/PortagePaths.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>

PortageInstalledReader::PortageInstalledReader(PortageBackend *backend, QObject *parent)
    : QObject(parent)
    , m_backend(backend)
    , m_pkgDbPath(QLatin1String(PortagePaths::PKG_DB))
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
            
            // Try to match against known packages first for better accuracy
            QString pkg;
            QString ver;
            const QString fullAtom = category + QLatin1Char('/');
            
            bool foundMatch = false;
            if (!m_knownAtoms.isEmpty()) {
                for (int i = dirname.length() - 1; i >= 0; --i) {
                    if (dirname[i] == QLatin1Char('-') && i + 1 < dirname.length() && dirname[i + 1].isDigit()) {
                        QString testPkg = dirname.left(i);
                        QString testAtom = fullAtom + testPkg;
                        
                        if (m_knownAtoms.contains(testAtom.toLower())) {
                            pkg = testPkg;
                            ver = dirname.mid(i + 1);
                            foundMatch = true;
                            break;
                        }
                    }
                }
            }

            if (!foundMatch) {
                QRegularExpression versionRe(QStringLiteral("^(.+)-(\\d.*)$"));
                QRegularExpressionMatch match = versionRe.match(dirname);
                
                if (!match.hasMatch())
                    continue;
                    
                pkg = match.captured(1);
                ver = match.captured(2);
            }
            
            const QString atom = fullAtom + pkg;

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

bool PortageInstalledReader::isPackageInstalled(const QString &atom) const
{
    return m_installedVersions.contains(atom.toLower());
}

QString PortageInstalledReader::findInstalledVersion(const QString &atom) const
{
    // Check if exact atom exists
    QString normalized = atom.toLower();
    if (m_installedVersions.contains(normalized)) {
        return m_installedVersions.value(normalized);
    }
    
    // If not found, try to find by scanning /var/db/pkg directory
    return findPackageVersion(atom);
}

bool PortageInstalledReader::packageExists(const QString &atom)
{
    QString category = AtomParser::extractCategory(atom);
    QString packageName = AtomParser::extractPackageName(atom);
    
    QDir varDbDir(QLatin1String(PortagePaths::PKG_DB) + QLatin1Char('/') + category);
    if (!varDbDir.exists()) {
        return false;
    }
    
    QStringList entries = varDbDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (entry.startsWith(packageName + QLatin1Char('-'))) {
            return true;
        }
    }
    
    return false;
}

QString PortageInstalledReader::findPackageVersion(const QString &atom)
{
    QString category = AtomParser::extractCategory(atom);
    QString packageName = AtomParser::extractPackageName(atom);
    
    QDir categoryDir(QLatin1String(PortagePaths::PKG_DB) + QLatin1Char('/') + category);
    if (!categoryDir.exists()) {
        return QString();
    }
    
    QStringList entries = categoryDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QString &entry : entries) {
        if (entry.startsWith(packageName + QLatin1Char('-'))) {
            QString version = entry.mid(packageName.length() + 1);
            return version;
        }
    }
    
    return QString();
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
