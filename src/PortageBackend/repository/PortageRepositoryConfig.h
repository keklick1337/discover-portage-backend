/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QMap>
#include <QString>
#include <QStringList>

/**
 * @brief Repository configuration parser and cache
 * 
 * Parses repository configuration from:
 * 1. portageq repositories_configuration /
 * 2. /etc/portage/repos.conf (file or directory)
 */
class PortageRepositoryConfig
{
public:
    struct Repository {
        QString name;
        QString location;
        QString syncType;
        QString syncUri;
        int priority = 0;
        bool autoSync = true;
    };

    static PortageRepositoryConfig& instance();
    
    void reload();
    
    QString getRepositoryLocation(const QString &name) const;
    QStringList getAllRepositoryNames() const;
    Repository getRepository(const QString &name) const;
    
private:
    PortageRepositoryConfig();
    
    QMap<QString, Repository> m_repositories;
    
    void parseFromPortageq();
    void parseFromReposConf();
    void parseRepositoriesFromSettings(class QSettings &settings);
};
