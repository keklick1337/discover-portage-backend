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

    QMap<QString, QStringList> readPackageUseConfig(const QString &atom);

    bool writeUseFlags(const QString &atom, const QString &packageName, const QStringList &useFlags);

    bool removeUseFlagConfig(const QString &atom);

    static QString packageUseDir();

    static QString useFlagFileName(const QString &packageName);

    static QStringList parseIUSE(const QString &iuseLine);

    static QStringList parseUSE(const QString &useLine);

private:
    QString readVarDbFile(const QString &atom, const QString &version, const QString &filename);

    QStringList findPackageUseFiles(const QString &atom);

    bool removeLinesFromFile(const QString &filePath, const QString &atom);

    QMap<QString, UseFlagInfo> m_cache;
};
