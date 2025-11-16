/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "EmergeRunner.h"
#include <QDebug>
#include <QRegularExpression>
#include <QTimer>
#include <KAuth/Action>
#include <KAuth/ExecuteJob>

EmergeRunner::EmergeRunner(QObject *parent)
    : QObject(parent)
    , m_process(nullptr)
    , m_currentAction(Pretend)
{
}

EmergeRunner::~EmergeRunner()
{
    if (m_process) {
        m_process->kill();
        m_process->waitForFinished();
        delete m_process;
    }
}

void EmergeRunner::checkDependencies(const QString &atom)
{
    if (m_process && m_process->state() == QProcess::Running) {
        qWarning() << "EmergeRunner: Process already running";
        return;
    }

    m_currentAction = Pretend;
    m_currentAtom = atom;
    m_outputBuffer.clear();
    m_errorBuffer.clear();

    // Use KAuth for pretend check to request password upfront
    KAuth::Action pretendAction(QStringLiteral("org.kde.discover.portagebackend.execute"));
    pretendAction.setHelperId(QStringLiteral("org.kde.discover.portagebackend"));
    
    QVariantMap args;
    args[QStringLiteral("action")] = QStringLiteral("emerge");
    
    QStringList emergeArgs;
    emergeArgs << QStringLiteral("--pretend")
               << QStringLiteral("--verbose")
               << QStringLiteral("--tree")
               << QStringLiteral("--autounmask")
               << QStringLiteral("--autounmask-write=n")
               << QStringLiteral("--color=n")
               << atom;

    args[QStringLiteral("args")] = emergeArgs;
    pretendAction.setArguments(args);
    pretendAction.setTimeout(-1);
    
    qDebug() << "EmergeRunner: Checking dependencies via KAuth:" << emergeArgs.join(QLatin1Char(' '));
    KAuth::ExecuteJob *job = pretendAction.execute();
    
    connect(job, &KAuth::ExecuteJob::result, this, [this](KJob *kjob) {
        KAuth::ExecuteJob *authJob = static_cast<KAuth::ExecuteJob *>(kjob);
        
        QString output = authJob->data().value(QStringLiteral("output")).toString();
        QString error = authJob->data().value(QStringLiteral("error")).toString();
        int exitCode = authJob->data().value(QStringLiteral("exitCode"), authJob->error()).toInt();
        
        m_outputBuffer = output;
        m_errorBuffer = error;
        
        qDebug() << "EmergeRunner: Pretend check completed with exit code" << exitCode;
        qDebug() << "EmergeRunner: Output length:" << output.length() << "Error length:" << error.length();
        if (!output.isEmpty()) {
            qDebug() << "EmergeRunner: Output preview:" << output.left(500);
        }
        if (!error.isEmpty()) {
            qDebug() << "EmergeRunner: Error preview:" << error.left(500);
        }
        
        // Parse output even if exit code is non-zero (unmask needed)
        EmergeResult result = parsePretendOutput(output + error);
        result.exitCode = exitCode;
        result.success = (exitCode == 0);
        
        qDebug() << "EmergeRunner: Parsed result - needsUnmask:" << result.needsUnmask 
                 << "maskedPackages:" << result.maskedPackages.size();
        
        Q_EMIT dependenciesChecked(result);
    });
    
    job->start();
}

void EmergeRunner::installPackage(const QString &atom, const QStringList &useFlags)
{
    if (m_process && m_process->state() == QProcess::Running) {
        qWarning() << "EmergeRunner: Process already running";
        return;
    }

    m_currentAction = Install;
    m_currentAtom = atom;
    m_outputBuffer.clear();
    m_errorBuffer.clear();

    qDebug() << "EmergeRunner: Installing package via KAuth:" << atom;
    if (!useFlags.isEmpty()) {
        qDebug() << "EmergeRunner: Installing with USE flags:" << useFlags;
    }


    KAuth::Action installAction(QStringLiteral("org.kde.discover.portagebackend.execute"));
    installAction.setHelperId(QStringLiteral("org.kde.discover.portagebackend"));
    
    QVariantMap args;
    args[QStringLiteral("action")] = QStringLiteral("emerge");
    
    QStringList emergeArgs;
    emergeArgs << QStringLiteral("--verbose")
               << QStringLiteral("--noreplace")
               << QStringLiteral("--newuse");
    
    if (!useFlags.isEmpty()) {
        // TODO: Set USE flags via package.use before emerge
        qWarning() << "EmergeRunner: USE flags not yet implemented in new API";
    }
    
    emergeArgs << atom;
    args[QStringLiteral("args")] = emergeArgs;

    installAction.setArguments(args);
    installAction.setTimeout(-1);
    
    qDebug() << "EmergeRunner: Executing KAuth action for installation";
    KAuth::ExecuteJob *job = installAction.execute();
    
    connect(job, &KAuth::ExecuteJob::result, this, [this](KJob *kjob) {
        KAuth::ExecuteJob *authJob = static_cast<KAuth::ExecuteJob *>(kjob);
        
        QString output = authJob->data().value(QStringLiteral("output")).toString();
        QString error = authJob->data().value(QStringLiteral("error")).toString();
        int exitCode = authJob->data().value(QStringLiteral("exitCode"), 1).toInt();
        
        m_outputBuffer = output;
        m_errorBuffer = error;
        
        if (exitCode == 0) {
            qDebug() << "EmergeRunner: Installation completed successfully";
            Q_EMIT processFinished(true, 0);
        } else {
            qWarning() << "EmergeRunner: Installation failed with exit code:" << exitCode;
            qWarning() << "EmergeRunner: Error output:" << error;
            Q_EMIT processFinished(false, exitCode);
        }
    });
    
    // Handle progress updates
    connect(job, &KAuth::ExecuteJob::percentChanged, this, [this](KJob *, unsigned long percent) {
        Q_EMIT progressChanged(static_cast<int>(percent), QStringLiteral("Installing..."));
    });
    
    job->start();
}

void EmergeRunner::uninstallPackage(const QString &atom)
{
    if (m_process && m_process->state() == QProcess::Running) {
        qWarning() << "EmergeRunner: Process already running";
        return;
    }

    m_currentAction = Uninstall;
    m_currentAtom = atom;
    m_outputBuffer.clear();
    m_errorBuffer.clear();

    qDebug() << "EmergeRunner: Uninstalling package via KAuth:" << atom;

    KAuth::Action removeAction(QStringLiteral("org.kde.discover.portagebackend.execute"));
    removeAction.setHelperId(QStringLiteral("org.kde.discover.portagebackend"));
    
    QVariantMap args;
    args[QStringLiteral("action")] = QStringLiteral("emerge");
    
    QStringList emergeArgs;
    emergeArgs << QStringLiteral("--verbose")
               << QStringLiteral("--rage-clean")
               << atom;
    
    args[QStringLiteral("args")] = emergeArgs;
    args[QStringLiteral("timeout")] = -1;
    
    removeAction.setArguments(args);
    removeAction.setTimeout(-1);
    
    qDebug() << "EmergeRunner: Executing KAuth action for removal with --rage-clean";
    KAuth::ExecuteJob *job = removeAction.execute();
    
    connect(job, &KAuth::ExecuteJob::result, this, [this](KJob *kjob) {
        KAuth::ExecuteJob *authJob = static_cast<KAuth::ExecuteJob *>(kjob);
        
        QString output = authJob->data().value(QStringLiteral("output")).toString();
        QString error = authJob->data().value(QStringLiteral("error")).toString();
        int exitCode = authJob->data().value(QStringLiteral("exitCode"), 1).toInt();
        
        m_outputBuffer = output;
        m_errorBuffer = error;
        
        if (exitCode == 0) {
            qDebug() << "EmergeRunner: Removal completed successfully";
            Q_EMIT processFinished(true, 0);
        } else {
            qWarning() << "EmergeRunner: Removal failed with exit code:" << exitCode;
            qWarning() << "EmergeRunner: Error output:" << error;
            Q_EMIT processFinished(false, exitCode);
        }
    });
    
    // Handle progress updates
    connect(job, &KAuth::ExecuteJob::percentChanged, this, [this](KJob *, unsigned long percent) {
        Q_EMIT progressChanged(static_cast<int>(percent), QStringLiteral("Removing..."));
    });
    
    job->start();
}

void EmergeRunner::cancel()
{
    if (m_process && m_process->state() == QProcess::Running) {
        qDebug() << "EmergeRunner: Cancelling process";
        m_process->terminate();
        
        QTimer::singleShot(5000, this, [this]() {
            if (m_process && m_process->state() == QProcess::Running) {
                m_process->kill();
            }
        });
    }
}

void EmergeRunner::onProcessReadyRead()
{
    if (!m_process) return;

    QString stdoutText = QString::fromUtf8(m_process->readAllStandardOutput());
    QString stderrText = QString::fromUtf8(m_process->readAllStandardError());

    if (!stdoutText.isEmpty()) {
        m_outputBuffer += stdoutText;
        
        const QStringList lines = stdoutText.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (!line.trimmed().isEmpty()) {
                Q_EMIT outputReceived(line);
            }
        }
    }

    if (!stderrText.isEmpty()) {
        m_errorBuffer += stderrText;
        
        const QStringList lines = stderrText.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (!line.trimmed().isEmpty()) {
                Q_EMIT errorReceived(line);
            }
        }
    }
}

void EmergeRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "EmergeRunner: Process finished with exit code" << exitCode;

    bool success = (exitCode == 0 && exitStatus == QProcess::NormalExit);

    if (m_currentAction == Pretend) {
        EmergeResult result = parsePretendOutput(m_outputBuffer + m_errorBuffer);
        result.exitCode = exitCode;
        result.success = success;
        Q_EMIT dependenciesChecked(result);
    }

    Q_EMIT processFinished(success, exitCode);
}

void EmergeRunner::onProcessError(QProcess::ProcessError error)
{
    qWarning() << "EmergeRunner: Process error" << error;
    Q_EMIT errorReceived(QStringLiteral("Process error: %1").arg(error));
}

EmergeRunner::EmergeResult EmergeRunner::parsePretendOutput(const QString &output)
{
    EmergeResult result;
    result.success = false;
    result.needsUnmask = false;
    result.output = output;

    if (output.contains(QStringLiteral("The following keyword changes are necessary")) ||
        output.contains(QStringLiteral("The following mask changes are necessary"))) {
        result.needsUnmask = true;
        
        QStringList lines = output.split(QLatin1Char('\n'));
        for (const QString &line : lines) {
            if (line.contains(QStringLiteral("="))) {
                QString trimmed = line.trimmed();
                if (trimmed.startsWith(QLatin1Char('='))) {
                    result.maskedPackages << trimmed;
                }
            }
        }
    }

    // Walk the output line-by-line. Two important formats to handle:
    // 1) Masked candidates list that is shown as:
    //    - category/package-version::repo (masked by: ~arch keyword)
    // 2) Ebuild processing lines that include the ebuild indicator and may contain USE="..."
    QStringList lines = output.split(QLatin1Char('\n'));

    QRegularExpression maskedLineRe(QStringLiteral(R"(^\s*-\s*([^\s]+).*\(masked by:\s*([^\)]+)\))"));
    QRegularExpression ebuildLineRe(QStringLiteral(R"(^\s*\[ebuild[^\]]*\]\s+([^\s]+)(.*)$)"));
    QRegularExpression useRe(QStringLiteral(R"((?:USE|USE_FLAGS)=['\"]([^'\"]*)['\"])"));
    QRegularExpression versionRe(QStringLiteral(R"(^(.+?)-([\d].*)$)"));

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty())
            continue;

        // masked package list entries (from the user example)
        QRegularExpressionMatch m = maskedLineRe.match(trimmed);
        if (m.hasMatch()) {
            QString atom = m.captured(1);
            QString reason = m.captured(2).trimmed();
            result.needsUnmask = true;
            result.maskedPackages << QStringLiteral("%1 (%2)").arg(atom, reason);
            continue;
        }

        // ebuild-line: capture atom and remainder
        QRegularExpressionMatch eMatch = ebuildLineRe.match(line);
        if (eMatch.hasMatch()) {
            DependencyInfo dep;
            QString fullAtom = eMatch.captured(1);
            QString rest = eMatch.captured(2);

            // Remove ::repo suffix if present (e.g., www-client/google-chrome-beta-143.0.7499.4::gentoo)
            int repoSep = fullAtom.indexOf(QStringLiteral("::"));
            if (repoSep > 0) {
                fullAtom = fullAtom.left(repoSep);
            }
            
            // Add = prefix for exact version matching
            if (!fullAtom.startsWith(QLatin1Char('='))) {
                fullAtom = QStringLiteral("=") + fullAtom;
            }

            dep.atom = fullAtom;
            QRegularExpressionMatch vMatch = versionRe.match(fullAtom);
            if (vMatch.hasMatch()) {
                dep.version = vMatch.captured(2);
            }

            QRegularExpressionMatch useMatch = useRe.match(rest);
            if (useMatch.hasMatch()) {
                QString useString = useMatch.captured(1);
                QStringList useParts = useString.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                for (const QString &flag : useParts) {
                    if (!flag.isEmpty())
                        dep.useFlags << flag;
                }
            }

            result.dependencies << dep;
        }
    }

    result.success = true;
    return result;
}

bool EmergeRunner::isPackageMasked(const QString &line)
{
    return line.contains(QStringLiteral("masked by:")) ||
           line.contains(QStringLiteral("keyword")) ||
           line.contains(QStringLiteral("~"));
}

QString EmergeRunner::extractMaskReason(const QString &output, const QString &atom)
{
    qDebug() << "EmergeRunner: Extracting mask reason for" << atom;
    
    QString reason;
    
    if (output.contains(QStringLiteral("keyword"))) {
        reason = QStringLiteral("Package needs keyword unmasking (~amd64)");
    } else if (output.contains(QStringLiteral("masked by:"))) {
        reason = QStringLiteral("Package is hard masked");
    }
    
    return reason;
}
