/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageRepositoryConfig.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QTemporaryFile>

PortageRepositoryConfig& PortageRepositoryConfig::instance()
{
    static PortageRepositoryConfig inst;
    return inst;
}

PortageRepositoryConfig::PortageRepositoryConfig()
{
    reload();
}

void PortageRepositoryConfig::reload()
{
    m_repositories.clear();
    
    // Try portageq first (most reliable)
    parseFromPortageq();
    
    // Fallback to repos.conf if portageq didn't return anything
    if (m_repositories.isEmpty()) {
        parseFromReposConf();
    }
    
    qDebug() << "PortageRepositoryConfig: Loaded" << m_repositories.size() << "repositories";
}

QString PortageRepositoryConfig::getRepositoryLocation(const QString &name) const
{
    if (m_repositories.contains(name)) {
        return m_repositories.value(name).location;
    }
    return QString();
}

QStringList PortageRepositoryConfig::getAllRepositoryNames() const
{
    return m_repositories.keys();
}

PortageRepositoryConfig::Repository PortageRepositoryConfig::getRepository(const QString &name) const
{
    return m_repositories.value(name);
}

void PortageRepositoryConfig::parseFromPortageq()
{
    QProcess proc;
    proc.start(QStringLiteral("portageq"), QStringList{QStringLiteral("repositories_configuration"), QStringLiteral("/")});
    if (!proc.waitForFinished(5000)) {
        qDebug() << "PortageRepositoryConfig: portageq timed out";
        return;
    }
    
    if (proc.exitCode() != 0) {
        qDebug() << "PortageRepositoryConfig: portageq failed with exit code" << proc.exitCode();
        return;
    }
    
    const QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
    if (output.isEmpty()) {
        qDebug() << "PortageRepositoryConfig: portageq returned empty output";
        return;
    }
    
    // Write output to temporary file for QSettings to parse
    QTemporaryFile tempFile;
    if (!tempFile.open()) {
        qDebug() << "PortageRepositoryConfig: Failed to create temporary file";
        return;
    }
    tempFile.write(output.toUtf8());
    tempFile.flush();
    
    QSettings settings(tempFile.fileName(), QSettings::IniFormat);
    parseRepositoriesFromSettings(settings);
    
    qDebug() << "PortageRepositoryConfig: Parsed" << m_repositories.size() << "repositories from portageq";
}

void PortageRepositoryConfig::parseFromReposConf()
{
    const QString reposConfPath = QStringLiteral("/etc/portage/repos.conf");
    QFileInfo fi(reposConfPath);
    
    if (!fi.exists()) {
        qDebug() << "PortageRepositoryConfig: /etc/portage/repos.conf does not exist";
        return;
    }
    
    // QSettings can read single file or we need to merge directory
    if (fi.isFile()) {
        QSettings settings(reposConfPath, QSettings::IniFormat);
        parseRepositoriesFromSettings(settings);
    } else if (fi.isDir()) {
        // Merge all .conf files from directory
        QString combined;
        QDirIterator it(reposConfPath, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QFile f(it.filePath());
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                combined += QString::fromUtf8(f.readAll()) + QStringLiteral("\n");
            }
        }
        
        if (!combined.isEmpty()) {
            QTemporaryFile tempFile;
            if (tempFile.open()) {
                tempFile.write(combined.toUtf8());
                tempFile.flush();
                
                QSettings settings(tempFile.fileName(), QSettings::IniFormat);
                parseRepositoriesFromSettings(settings);
            }
        }
    }
    
    qDebug() << "PortageRepositoryConfig: Parsed" << m_repositories.size() << "repositories from repos.conf";
}

void PortageRepositoryConfig::parseRepositoriesFromSettings(QSettings &settings)
{
    const QStringList groups = settings.childGroups();
    for (const QString &name : groups) {
        // Skip [DEFAULT] section
        if (name == QLatin1String("DEFAULT"))
            continue;
        
        settings.beginGroup(name);
        
        Repository repo;
        repo.name = name;
        repo.location = settings.value(QStringLiteral("location")).toString();
        repo.syncType = settings.value(QStringLiteral("sync-type")).toString();
        repo.syncUri = settings.value(QStringLiteral("sync-uri")).toString();
        repo.priority = settings.value(QStringLiteral("priority"), 0).toInt();
        repo.autoSync = settings.value(QStringLiteral("auto-sync"), true).toBool();
        
        settings.endGroup();
        
        if (!repo.location.isEmpty()) {
            m_repositories.insert(name, repo);
        }
    }
}
