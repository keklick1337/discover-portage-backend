/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class MakeConfReader
{
public:
    MakeConfReader();
    
    QStringList readGlobalUseFlags() const;
    
    QStringList readL10N() const;
    
    QString readVariable(const QString &variableName) const;
    
private:
    QString parseVariable(const QString &filePath, const QString &variableName) const;
    
    static constexpr const char *MAKE_CONF_PATH = "/etc/portage/make.conf";
};
