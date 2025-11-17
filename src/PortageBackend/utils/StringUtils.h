/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QString>

class StringUtils
{
public:
    static bool isCommentOrEmpty(const QString &line)
    {
        QString trimmed = line.trimmed();
        return trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'));
    }
    
    static bool isCommentOrEmptyTrimmed(const QString &trimmedLine)
    {
        return trimmedLine.isEmpty() || trimmedLine.startsWith(QLatin1Char('#'));
    }
};
