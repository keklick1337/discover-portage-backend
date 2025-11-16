/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageAuthHelper.h"
#include <KAuth/HelperSupport>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <syslog.h>

using namespace KAuth;

PortageAuthHelper::PortageAuthHelper()
{
    syslog(LOG_INFO, "PortageAuthHelper: Initialized");
}

ActionReply PortageAuthHelper::execute(const QVariantMap &args)
{
    const QString action = args.value(QStringLiteral("action")).toString();
    syslog(LOG_INFO, "PortageAuthHelper: action=%s", qPrintable(action));
    
    // Route to appropriate handler
    if (action == QStringLiteral("emerge")) {
        return emergeExecute(args);
    } else if (action == QStringLiteral("file.write")) {
        return fileWrite(args);
    } else if (action == QStringLiteral("file.read")) {
        return fileRead(args);
    } else if (action == QStringLiteral("package.unmask")) {
        return packageUnmask(args);
    } else if (action == QStringLiteral("package.mask")) {
        return packageMask(args);
    } else if (action == QStringLiteral("package.use")) {
        return packageUse(args);
    } else if (action == QStringLiteral("package.license")) {
        return packageLicense(args);
    } else if (action == QStringLiteral("world.add")) {
        return worldAdd(args);
    } else if (action == QStringLiteral("world.remove")) {
        return worldRemove(args);
    }
    
    ActionReply reply = ActionReply::HelperErrorReply();
    reply.setErrorDescription(QStringLiteral("Unknown action: ") + action);
    return reply;
}

//=============================================================================
// Emerge Operations
//=============================================================================

ActionReply PortageAuthHelper::emergeExecute(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::emergeExecute called");
    
    const QStringList emergeArgs = args.value(QStringLiteral("args")).toStringList();
    const int timeout = args.value(QStringLiteral("timeout"), -1).toInt(); // -1 = no timeout
    
    if (emergeArgs.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("No emerge arguments provided"));
        return reply;
    }
    
    syslog(LOG_INFO, "emerge %s", qPrintable(emergeArgs.join(QLatin1Char(' '))));
    
    // Set up environment for emerge to work in non-interactive mode
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("dumb"));  // No fancy terminal features
    env.insert(QStringLiteral("PATH"), QStringLiteral("/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/opt/bin"));
    
    return runProcess(QStringLiteral("/usr/bin/emerge"), emergeArgs, timeout, env);
}

//=============================================================================
// File Operations
//=============================================================================

ActionReply PortageAuthHelper::fileWrite(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::fileWrite called");
    
    const QString path = args.value(QStringLiteral("path")).toString();
    const QString content = args.value(QStringLiteral("content")).toString();
    const bool append = args.value(QStringLiteral("append"), false).toBool();
    
    if (path.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("No file path provided"));
        return reply;
    }
    
    if (!validatePortagePath(path)) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Invalid path: must be under /etc/portage or /var/lib/portage"));
        return reply;
    }
    
    bool success = append ? appendToPortageFile(path, content) 
                          : writePortageFile(path, content);
    
    if (success) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("path"), path);
        reply.addData(QStringLiteral("bytes"), content.toUtf8().size());
        return reply;
    } else {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to write file: ") + path);
        return reply;
    }
}

ActionReply PortageAuthHelper::fileRead(const QVariantMap &args)
{
    const QString path = args.value(QStringLiteral("path")).toString();
    
    if (!validatePortagePath(path)) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Invalid path"));
        return reply;
    }
    
    QString content = readPortageFile(path);
    
    ActionReply reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("content"), content);
    reply.addData(QStringLiteral("path"), path);
    return reply;
}

//=============================================================================
// Package Configuration
//=============================================================================

ActionReply PortageAuthHelper::packageUnmask(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::packageUnmask called");
    
    const QString atom = args.value(QStringLiteral("atom")).toString();
    const QStringList keywords = args.value(QStringLiteral("keywords")).toStringList();
    
    if (atom.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("No package atom provided"));
        return reply;
    }
    
    // Default to package.accept_keywords
    QString filePath = QStringLiteral("/etc/portage/package.accept_keywords/discover");
    
    QString entry = atom;
    if (!keywords.isEmpty()) {
        entry += QStringLiteral(" ") + keywords.join(QLatin1Char(' '));
    } else {
        // Default to ~amd64 if no keywords specified
        entry += QStringLiteral(" ~amd64");
    }
    
    QString content = getFileHeader();
    content += entry + QStringLiteral("\n");
    
    if (appendToPortageFile(filePath, entry + QStringLiteral("\n"))) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("atom"), atom);
        reply.addData(QStringLiteral("file"), filePath);
        return reply;
    } else {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to unmask package"));
        return reply;
    }
}

ActionReply PortageAuthHelper::packageMask(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    const QString reason = args.value(QStringLiteral("reason")).toString();
    
    if (atom.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("No package atom provided"));
        return reply;
    }
    
    QString filePath = QStringLiteral("/etc/portage/package.mask/discover");
    QString entry;
    
    if (!reason.isEmpty()) {
        entry = QStringLiteral("# ") + reason + QStringLiteral("\n");
    }
    entry += atom + QStringLiteral("\n");
    
    if (appendToPortageFile(filePath, entry)) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("atom"), atom);
        return reply;
    } else {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to mask package"));
        return reply;
    }
}

ActionReply PortageAuthHelper::packageUse(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::packageUse called");
    
    const QString atom = args.value(QStringLiteral("atom")).toString();
    const QStringList useFlags = args.value(QStringLiteral("useFlags")).toStringList();
    
    if (atom.isEmpty() || useFlags.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Missing atom or USE flags"));
        return reply;
    }
    
    // Extract package name from atom (e.g., "net-misc/remmina" -> "remmina")
    QString packageName = atom;
    int slashIndex = atom.lastIndexOf(QLatin1Char('/'));
    if (slashIndex != -1) {
        packageName = atom.mid(slashIndex + 1);
    }
    
    // First, remove existing USE flag configurations from all files
    const QString packageUseDir = QStringLiteral("/etc/portage/package.use");
    QDir dir(packageUseDir);
    
    if (dir.exists()) {
        const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
        
        for (const QFileInfo &fileInfo : files) {
            QString filePath = fileInfo.absoluteFilePath();
            
            // Read file and remove lines with this atom
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            
            QStringList lines;
            QTextStream in(&file);
            while (!in.atEnd()) {
                QString line = in.readLine();
                QString trimmedLine = line.trimmed();
                
                // Keep the line if it doesn't contain the atom or is a comment
                if (trimmedLine.isEmpty() || trimmedLine.startsWith(QLatin1Char('#'))) {
                    lines << line;
                } else {
                    QStringList parts = trimmedLine.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                    if (parts.isEmpty() || parts.first() != atom) {
                        lines << line;
                    }
                    // else: skip this line (it's for the atom we want to remove)
                }
            }
            file.close();
            
            // Rewrite the file without the old entries
            if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
                syslog(LOG_WARNING, "Failed to rewrite file: %s", qPrintable(filePath));
                continue;
            }
            
            QTextStream out(&file);
            for (const QString &line : lines) {
                out << line << "\n";
            }
            file.close();
        }
    }
    
    // Now add the new configuration to discover_<packagename> file
    QString targetFile = packageUseDir + QStringLiteral("/discover_") + packageName;
    QString entry = atom + QStringLiteral(" ") + useFlags.join(QLatin1Char(' ')) + QStringLiteral("\n");
    
    if (appendToPortageFile(targetFile, entry)) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("atom"), atom);
        reply.addData(QStringLiteral("useFlags"), useFlags);
        return reply;
    } else {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to set USE flags"));
        return reply;
    }
}

ActionReply PortageAuthHelper::packageLicense(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    const QStringList licenses = args.value(QStringLiteral("licenses")).toStringList();
    
    if (atom.isEmpty() || licenses.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Missing atom or licenses"));
        return reply;
    }
    
    QString filePath = QStringLiteral("/etc/portage/package.license/discover");
    QString entry = atom + QStringLiteral(" ") + licenses.join(QLatin1Char(' ')) + QStringLiteral("\n");
    
    if (appendToPortageFile(filePath, entry)) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("atom"), atom);
        return reply;
    } else {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to accept license"));
        return reply;
    }
}

//=============================================================================
// World Set Management
//=============================================================================

ActionReply PortageAuthHelper::worldAdd(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    
    if (atom.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("No package atom provided"));
        return reply;
    }
    
    QString worldPath = QStringLiteral("/var/lib/portage/world");
    QString content = readPortageFile(worldPath);
    
    // Check if already in world
    QStringList entries = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (entries.contains(atom)) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("status"), QStringLiteral("already_exists"));
        return reply;
    }
    
    // Add to world
    if (appendToPortageFile(worldPath, atom + QStringLiteral("\n"))) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("atom"), atom);
        return reply;
    } else {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to add to world"));
        return reply;
    }
}

ActionReply PortageAuthHelper::worldRemove(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    
    if (atom.isEmpty()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("No package atom provided"));
        return reply;
    }
    
    QString worldPath = QStringLiteral("/var/lib/portage/world");
    QString content = readPortageFile(worldPath);
    
    QStringList entries = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    entries.removeAll(atom);
    
    QString newContent = entries.join(QLatin1Char('\n')) + QStringLiteral("\n");
    
    if (writePortageFile(worldPath, newContent)) {
        ActionReply reply = ActionReply::SuccessReply();
        reply.addData(QStringLiteral("atom"), atom);
        return reply;
    } else {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to remove from world"));
        return reply;
    }
}

//=============================================================================
// Helper Methods
//=============================================================================

ActionReply PortageAuthHelper::runProcess(const QString &program, 
                                         const QStringList &args,
                                         int timeoutMs,
                                         const QProcessEnvironment &env)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
    
    // Set environment if provided
    if (!env.isEmpty()) {
        process.setProcessEnvironment(env);
    }
    process.start(program, args);
    
    if (!process.waitForStarted()) {
        ActionReply reply = ActionReply::HelperErrorReply();
        reply.setErrorDescription(QStringLiteral("Failed to start: ") + program);
        return reply;
    }
    
    QByteArray outputBuffer;
    QByteArray errorBuffer;
    
    // Wait for process to finish while reading output
    bool finished = false;
    int progressCounter = 0;  // Counter to keep DBus connection alive
    while (!finished) {
        // Wait for process to finish or for data to be available
        if (timeoutMs > 0) {
            finished = process.waitForFinished(100);  // Short timeout to keep reading
        } else {
            finished = process.waitForFinished(100);
        }
        
        // Increment progress counter every 10 iterations (1 second)
        // This keeps DBus connection alive during long operations
        progressCounter++;
        if (progressCounter % 10 == 0) {
            HelperSupport::progressStep(0);  // Send keepalive signal to DBus
        }
        
        // Read all available output
        QByteArray output = process.readAllStandardOutput();
        QByteArray error = process.readAllStandardError();
        
        if (!output.isEmpty()) {
            outputBuffer += output;
            // Send progress data back to caller
            QVariantMap progressData;
            progressData[QStringLiteral("progress")] = QString::fromUtf8(output);
            HelperSupport::progressStep(progressData);
        }
        
        if (!error.isEmpty()) {
            errorBuffer += error;
        }
    }
    
    // Read any remaining output after process finished
    outputBuffer += process.readAllStandardOutput();
    errorBuffer += process.readAllStandardError();
    
    const int exitCode = process.exitCode();
    syslog(LOG_INFO, "%s finished with exit code %d", qPrintable(program), exitCode);
    
    // Always return success reply, but include exit code in data
    ActionReply reply = ActionReply::SuccessReply();
    reply.addData(QStringLiteral("output"), QString::fromUtf8(outputBuffer));
    reply.addData(QStringLiteral("error"), QString::fromUtf8(errorBuffer));
    reply.addData(QStringLiteral("exitCode"), exitCode);
    
    if (exitCode != 0) {
        reply.setErrorDescription(QStringLiteral("Process exited with code: ") + QString::number(exitCode));
    }
    
    return reply;
}

bool PortageAuthHelper::validatePortagePath(const QString &path)
{
    // Only allow access to /etc/portage and /var/lib/portage
    return path.startsWith(QStringLiteral("/etc/portage/")) ||
           path.startsWith(QStringLiteral("/var/lib/portage/"));
}

QString PortageAuthHelper::readPortageFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream in(&file);
    return in.readAll();
}

bool PortageAuthHelper::writePortageFile(const QString &path, const QString &content)
{
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            syslog(LOG_ERR, "Failed to create directory: %s", qPrintable(dir.path()));
            return false;
        }
    }
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        syslog(LOG_ERR, "Failed to open file for writing: %s", qPrintable(path));
        return false;
    }
    
    QTextStream out(&file);
    out << content;
    file.close();
    
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | 
                       QFile::ReadGroup | QFile::ReadOther);
    
    syslog(LOG_INFO, "Wrote file: %s", qPrintable(path));
    return true;
}

bool PortageAuthHelper::appendToPortageFile(const QString &path, const QString &content)
{
    QFileInfo fileInfo(path);
    QDir dir = fileInfo.dir();
    
    if (!dir.exists()) {
        if (!dir.mkpath(QStringLiteral("."))) {
            return false;
        }
    }
    
    // If file doesn't exist, add header
    bool needsHeader = !QFile::exists(path);
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return false;
    }
    
    QTextStream out(&file);
    if (needsHeader) {
        out << getFileHeader() << "\n\n";
    }
    out << content;
    file.close();
    
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner | 
                       QFile::ReadGroup | QFile::ReadOther);
    
    return true;
}

QString PortageAuthHelper::getFileHeader()
{
    return QStringLiteral("# Managed by Plasma Discover\n"
                         "# Generated on %1")
           .arg(QDateTime::currentDateTime().toString(Qt::ISODate));
}

KAUTH_HELPER_MAIN("org.kde.discover.portagebackend", PortageAuthHelper)

#include "moc_PortageAuthHelper.cpp"
