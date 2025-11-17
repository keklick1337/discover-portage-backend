/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QSet>

class PortageResource;

struct UseFlagInfo {
    QString atom;                        // Package atom (category/package)
    QString version;                     // Installed version
    QStringList activeFlags;             // Currently active USE flags
    QStringList availableFlags;          // All available USE flags (from IUSE)
    QStringList rawIuse;                 // Raw IUSE with +/- prefixes for defaults
    QMap<QString, QString> descriptions; // Flag descriptions
    QString repository;                  // Source repository (gentoo, guru, etc.)
    QString slot;                        // Package slot
};

class PortageUseFlags : public QObject
{
    Q_OBJECT

public:
    explicit PortageUseFlags(QObject *parent = nullptr);
    ~PortageUseFlags() override;

    UseFlagInfo readInstalledPackageInfo(const QString &atom, const QString &version);

    QStringList readAvailableUseFlags(const QString &atom, const QString &repoPath = QStringLiteral("/var/db/repos/gentoo"));
    
    // Read USE flags from repository ebuild and metadata.xml
    UseFlagInfo readRepositoryPackageInfo(const QString &atom, const QString &version, const QString &repoPath = QStringLiteral("/var/db/repos/gentoo"));

    // Compute effective USE flags by combining:
    // 1. Global USE from make.conf
    // 2. IUSE defaults from ebuild
    // 3. package.use configurations
    // 4. Installed package USE (if package is installed)
    struct EffectiveUseFlags {
        QStringList enabled;      // Flags that will be enabled
        QStringList disabled;     // Flags that will be disabled
        QStringList iuse;         // All available flags from IUSE
        QMap<QString, QString> descriptions;
    };
    EffectiveUseFlags computeEffectiveUseFlags(const QString &atom, const QString &version, bool isInstalled);

    QMap<QString, QStringList> readPackageUseConfig(const QString &atom);

    bool writeUseFlags(const QString &atom, const QString &packageName, const QStringList &useFlags);

    bool removeUseFlagConfig(const QString &atom);

    static QString packageUseDir();

    static QString useFlagFileName(const QString &packageName);

    static QStringList parseIUSE(const QString &iuseLine);

    static QStringList parseUSE(const QString &useLine);
    
    static QMap<QString, QString> parseMetadataXml(const QString &metadataPath);
    
    // Helper methods for atom parsing
    static QString extractCategory(const QString &atom);
    static QString extractPackageName(const QString &atom);

private:
    QString readVarDbFile(const QString &atom, const QString &version, const QString &filename);

    QStringList findPackageUseFiles(const QString &atom);

    bool removeLinesFromFile(const QString &filePath, const QString &atom);

    QMap<QString, UseFlagInfo> m_cache;
};
