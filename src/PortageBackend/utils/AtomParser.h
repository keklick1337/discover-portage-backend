/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QString>
#include <QPair>

class AtomParser
{
public:
    static QString extractCategory(const QString &atom)
    {
        return atom.section(QLatin1Char('/'), 0, 0);
    }
    
    static QString extractPackageName(const QString &atom)
    {
        return atom.section(QLatin1Char('/'), 1);
    }
    
    static QPair<QString, QString> splitAtom(const QString &atom)
    {
        int slashPos = atom.indexOf(QLatin1Char('/'));
        if (slashPos > 0) {
            return {atom.left(slashPos), atom.mid(slashPos + 1)};
        }
        return {QString(), atom};
    }
    
    static QString extractPackageNameForFile(const QString &atom)
    {
        QString cleanAtom = atom;
        
        // Remove version prefix if present (=, ~, <, >, etc.)
        if (!cleanAtom.isEmpty() && !cleanAtom[0].isLetter()) {
            cleanAtom = cleanAtom.mid(1);
        }
        
        // Extract package name after slash
        int slashIndex = cleanAtom.lastIndexOf(QLatin1Char('/'));
        if (slashIndex != -1) {
            return cleanAtom.mid(slashIndex + 1);
        }
        
        return cleanAtom;
    }
};
