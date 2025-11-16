/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "MakeConfReader.h"

#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRegularExpression>

MakeConfReader::MakeConfReader()
{
}

QStringList MakeConfReader::readGlobalUseFlags() const
{
    QString useValue = readVariable(QStringLiteral("USE"));
    if (useValue.isEmpty()) {
        return {};
    }
    
    // Split by whitespace and return list
    return useValue.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

QStringList MakeConfReader::readL10N() const
{
    QString l10nValue = readVariable(QStringLiteral("L10N"));
    if (l10nValue.isEmpty()) {
        return {};
    }
    
    // Split by whitespace
    QStringList locales = l10nValue.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
    
    // Convert to l10n_XX format for USE flag matching
    QStringList l10nFlags;
    for (const QString &locale : locales) {
        l10nFlags << QStringLiteral("l10n_") + locale;
    }
    
    return l10nFlags;
}

QString MakeConfReader::readVariable(const QString &variableName) const
{
    return parseVariable(QString::fromLatin1(MAKE_CONF_PATH), variableName);
}

QString MakeConfReader::parseVariable(const QString &filePath, const QString &variableName) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "MakeConfReader: Could not open" << filePath;
        return QString();
    }
    
    QTextStream in(&file);
    QString result;
    bool foundVariable = false;
    
    // Regular expressions to match variable assignment
    // Matches: VARIABLE="value" or VARIABLE='value' or VARIABLE=value
    QRegularExpression varRegex(QStringLiteral("^\\s*%1\\s*=\\s*(.*)$").arg(QRegularExpression::escape(variableName)));
    // Matches: VARIABLE+="value" or VARIABLE+='value' or VARIABLE+=value
    QRegularExpression varAppendRegex(QStringLiteral("^\\s*%1\\s*\\+=\\s*(.*)$").arg(QRegularExpression::escape(variableName)));
    
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmedLine = line.trimmed();
        
        // Skip comments and empty lines
        if (trimmedLine.isEmpty() || trimmedLine.startsWith(QLatin1Char('#'))) {
            continue;
        }
        
        // Check for VARIABLE= assignment
        auto match = varRegex.match(trimmedLine);
        bool isAppend = false;
        
        if (!match.hasMatch()) {
            // Check for VARIABLE+= append
            match = varAppendRegex.match(trimmedLine);
            isAppend = match.hasMatch();
        }
        
        if (match.hasMatch()) {
            foundVariable = true;
            QString currentLine = match.captured(1).trimmed();
            QString accumulated;
            
            // Handle multiline values
            bool inQuotes = false;
            QChar quoteChar;
            
            // Check if we're starting with a quote
            if (!currentLine.isEmpty()) {
                if (currentLine.startsWith(QLatin1Char('"'))) {
                    inQuotes = true;
                    quoteChar = QLatin1Char('"');
                } else if (currentLine.startsWith(QLatin1Char('\''))) {
                    inQuotes = true;
                    quoteChar = QLatin1Char('\'');
                }
            }
            
            // Read multiline value
            while (true) {
                // Check if line ends with backslash (line continuation)
                bool hasContinuation = currentLine.endsWith(QLatin1Char('\\'));
                
                // Check if we're still in quotes
                if (inQuotes) {
                    // Count unescaped quotes in current line
                    int quoteCount = 0;
                    bool escaped = false;
                    for (int i = 1; i < currentLine.length(); ++i) { // Start from 1 to skip opening quote
                        if (escaped) {
                            escaped = false;
                            continue;
                        }
                        if (currentLine[i] == QLatin1Char('\\')) {
                            escaped = true;
                            continue;
                        }
                        if (currentLine[i] == quoteChar) {
                            quoteCount++;
                        }
                    }
                    
                    // If odd number of quotes, we've closed the quote
                    if (quoteCount % 2 == 1) {
                        inQuotes = false;
                    }
                }
                
                // Add current line to accumulated value
                if (hasContinuation) {
                    // Remove trailing backslash
                    accumulated += currentLine.left(currentLine.length() - 1);
                } else {
                    accumulated += currentLine;
                }
                
                // Break if not continuing and not in quotes
                if (!hasContinuation && !inQuotes) {
                    break;
                }
                
                // Read next line
                if (!in.atEnd()) {
                    currentLine = in.readLine().trimmed();
                } else {
                    break;
                }
            }
            
            // Process accumulated value
            if (isAppend) {
                // Append to existing result
                if (!result.isEmpty()) {
                    result += QLatin1Char(' ');
                }
                result += accumulated;
            } else {
                // Replace result
                result = accumulated;
            }
        }
    }
    
    if (!foundVariable) {
        return QString();
    }
    
    // Remove quotes if present
    result = result.trimmed();
    if ((result.startsWith(QLatin1Char('"')) && result.endsWith(QLatin1Char('"'))) ||
        (result.startsWith(QLatin1Char('\'')) && result.endsWith(QLatin1Char('\''))) ) {
        result = result.mid(1, result.length() - 2);
    }
    
    qDebug() << "MakeConfReader: Read" << variableName << "=" << result;
    
    return result;
}
