/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <KAuth/ActionReply>

using namespace KAuth;

class PortageAuthHelper : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.discover.portagebackend")
    
public:
    PortageAuthHelper();

public Q_SLOTS:

    ActionReply execute(const QVariantMap &args);

private:

    ActionReply emergeExecute(const QVariantMap &args);

    ActionReply fileWrite(const QVariantMap &args);
    ActionReply fileRead(const QVariantMap &args);

    ActionReply packageUnmask(const QVariantMap &args);
    ActionReply packageMask(const QVariantMap &args);
    ActionReply packageUse(const QVariantMap &args);
    ActionReply packageLicense(const QVariantMap &args);

    ActionReply worldAdd(const QVariantMap &args);
    ActionReply worldRemove(const QVariantMap &args);
    
    // Repository management
    ActionReply repositoryEnable(const QVariantMap &args);
    ActionReply repositoryDisable(const QVariantMap &args);
    ActionReply repositoryRemove(const QVariantMap &args);
    ActionReply repositoryAdd(const QVariantMap &args);
    ActionReply repositorySync(const QVariantMap &args);

    ActionReply runProcess(const QString &program, const QStringList &args, 
                          int timeoutMs = -1,  // -1 = no timeout (unlimited)
                          const QProcessEnvironment &env = QProcessEnvironment());
    bool validatePortagePath(const QString &path);
    QString readPortageFile(const QString &path);
    bool writePortageFile(const QString &path, const QString &content);
    bool appendToPortageFile(const QString &path, const QString &content);
    QString getFileHeader();
    
    // Helper methods to reduce boilerplate
    static ActionReply errorReply(const QString &message);
    static ActionReply successReply(const QVariantMap &data = QVariantMap());
    
    // Package.use specific helpers
    void removeAtomFromFile(const QString &filePath, const QString &atom);
    bool removeAtomFromAllFiles(const QString &packageUseDir, const QString &atom);
};
