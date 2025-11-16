/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "UnmaskManager.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QDebug>
#include <QEventLoop>
#include <KAuth/Action>
#include <KAuth/ExecuteJob>

UnmaskManager::UnmaskManager(QObject *parent)
    : QObject(parent)
    , m_unmaskFilePath(QStringLiteral("/etc/portage/package.accept_keywords/discover_unmask"))
{
}

bool UnmaskManager::unmaskPackage(const QString &atom, const QString &keyword)
{
    QStringList lines;
    readUnmaskFile(lines);
    
    // Check if already exists
    QString entry = atom + QLatin1Char(' ') + keyword;
    for (const QString &line : lines) {
        if (line.trimmed().startsWith(atom + QLatin1Char(' '))) {
            qDebug() << "UnmaskManager: Package already unmasked:" << atom;
            return true;
        }
    }
    
    // Add new entry
    lines.append(entry);
    
    qDebug() << "UnmaskManager: Unmasking package:" << entry;
    return writeUnmaskFile(lines);
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

bool UnmaskManager::writeUnmaskFile(const QStringList &lines) const
{
    QString content;
    QTextStream stream(&content);
    
    stream << getFileHeader() << "\n\n";
    
    for (const QString &line : lines) {
        if (line.trimmed().isEmpty() || line.trimmed().startsWith(QLatin1Char('#'))) {
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

    bool success = false;
    QEventLoop loop;
    QObject::connect(job, &KAuth::ExecuteJob::result, [&success, &loop](KJob *finishedJob) {
        KAuth::ExecuteJob *authJob = static_cast<KAuth::ExecuteJob *>(finishedJob);
        if (authJob->error() == 0) {
            qDebug() << "UnmaskManager: Successfully wrote unmask file via KAuth";
            success = true;
        } else {
            qWarning() << "UnmaskManager: KAuth action failed:" << authJob->errorString();
        }
        loop.quit();
    });
    
    job->start();
    loop.exec(); // Wait for completion
    
    return success;
}

QString UnmaskManager::getFileHeader() const
{
    return QStringLiteral("# This file is managed by KDE Discover\n"
                         "# Manual changes may be overwritten\n"
                         "# Package keyword unmasking for Discover-installed packages");
}
