/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "UnmaskManager.h"
#include "../utils/StringUtils.h"
#include "../utils/PortagePaths.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <KAuth/Action>
#include <KAuth/ExecuteJob>

UnmaskManager::UnmaskManager(QObject *parent)
    : QObject(parent)
    , m_unmaskFilePath(QLatin1String(PortagePaths::PACKAGE_ACCEPT_KEYWORDS) + QStringLiteral("/discover_unmask"))
{
}

void UnmaskManager::unmaskPackage(const QString &atom, const QString &keyword, std::function<void(bool)> callback)
{
    QStringList lines;
    readUnmaskFile(lines);
    
    // Check if already exists
    QString entry = atom + QLatin1Char(' ') + keyword;
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(atom + QLatin1Char(' '))) {
            qDebug() << "UnmaskManager: Package already unmasked:" << atom;
            callback(true);
            return;
        }
    }
    
    // Add new entry
    lines.append(entry);
    
    qDebug() << "UnmaskManager: Unmasking package:" << entry;
    writeUnmaskFileAsync(lines, callback);
}

bool UnmaskManager::maskPackage(const QString &atom)
{
    QStringList lines;
    if (!readUnmaskFile(lines)) {
        return false;
    }
    
    // Remove entry
    QStringList newLines;
    bool found = false;
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(atom + QLatin1Char(' '))) {
            found = true;
            qDebug() << "UnmaskManager: Removing unmask for:" << atom;
            continue;
        }
        newLines.append(line);
    }
    
    if (!found) {
        qDebug() << "UnmaskManager: Package not found in unmask file:" << atom;
        return false;
    }
    
    return writeUnmaskFile(newLines);
}

bool UnmaskManager::isUnmasked(const QString &atom) const
{
    QStringList lines;
    if (!readUnmaskFile(lines)) {
        return false;
    }
    
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(atom + QLatin1Char(' '))) {
            return true;
        }
    }
    
    return false;
}

QStringList UnmaskManager::getUnmaskedPackages() const
{
    QStringList packages;
    QStringList lines;
    
    if (!readUnmaskFile(lines)) {
        return packages;
    }
    
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) {
            continue;
        }
        
        // Extract atom (first part before space)
        int spaceIdx = trimmed.indexOf(QLatin1Char(' '));
        if (spaceIdx > 0) {
            packages.append(trimmed.left(spaceIdx));
        }
    }
    
    return packages;
}

bool UnmaskManager::readUnmaskFile(QStringList &lines) const
{
    QFile file(m_unmaskFilePath);
    
    if (!file.exists()) {
        qDebug() << "UnmaskManager: Unmask file does not exist, will create:" << m_unmaskFilePath;
        lines.clear();
        return true;
    }
    
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "UnmaskManager: Failed to open unmask file for reading:" << m_unmaskFilePath;
        return false;
    }
    
    QTextStream in(&file);
    lines.clear();
    
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    
    file.close();
    return true;
}

void UnmaskManager::writeUnmaskFileAsync(const QStringList &lines, std::function<void(bool)> callback)
{
    QString content;
    QTextStream stream(&content);
    
    stream << getFileHeader() << "\n\n";
    
    for (const QString &line : lines) {
        if (StringUtils::isCommentOrEmpty(line)) {
            continue;
        }
        stream << line << "\n";
    }

    KAuth::Action writeAction(QStringLiteral("org.kde.discover.portagebackend.execute"));
    writeAction.setHelperId(QStringLiteral("org.kde.discover.portagebackend"));
    writeAction.setTimeout(-1);
    
    QVariantMap args;
    args[QStringLiteral("action")] = QStringLiteral("file.write");
    args[QStringLiteral("path")] = m_unmaskFilePath;
    args[QStringLiteral("content")] = content;
    args[QStringLiteral("append")] = false;
    
    writeAction.setArguments(args);
    
    qDebug() << "UnmaskManager: Executing KAuth action to write unmask file";
    KAuth::ExecuteJob *job = writeAction.execute();

    QObject::connect(job, &KAuth::ExecuteJob::result, this, [callback](KJob *finishedJob) {
        KAuth::ExecuteJob *authJob = static_cast<KAuth::ExecuteJob *>(finishedJob);
        bool success = (authJob->error() == 0);
        if (success) {
            qDebug() << "UnmaskManager: Successfully wrote unmask file via KAuth";
        } else {
            qWarning() << "UnmaskManager: KAuth action failed:" << authJob->errorString();
        }
        callback(success);
    });
    
    job->start();
}

bool UnmaskManager::writeUnmaskFile(const QStringList &lines) const
{
    QString content;
    QTextStream stream(&content);
    
    stream << getFileHeader() << "\n\n";
    
    for (const QString &line : lines) {
        if (StringUtils::isCommentOrEmpty(line)) {
            continue;
        }
        stream << line << "\n";
    }

    // Use KAuth synchronously for backward compatibility (maskPackage)
    KAuth::Action writeAction(QStringLiteral("org.kde.discover.portagebackend.execute"));
    writeAction.setHelperId(QStringLiteral("org.kde.discover.portagebackend"));
    writeAction.setTimeout(-1);
    
    QVariantMap args;
    args[QStringLiteral("action")] = QStringLiteral("file.write");
    args[QStringLiteral("path")] = m_unmaskFilePath;
    args[QStringLiteral("content")] = content;
    args[QStringLiteral("append")] = false;
    
    writeAction.setArguments(args);
    
    KAuth::ExecuteJob *job = writeAction.execute();
    job->exec();  // Synchronous
    
    return (job->error() == 0);
}

QString UnmaskManager::getFileHeader() const
{
    return QStringLiteral("# This file is managed by KDE Discover\n"
                         "# Manual changes may be overwritten\n"
                         "# Package keyword unmasking for Discover-installed packages");
}
