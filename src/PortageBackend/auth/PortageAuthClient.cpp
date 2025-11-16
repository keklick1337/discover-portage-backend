/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageAuthClient.h"
#include <KAuth/Action>
#include <KAuth/ExecuteJob>
#include <QDebug>

PortageAuthClient::PortageAuthClient(QObject *parent)
    : QObject(parent)
{
}

PortageAuthClient::~PortageAuthClient() = default;

void PortageAuthClient::emergeExecute(const QStringList &args,
                                     ResultCallback callback,
                                     ProgressCallback progress,
                                     int timeoutMs)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("emerge");
    arguments[QStringLiteral("args")] = args;
    arguments[QStringLiteral("timeout")] = timeoutMs;
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, progress);
}

void PortageAuthClient::emergeInstall(const QString &atom,
                                     ResultCallback callback,
                                     ProgressCallback progress)
{
    QStringList args;
    args << QStringLiteral("--verbose")
         << QStringLiteral("--color=n")
         << atom;
    
    emergeExecute(args, callback, progress);
}

void PortageAuthClient::emergeRemove(const QString &atom,
                                    ResultCallback callback,
                                    ProgressCallback progress)
{
    QStringList args;
    args << QStringLiteral("--unmerge")
         << QStringLiteral("--verbose")
         << QStringLiteral("--color=n")
         << atom;
    
    emergeExecute(args, callback, progress);
}

void PortageAuthClient::emergeSync(ResultCallback callback,
                                  ProgressCallback progress)
{
    QStringList args;
    args << QStringLiteral("--sync");
    
    emergeExecute(args, callback, progress, -1);
}

void PortageAuthClient::writeFile(const QString &path,
                                 const QString &content,
                                 bool append,
                                 ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("file.write");
    arguments[QStringLiteral("path")] = path;
    arguments[QStringLiteral("content")] = content;
    arguments[QStringLiteral("append")] = append;
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::readFile(const QString &path,
                                ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("file.read");
    arguments[QStringLiteral("path")] = path;
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::unmaskPackage(const QString &atom,
                                     const QStringList &keywords,
                                     ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("package.unmask");
    arguments[QStringLiteral("atom")] = atom;
    if (!keywords.isEmpty()) {
        arguments[QStringLiteral("keywords")] = keywords;
    }
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::maskPackage(const QString &atom,
                                   const QString &reason,
                                   ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("package.mask");
    arguments[QStringLiteral("atom")] = atom;
    if (!reason.isEmpty()) {
        arguments[QStringLiteral("reason")] = reason;
    }
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::setUseFlags(const QString &atom,
                                   const QStringList &useFlags,
                                   ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("package.use");
    arguments[QStringLiteral("atom")] = atom;
    arguments[QStringLiteral("useFlags")] = useFlags;
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::acceptLicense(const QString &atom,
                                     const QStringList &licenses,
                                     ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("package.license");
    arguments[QStringLiteral("atom")] = atom;
    arguments[QStringLiteral("licenses")] = licenses;
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::addToWorld(const QString &atom,
                                  ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("world.add");
    arguments[QStringLiteral("atom")] = atom;
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::removeFromWorld(const QString &atom,
                                       ResultCallback callback)
{
    QVariantMap arguments;
    arguments[QStringLiteral("action")] = QStringLiteral("world.remove");
    arguments[QStringLiteral("atom")] = atom;
    
    executeAction(QStringLiteral("org.kde.discover.portagebackend.execute"),
                 arguments, callback, nullptr);
}

void PortageAuthClient::executeAction(const QString &actionName,
                                     const QVariantMap &args,
                                     ResultCallback callback,
                                     ProgressCallback progress)
{
    qDebug() << "PortageAuthClient: Executing action" << actionName;
    
    Q_EMIT operationStarted(actionName);
    
    KAuth::Action action(actionName);
    action.setHelperId(QStringLiteral("org.kde.discover.portagebackend"));
    action.setArguments(args);
    
    KAuth::ExecuteJob *job = action.execute();
    
    if (!job) {
        qWarning() << "PortageAuthClient: Failed to create execute job";
        if (callback) {
            callback(false, QString(), QStringLiteral("Failed to create KAuth job"));
        }
        Q_EMIT operationFinished(actionName, false);
        return;
    }
    
    // Handle progress updates
    if (progress) {
        connect(job, &KAuth::ExecuteJob::percentChanged, this,
               [progress](KJob *, unsigned long percent) {
            progress(static_cast<int>(percent), QString());
        });
        
        // Also handle progress data from helper
        connect(job, &KAuth::ExecuteJob::newData, this,
               [progress](const QVariantMap &data) {
            if (data.contains(QStringLiteral("progress"))) {
                QString message = data.value(QStringLiteral("progress")).toString();
                progress(-1, message);
            }
        });
    }
    
    // Handle completion
    connect(job, &KAuth::ExecuteJob::result, this,
           [this, callback, actionName](KJob *kjob) {
        handleJobResult(static_cast<KAuth::ExecuteJob *>(kjob), callback, actionName);
    });
    
    job->start();
}

void PortageAuthClient::handleJobResult(KAuth::ExecuteJob *job,
                                       ResultCallback callback,
                                       const QString &actionName)
{
    bool success = (job->error() == 0);
    QString output;
    QString error;
    
    if (success) {
        output = job->data().value(QStringLiteral("output")).toString();
        qDebug() << "PortageAuthClient: Action succeeded:" << actionName;
    } else {
        error = job->errorString();
        output = job->data().value(QStringLiteral("output")).toString();
        QString errOutput = job->data().value(QStringLiteral("error")).toString();
        
        if (!errOutput.isEmpty()) {
            error += QStringLiteral("\n") + errOutput;
        }
        
        qWarning() << "PortageAuthClient: Action failed:" << actionName << error;
    }
    
    Q_EMIT operationFinished(actionName, success);
    
    if (callback) {
        callback(success, output, error);
    }
}

#include "moc_PortageAuthClient.cpp"
