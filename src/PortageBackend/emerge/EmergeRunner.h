/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>

class EmergeRunner : public QObject
{
    Q_OBJECT

public:
    enum EmergeAction {
        Pretend,      // --pretend -v (check dependencies)
        Install,      // Install package
        Uninstall     // -C (uninstall)
    };

    struct DependencyInfo {
        QString atom;
        QString version;
        bool isMasked;
        QString maskReason;
        QStringList keywords; // ~amd64, etc
        QStringList useFlags;
    };

    struct EmergeResult {
        bool success;
        int exitCode;
        QString output;
        QString error;
        QList<DependencyInfo> dependencies;
        bool needsUnmask;
        QStringList maskedPackages;
    };

    explicit EmergeRunner(QObject *parent = nullptr);
    ~EmergeRunner() override;

    // Run emerge --pretend to check what would be installed
    void checkDependencies(const QString &atom);
    
    // Install package (requires sudo)
    void installPackage(const QString &atom, const QStringList &useFlags = QStringList());
    
    // Uninstall package (requires sudo)
    void uninstallPackage(const QString &atom);
    
    // Cancel current operation
    void cancel();

Q_SIGNALS:
    void dependenciesChecked(const EmergeResult &result);
    void outputReceived(const QString &line);
    void errorReceived(const QString &line);
    void processFinished(bool success, int exitCode);
    void progressChanged(int percent, const QString &message);

private Q_SLOTS:
    void onProcessReadyRead();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    EmergeResult parsePretendOutput(const QString &output);
    bool isPackageMasked(const QString &line);
    QString extractMaskReason(const QString &output, const QString &atom);
    
    QProcess *m_process;
    EmergeAction m_currentAction;
    QString m_currentAtom;
    QString m_outputBuffer;
    QString m_errorBuffer;
};
