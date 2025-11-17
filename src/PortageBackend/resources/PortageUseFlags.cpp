/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageUseFlags.h"
#include "config/MakeConfReader.h"
#include "../repository/PortageRepositoryReader.h"
#include "../installed/PortageInstalledReader.h"
#include "../utils/StringUtils.h"
#include "../utils/PortagePaths.h"
#include <QFile>
#include <QDir>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>
#include <QDateTime>
#include <QFileInfo>
#include <QProcess>

PortageUseFlags::PortageUseFlags(QObject *parent)
    : QObject(parent)
{
}

PortageUseFlags::~PortageUseFlags()
{
}

UseFlagInfo PortageUseFlags::readInstalledPackageInfo(const QString &atom, const QString &version)
{
    QString actualVersion = version;
    
    // If version is empty, auto-detect from /var/db/pkg
    if (actualVersion.isEmpty()) {
        actualVersion = PortageInstalledReader::findPackageVersion(atom);
        if (!actualVersion.isEmpty()) {
            qDebug() << "PortageUseFlags: Auto-detected version" << actualVersion << "for" << atom;
        }
    }
    
    const QString cacheKey = atom + QStringLiteral("-") + actualVersion;
    if (m_cache.contains(cacheKey)) {
        return m_cache.value(cacheKey);
    }

    UseFlagInfo info;
    info.atom = atom;
    info.version = actualVersion;

    const QString useContent = readVarDbFile(atom, actualVersion, QStringLiteral("USE"));
    info.activeFlags = parseUSE(useContent);

    const QString iuseContent = readVarDbFile(atom, actualVersion, QStringLiteral("IUSE"));
    info.availableFlags = parseIUSE(iuseContent);

    info.repository = readVarDbFile(atom, actualVersion, QStringLiteral("repository")).trimmed();

    info.slot = readVarDbFile(atom, actualVersion, QStringLiteral("SLOT"));
    
    // Read USE flag descriptions from repository metadata.xml
    if (!info.repository.isEmpty()) {
        QString pkgPath = PortageRepositoryReader::findPackagePath(atom, info.repository);
        if (!pkgPath.isEmpty()) {
            QString metadataPath = pkgPath + QStringLiteral("/metadata.xml");
            info.descriptions = parseMetadataXml(metadataPath);
        }
    }

    m_cache.insert(cacheKey, info);

    qDebug() << "PortageUseFlags: Read installed package info for" << atom << actualVersion
             << "- Active:" << info.activeFlags.size() << "Available:" << info.availableFlags.size()
             << "Repo:" << info.repository << "Slot:" << info.slot
             << "Descriptions:" << info.descriptions.size();

    return info;
}

QString PortageUseFlags::readVarDbFile(const QString &atom, const QString &version, const QString &filename)
{
    // First try exact version match
    QString exactPath = QLatin1String(PortagePaths::PKG_DB) + QLatin1Char('/') + 
                        atom + QLatin1Char('-') + version + QLatin1Char('/') + filename;
    QFile exactFile(exactPath);
    
    if (exactFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(exactFile.readAll()).trimmed();
        exactFile.close();
        return content;
    }
    
    // If exact match failed, search for version with revision (e.g., 1.2.3-r1)
    QDir pkgDir(QLatin1String(PortagePaths::PKG_DB) + QLatin1Char('/') + atom);
    if (!pkgDir.exists()) {
        qDebug() << "PortageUseFlags: Package directory does not exist:" << pkgDir.path();
        return QString();
    }
    
    // Find directories that start with "version-"
    QStringList filters;
    filters << version + QLatin1String("-r*");  // version-r1, version-r2, etc.
    filters << version;                          // exact version without revision
    
    QFileInfoList entries = pkgDir.entryInfoList(filters, QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (entries.isEmpty()) {
        qDebug() << "PortageUseFlags: No matching version found for" << atom << version << "in" << pkgDir.path();
        return QString();
    }
    
    // Use the first match (should be only one installed version)
    QString foundPath = entries.first().filePath() + QLatin1Char('/') + filename;
    QFile file(foundPath);
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "PortageUseFlags: Could not open" << foundPath;
        return QString();
    }

    QString content = QString::fromUtf8(file.readAll()).trimmed();
    file.close();
    
    qDebug() << "PortageUseFlags: Read" << filename << "from" << foundPath;
    
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
    const QString category = extractCategory(atom);
    const QString package = extractPackageName(atom);
    
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
        if (StringUtils::isCommentOrEmpty(line)) {
            lines << line;
        } else {
            QString trimmedLine = line.trimmed();
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
    return QLatin1String(PortagePaths::PACKAGE_USE);
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
    
    // Use portageq to get IUSE after ebuild processing (handles dynamic generation like L10N)
    QProcess portageq;
    portageq.setProgram(QStringLiteral("portageq"));
    portageq.setArguments({
        QStringLiteral("metadata"),
        QStringLiteral("/"),
        QStringLiteral("ebuild"),
        QStringLiteral("%1-%2").arg(atom, version),
        QStringLiteral("IUSE")
    });
    
    portageq.start();
    portageq.waitForFinished(10000); // 10 second timeout
    
    if (portageq.exitCode() == 0) {
        QString iuseOutput = QString::fromUtf8(portageq.readAllStandardOutput()).trimmed();
        
        // Save raw IUSE with prefixes for defaults
        info.rawIuse = iuseOutput.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        
        // Parse to get clean flag names (without +/-)
        info.availableFlags = parseIUSE(iuseOutput);
        
        qDebug() << "PortageUseFlags: Got" << info.availableFlags.size() << "flags from portageq for" << atom << version;
    } else {
        // Fallback to reading ebuild file directly (won't catch dynamic flags like L10N)
        qDebug() << "PortageUseFlags: portageq failed, falling back to ebuild parsing";
        
        QString packageName = extractPackageName(atom);
        QString ebuildPath = QStringLiteral("%1/%2/%3-%4.ebuild").arg(repoPath, atom, packageName, version);
        
        QFile ebuildFile(ebuildPath);
        if (ebuildFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&ebuildFile);
            QString line;
            QString iuseAccumulated;
            
            // Read ebuild line by line to find all IUSE lines (IUSE= and IUSE+=)
            while (in.readLineInto(&line)) {
                QString trimmed = line.trimmed();
                
                // Match IUSE= or IUSE+=
                if (trimmed.startsWith(QStringLiteral("IUSE=")) || trimmed.startsWith(QStringLiteral("IUSE+="))) {
                    // Extract IUSE value
                    int eqPos = trimmed.indexOf(QLatin1Char('='));
                    QString iuseLine = trimmed.mid(eqPos + 1).trimmed();
                    
                    // Remove quotes if present
                    if (iuseLine.startsWith(QLatin1Char('"')) && iuseLine.endsWith(QLatin1Char('"'))) {
                        iuseLine = iuseLine.mid(1, iuseLine.length() - 2);
                    }
                    
                    // Accumulate all IUSE values
                    if (!iuseAccumulated.isEmpty()) {
                        iuseAccumulated += QLatin1Char(' ');
                    }
                    iuseAccumulated += iuseLine;
                }
            }
            
            // Parse accumulated IUSE
            if (!iuseAccumulated.isEmpty()) {
                info.availableFlags = parseIUSE(iuseAccumulated);
            }
            
            ebuildFile.close();
        } else {
            qDebug() << "PortageUseFlags: Could not open ebuild" << ebuildPath;
        }
    }
    
    // Read metadata.xml for descriptions
    QString metadataPath = QStringLiteral("%1/%2/metadata.xml").arg(repoPath, atom);
    info.descriptions = parseMetadataXml(metadataPath);
    
    qDebug() << "PortageUseFlags: Read repository package info for" << atom << version
             << "- Available:" << info.availableFlags.size()
             << "- Descriptions:" << info.descriptions.size();
    
    return info;
}

PortageUseFlags::EffectiveUseFlags PortageUseFlags::computeEffectiveUseFlags(const QString &atom, const QString &version, bool isInstalled)
{
    EffectiveUseFlags result;
    
    // 1. Get IUSE from repository ebuild
    UseFlagInfo repoInfo = readRepositoryPackageInfo(atom, version);
    result.iuse = repoInfo.availableFlags;
    result.descriptions = repoInfo.descriptions;
    
    // 2. Start with global USE flags from make.conf
    MakeConfReader makeConf;
    QStringList globalUse = makeConf.readGlobalUseFlags();
    QStringList globalL10n = makeConf.readL10N();
    
    QSet<QString> enabledSet;
    QSet<QString> disabledSet;
    
    for (const QString &flag : globalUse) {
        if (flag.startsWith(QLatin1Char('-'))) {
            disabledSet.insert(flag.mid(1));
        } else {
            enabledSet.insert(flag);
        }
    }
    
    // 3. Apply IUSE defaults (flags with +/- prefix from portageq)
    // But SKIP L10N flags - they should only be enabled if in L10N variable
    for (const QString &rawFlag : repoInfo.rawIuse) {
        QString cleanFlag;
        bool isDefault = false;
        bool isDisabled = false;
        
        if (rawFlag.startsWith(QLatin1Char('+'))) {
            cleanFlag = rawFlag.mid(1);
            isDefault = true;
        } else if (rawFlag.startsWith(QLatin1Char('-'))) {
            cleanFlag = rawFlag.mid(1);
            isDisabled = true;
        } else {
            cleanFlag = rawFlag;
        }
        
        // Special handling for L10N flags
        if (cleanFlag.startsWith(QStringLiteral("l10n_"))) {
            // L10N flags are ONLY enabled if they're in global L10N variable
            if (globalL10n.contains(cleanFlag)) {
                enabledSet.insert(cleanFlag);
                disabledSet.remove(cleanFlag);
            } else {
                // Not in L10N - disable it
                disabledSet.insert(cleanFlag);
                enabledSet.remove(cleanFlag);
            }
        } else {
            // Regular flags - apply IUSE defaults
            if (isDefault) {
                enabledSet.insert(cleanFlag);
                disabledSet.remove(cleanFlag);
            } else if (isDisabled) {
                disabledSet.insert(cleanFlag);
                enabledSet.remove(cleanFlag);
            }
        }
    }
    
    // 4. Apply package-specific USE from package.use files
    QMap<QString, QStringList> packageUse = readPackageUseConfig(atom);
    for (const QStringList &flags : packageUse) {
        for (const QString &flag : flags) {
            if (flag.startsWith(QLatin1Char('-'))) {
                QString cleanFlag = flag.mid(1);
                disabledSet.insert(cleanFlag);
                enabledSet.remove(cleanFlag);
            } else {
                enabledSet.insert(flag);
                disabledSet.remove(flag);
            }
        }
    }
    
    // 5. If package is installed, use its actual USE flags as final state
    if (isInstalled) {
        UseFlagInfo installedInfo = readInstalledPackageInfo(atom, version);
        if (!installedInfo.activeFlags.isEmpty()) {
            // The installed USE flags are the final truth
            QSet<QString> installedSet(installedInfo.activeFlags.begin(), installedInfo.activeFlags.end());
            
            // All IUSE flags not in installed set are disabled
            enabledSet = installedSet;
            disabledSet.clear();
            for (const QString &flag : result.iuse) {
                if (!enabledSet.contains(flag)) {
                    disabledSet.insert(flag);
                }
            }
        }
    }
    
    // Convert sets to lists
    result.enabled = enabledSet.values();
    result.disabled = disabledSet.values();
    
    // Only include flags that are in IUSE
    QSet<QString> iuseSet(result.iuse.begin(), result.iuse.end());
    QSet<QString> enabledIuseSet = QSet<QString>(result.enabled.begin(), result.enabled.end()).intersect(iuseSet);
    QSet<QString> disabledIuseSet = QSet<QString>(result.disabled.begin(), result.disabled.end()).intersect(iuseSet);
    result.enabled = enabledIuseSet.values();
    result.disabled = disabledIuseSet.values();
    
    qDebug() << "PortageUseFlags: computeEffectiveUseFlags for" << atom << version
             << "- Enabled:" << result.enabled.size() << "Disabled:" << result.disabled.size()
             << "- IUSE:" << result.iuse.size();
    
    return result;
}

QString PortageUseFlags::extractCategory(const QString &atom)
{
    return atom.section(QLatin1Char('/'), 0, 0);
}

QString PortageUseFlags::extractPackageName(const QString &atom)
{
    return atom.section(QLatin1Char('/'), 1);
}
