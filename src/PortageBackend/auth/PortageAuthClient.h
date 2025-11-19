/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

namespace KAuth {
class ExecuteJob;
}

class PortageAuthClient : public QObject
{
    Q_OBJECT

public:
    explicit PortageAuthClient(QObject *parent = nullptr);
    ~PortageAuthClient() override;

    using ResultCallback = std::function<void(bool success, const QString &output, const QString &error)>;
    using ProgressCallback = std::function<void(int percent, const QString &message)>;

    void emergeExecute(const QStringList &args,
                      ResultCallback callback = nullptr,
                      ProgressCallback progress = nullptr,
                      int timeoutMs = -1);  // -1 = no timeout

    void emergeInstall(const QString &atom,
                      ResultCallback callback = nullptr,
                      ProgressCallback progress = nullptr);

    void emergeRemove(const QString &atom,
                     ResultCallback callback = nullptr,
                     ProgressCallback progress = nullptr);

    void emergeSync(ResultCallback callback = nullptr,
                   ProgressCallback progress = nullptr);

    void writeFile(const QString &path,
                  const QString &content,
                  bool append = false,
                  ResultCallback callback = nullptr);

    void readFile(const QString &path,
                 ResultCallback callback = nullptr);

    void unmaskPackage(const QString &atom,
                      const QStringList &keywords = QStringList(),
                      ResultCallback callback = nullptr);

    void maskPackage(const QString &atom,
                    const QString &reason = QString(),
                    ResultCallback callback = nullptr);

    void setUseFlags(const QString &atom,
                    const QStringList &useFlags,
                    ResultCallback callback = nullptr);

    void acceptLicense(const QString &atom,
                      const QStringList &licenses,
                      ResultCallback callback = nullptr);

    void addToWorld(const QString &atom,
                   ResultCallback callback = nullptr);

    void removeFromWorld(const QString &atom,
                        ResultCallback callback = nullptr);

    void repositoryEnable(const QString &name,
                         ResultCallback callback = nullptr);

    void repositoryDisable(const QString &name,
                          ResultCallback callback = nullptr);

    void repositoryRemove(const QString &name,
                         ResultCallback callback = nullptr);

    void repositoryAdd(const QString &name,
                      const QString &syncType,
                      const QString &syncUri,
                      ResultCallback callback = nullptr);

    void repositorySync(const QString &repository,
                       bool runEixUpdate = true,
                       ResultCallback callback = nullptr,
                       ProgressCallback progress = nullptr);

Q_SIGNALS:
    void operationStarted(const QString &action);

    void operationFinished(const QString &action, bool success);

private:
    void executeAction(const QString &actionName,
                      const QVariantMap &args,
                      ResultCallback callback,
                      ProgressCallback progress);
    
    void handleJobResult(KAuth::ExecuteJob *job,
                        ResultCallback callback,
                        const QString &actionName);
};
