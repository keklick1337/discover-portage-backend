/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class UnmaskManager : public QObject
{
    Q_OBJECT
public:
    explicit UnmaskManager(QObject *parent = nullptr);

    void unmaskPackage(const QString &atom, const QString &keyword, std::function<void(bool)> callback);

    bool maskPackage(const QString &atom);

    bool isUnmasked(const QString &atom) const;

    QStringList getUnmaskedPackages() const;

private:
    QString m_unmaskFilePath;
    
    bool readUnmaskFile(QStringList &lines) const;
    bool writeUnmaskFile(const QStringList &lines) const;  // Sync version for maskPackage
    void writeUnmaskFileAsync(const QStringList &lines, std::function<void(bool)> callback);
    QString getFileHeader() const;
};
