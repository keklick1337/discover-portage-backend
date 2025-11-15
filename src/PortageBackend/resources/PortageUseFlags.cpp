/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageUseFlags.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>
#include <QFileInfo>

PortageUseFlags::PortageUseFlags(QObject *parent)
    : QObject(parent)
{
}

PortageUseFlags::~PortageUseFlags()
{
}

UseFlagInfo PortageUseFlags::readInstalledPackageInfo(const QString &atom, const QString &version)
{
    const QString cacheKey = atom + QStringLiteral("-") + version;
    if (m_cache.contains(cacheKey)) {
        return m_cache.value(cacheKey);
    }

    UseFlagInfo info;
    info.atom = atom;
    info.version = version;

    const QString useContent = readVarDbFile(atom, version, QStringLiteral("USE"));
    info.activeFlags = parseUSE(useContent);

    const QString iuseContent = readVarDbFile(atom, version, QStringLiteral("IUSE"));
    info.availableFlags = parseIUSE(iuseContent);

    info.repository = readVarDbFile(atom, version, QStringLiteral("repository")).trimmed();

    info.slot = readVarDbFile(atom, version, QStringLiteral("SLOT"));
    
    // Read USE flag descriptions from repository metadata.xml
    if (!info.repository.isEmpty()) {
        QString metadataPath = QStringLiteral("/var/db/repos/%1/%2/metadata.xml").arg(info.repository, atom);
        info.descriptions = parseMetadataXml(metadataPath);
    }

    m_cache.insert(cacheKey, info);

    qDebug() << "PortageUseFlags: Read installed package info for" << atom << version
             << "- Active:" << info.activeFlags.size() << "Available:" << info.availableFlags.size()
             << "Repo:" << info.repository << "Slot:" << info.slot
             << "Descriptions:" << info.descriptions.size();

    return info;
}

QString PortageUseFlags::readVarDbFile(const QString &atom, const QString &version, const QString &filename)
{
    const QString path = QStringLiteral("/var/db/pkg/%1-%2/%3").arg(atom, version, filename);
    QFile file(path);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "PortageUseFlags: Could not open" << path;
        return QString();
    }

    QString content = QString::fromUtf8(file.readAll()).trimmed();
    file.close();
    
    return content;
}

QStringList PortageUseFlags::parseUSE(const QString &useLine)
{
    if (useLine.isEmpty()) {
        return QStringList();
    }

    // USE flags are space-separated
    return useLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

QStringList PortageUseFlags::parseIUSE(const QString &iuseLine)
{
    if (iuseLine.isEmpty()) {
        return QStringList();
    }

    QStringList flags;
    // IUSE can have flags with +/- prefix indicating defaults
    const QStringList rawFlags = iuseLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    
    for (const QString &flag : rawFlags) {
        // Remove leading +/- if present
        QString cleanFlag = flag;
        if (cleanFlag.startsWith(QLatin1Char('+')) || cleanFlag.startsWith(QLatin1Char('-'))) {
            cleanFlag = cleanFlag.mid(1);
        }
        if (!cleanFlag.isEmpty()) {
            flags << cleanFlag;
        }
    }

    return flags;
}

QStringList PortageUseFlags::readAvailableUseFlags(const QString &atom, const QString &repoPath)
{
    // Try to read IUSE from repository metadata
    // Format: /var/db/repos/gentoo/category/package/*.ebuild
    const QString category = atom.section(QLatin1Char('/'), 0, 0);
    const QString package = atom.section(QLatin1Char('/'), 1, 1);
    
    const QString packageDir = QStringLiteral("%1/%2/%3").arg(repoPath, category, package);
    QDir dir(packageDir);
    
    if (!dir.exists()) {
        qDebug() << "PortageUseFlags: Repository directory not found:" << packageDir;
        return QStringList();
    }

    // Look for metadata.xml or parse ebuild files
    const QString metadataPath = packageDir + QStringLiteral("/metadata.xml");
    QFile metadataFile(metadataPath);
    
    QStringList flags;
    
    // For now, we'll rely on IUSE from installed packages
    // A more complete implementation would parse metadata.xml for descriptions
    // TODO: implement metadata.xml parsing for IUSE descriptions
    
    return flags;
}

QMap<QString, QStringList> PortageUseFlags::readPackageUseConfig(const QString &atom)
{
    QMap<QString, QStringList> result;
    
    const QStringList files = findPackageUseFiles(atom);
    
    for (const QString &filePath : files) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            
            if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) {
                continue;
            }

            // Line format: "category/package flag1 flag2 -flag3"
            if (line.contains(atom)) {
                QStringList parts = line.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                if (!parts.isEmpty() && parts.first() == atom) {
                    parts.removeFirst();
                    result.insert(filePath, parts);
                }
            }
        }
        file.close();
    }

    return result;
}

QStringList PortageUseFlags::findPackageUseFiles(const QString &atom)
{
    QStringList result;
    const QString packageUseDirectory = packageUseDir();
    
    QDir dir(packageUseDirectory);
    if (!dir.exists()) {
        qDebug() << "PortageUseFlags: package.use directory not found:" << packageUseDirectory;
        return result;
    }

    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    
    for (const QFileInfo &fileInfo : files) {
        QFile file(fileInfo.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine().trimmed();
            if (line.contains(atom)) {
                result << fileInfo.absoluteFilePath();
                break;
            }
        }
        file.close();
    }

    return result;
}

bool PortageUseFlags::writeUseFlags(const QString &atom, const QString &packageName, const QStringList &useFlags)
{
    // First, remove existing configurations
    if (!removeUseFlagConfig(atom)) {
        qDebug() << "PortageUseFlags: Warning - could not remove existing config for" << atom;
    }

    const QString packageUseDirectory = packageUseDir();
    const QString fileName = useFlagFileName(packageName);
    const QString filePath = packageUseDirectory + QStringLiteral("/") + fileName;

    // Ensure directory exists
    QDir dir;
    if (!dir.mkpath(packageUseDirectory)) {
        qDebug() << "PortageUseFlags: Could not create directory:" << packageUseDirectory;
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        qDebug() << "PortageUseFlags: Could not open file for writing:" << filePath;
        return false;
    }

    QTextStream out(&file);
    
    out << "# Managed by Discover - " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n";
    
    out << atom;
    for (const QString &flag : useFlags) {
        out << " " << flag;
    }
    out << "\n";

    file.close();

    qDebug() << "PortageUseFlags: Wrote USE flags for" << atom << "to" << filePath;
    return true;
}

bool PortageUseFlags::removeUseFlagConfig(const QString &atom)
{
    const QStringList files = findPackageUseFiles(atom);
    
    bool success = true;
    for (const QString &filePath : files) {
        if (!removeLinesFromFile(filePath, atom)) {
            qDebug() << "PortageUseFlags: Failed to remove lines from" << filePath;
            success = false;
        }
    }

    return success;
}

bool PortageUseFlags::removeLinesFromFile(const QString &filePath, const QString &atom)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        // Keep the line if it doesn't contain the atom or is a comment
        QString trimmedLine = line.trimmed();
        if (trimmedLine.isEmpty() || trimmedLine.startsWith(QLatin1Char('#'))) {
            lines << line;
        } else {
            QStringList parts = trimmedLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
            if (parts.isEmpty() || parts.first() != atom) {
                lines << line;
            }
            // else: skip this line (it's for the atom we want to remove)
        }
    }
    file.close();

    // Rewrite the file
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }

    QTextStream out(&file);
    for (const QString &line : lines) {
        out << line << "\n";
    }
    file.close();

    return true;
}

QString PortageUseFlags::packageUseDir()
{
    return QStringLiteral("/etc/portage/package.use");
}

QString PortageUseFlags::useFlagFileName(const QString &packageName)
{
    return QStringLiteral("discover_") + packageName;
}

QMap<QString, QString> PortageUseFlags::parseMetadataXml(const QString &metadataPath)
{
    QMap<QString, QString> descriptions;
    
    QFile file(metadataPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return descriptions;
    }
    
    QString content = QString::fromUtf8(file.readAll());
    file.close();

    QRegularExpression flagRegex(QStringLiteral(R"(<flag\s+name=\"([^\"]+)\">([^<]+)</flag>)"));
    QRegularExpressionMatchIterator it = flagRegex.globalMatch(content);
    
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        QString flagName = match.captured(1);
        QString description = match.captured(2).trimmed();

        description = description.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));

        descriptions[flagName] = description;
    }
    
    qDebug() << "PortageUseFlags: Parsed" << descriptions.size() << "USE flag descriptions from" << metadataPath;
    
    return descriptions;
}

UseFlagInfo PortageUseFlags::readRepositoryPackageInfo(const QString &atom, const QString &version, const QString &repoPath)
{
    UseFlagInfo info;
    info.atom = atom;
    info.version = version;
    info.repository = QFileInfo(repoPath).fileName(); // Extract repo name from path
    
    // Find ebuild file
    QString ebuildPath = QStringLiteral("%1/%2/%3-%4.ebuild").arg(repoPath, atom, atom.section(QLatin1Char('/'), -1), version);
    
    QFile ebuildFile(ebuildPath);
    if (ebuildFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&ebuildFile);
        QString line;
        
        // Read ebuild line by line to find IUSE
        while (in.readLineInto(&line)) {
            if (line.startsWith(QStringLiteral("IUSE="))) {
                // Extract IUSE value
                QString iuseLine = line.mid(5).trimmed();
                
                // Remove quotes if present
                if (iuseLine.startsWith(QLatin1Char('"')) && iuseLine.endsWith(QLatin1Char('"'))) {
                    iuseLine = iuseLine.mid(1, iuseLine.length() - 2);
                }
                
                info.availableFlags = parseIUSE(iuseLine);
                break;
            }
        }
        ebuildFile.close();
    } else {
        qDebug() << "PortageUseFlags: Could not open ebuild" << ebuildPath;
    }
    
    // Read metadata.xml for descriptions
    QString metadataPath = QStringLiteral("%1/%2/metadata.xml").arg(repoPath, atom);
    info.descriptions = parseMetadataXml(metadataPath);
    
    qDebug() << "PortageUseFlags: Read repository package info for" << atom << version
             << "- Available:" << info.availableFlags.size()
             << "- Descriptions:" << info.descriptions.size();
    
    return info;
}
