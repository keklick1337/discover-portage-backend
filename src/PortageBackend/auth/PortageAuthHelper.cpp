/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageAuthHelper.h"
#include "../utils/AtomParser.h"
#include "../utils/StringUtils.h"
#include "../utils/PortagePaths.h"
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
    } else if (action == QStringLiteral("repository.enable")) {
        return repositoryEnable(args);
    } else if (action == QStringLiteral("repository.disable")) {
        return repositoryDisable(args);
    } else if (action == QStringLiteral("repository.remove")) {
        return repositoryRemove(args);
    } else if (action == QStringLiteral("repository.add")) {
        return repositoryAdd(args);
    } else if (action == QStringLiteral("repository.sync")) {
        return repositorySync(args);
    }
    
    return errorReply(QStringLiteral("Unknown action: ") + action);
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
        return errorReply(QStringLiteral("No emerge arguments provided"));
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
        return errorReply(QStringLiteral("No file path provided"));
    }
    
    if (!validatePortagePath(path)) {
        return errorReply(QStringLiteral("Invalid path: must be under /etc/portage or /var/lib/portage"));
    }
    
    bool success = append ? appendToPortageFile(path, content) 
                          : writePortageFile(path, content);
    
    if (success) {
        return successReply({{QStringLiteral("path"), path},
                            {QStringLiteral("bytes"), content.toUtf8().size()}});
    } else {
        return errorReply(QStringLiteral("Failed to write file: ") + path);
    }
}

ActionReply PortageAuthHelper::fileRead(const QVariantMap &args)
{
    const QString path = args.value(QStringLiteral("path")).toString();
    
    if (!validatePortagePath(path)) {
        return errorReply(QStringLiteral("Invalid path"));
    }
    
    QString content = readPortageFile(path);
    
    return successReply({{QStringLiteral("content"), content},
                        {QStringLiteral("path"), path}});
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
        return errorReply(QStringLiteral("No package atom provided"));
    }
    
    // Default to package.accept_keywords
    QString filePath = QString::fromLatin1(PortagePaths::PACKAGE_ACCEPT_KEYWORDS) + QStringLiteral("/discover");
    
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
        return successReply({{QStringLiteral("atom"), atom},
                            {QStringLiteral("file"), filePath}});
    } else {
        return errorReply(QStringLiteral("Failed to unmask package"));
    }
}

ActionReply PortageAuthHelper::packageMask(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    const QString reason = args.value(QStringLiteral("reason")).toString();
    
    if (atom.isEmpty()) {
        return errorReply(QStringLiteral("No package atom provided"));
    }
    
    QString filePath = QString::fromLatin1(PortagePaths::PACKAGE_MASK) + QStringLiteral("/discover");
    QString entry;
    
    if (!reason.isEmpty()) {
        entry = QStringLiteral("# ") + reason + QStringLiteral("\n");
    }
    entry += atom + QStringLiteral("\n");
    
    if (appendToPortageFile(filePath, entry)) {
        return successReply({{QStringLiteral("atom"), atom}});
    } else {
        return errorReply(QStringLiteral("Failed to mask package"));
    }
}

ActionReply PortageAuthHelper::packageUse(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::packageUse called");
    
    const QString atom = args.value(QStringLiteral("atom")).toString();
    const QStringList useFlags = args.value(QStringLiteral("useFlags")).toStringList();
    
    if (atom.isEmpty() || useFlags.isEmpty()) {
        return errorReply(QStringLiteral("Missing atom or USE flags"));
    }
    
    // Extract package name from atom using AtomParser
    QString packageName = AtomParser::extractPackageNameForFile(atom);
    
    // First, remove existing USE flag configurations from all files
    removeAtomFromAllFiles(QString::fromLatin1(PortagePaths::PACKAGE_USE), atom);
    
    // Now add the new configuration to discover_<packagename> file
    QString targetFile = QString::fromLatin1(PortagePaths::PACKAGE_USE) + QStringLiteral("/discover_") + packageName;
    QString entry = atom + QStringLiteral(" ") + useFlags.join(QLatin1Char(' ')) + QStringLiteral("\n");
    
    if (appendToPortageFile(targetFile, entry)) {
        return successReply({{QStringLiteral("atom"), atom}, 
                            {QStringLiteral("useFlags"), useFlags}});
    } else {
        return errorReply(QStringLiteral("Failed to set USE flags"));
    }
}

ActionReply PortageAuthHelper::packageLicense(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    const QStringList licenses = args.value(QStringLiteral("licenses")).toStringList();
    
    if (atom.isEmpty() || licenses.isEmpty()) {
        return errorReply(QStringLiteral("Missing atom or licenses"));
    }
    
    QString filePath = QString::fromLatin1(PortagePaths::PACKAGE_LICENSE) + QStringLiteral("/discover");
    QString entry = atom + QStringLiteral(" ") + licenses.join(QLatin1Char(' ')) + QStringLiteral("\n");
    
    if (appendToPortageFile(filePath, entry)) {
        return successReply({{QStringLiteral("atom"), atom}});
    } else {
        return errorReply(QStringLiteral("Failed to accept license"));
    }
}

//=============================================================================
// World Set Management
//=============================================================================

ActionReply PortageAuthHelper::worldAdd(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    
    if (atom.isEmpty()) {
        return errorReply(QStringLiteral("No package atom provided"));
    }
    
    QString worldPath = QLatin1String(PortagePaths::WORLD_FILE);
    QString content = readPortageFile(worldPath);
    
    // Check if already in world
    QStringList entries = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (entries.contains(atom)) {
        return successReply({{QStringLiteral("status"), QStringLiteral("already_exists")}});
    }
    
    // Add to world
    if (appendToPortageFile(worldPath, atom + QStringLiteral("\n"))) {
        return successReply({{QStringLiteral("atom"), atom}});
    } else {
        return errorReply(QStringLiteral("Failed to add to world"));
    }
}

ActionReply PortageAuthHelper::worldRemove(const QVariantMap &args)
{
    const QString atom = args.value(QStringLiteral("atom")).toString();
    
    if (atom.isEmpty()) {
        return errorReply(QStringLiteral("No package atom provided"));
    }
    
    QString worldPath = QLatin1String(PortagePaths::WORLD_FILE);
    QString content = readPortageFile(worldPath);
    
    QStringList entries = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    entries.removeAll(atom);
    
    QString newContent = entries.join(QLatin1Char('\n')) + QStringLiteral("\n");
    
    if (writePortageFile(worldPath, newContent)) {
        return successReply({{QStringLiteral("atom"), atom}});
    } else {
        return errorReply(QStringLiteral("Failed to remove from world"));
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
        return errorReply(QStringLiteral("Failed to start: ") + program);
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
    QVariantMap resultData = {
        {QStringLiteral("output"), QString::fromUtf8(outputBuffer)},
        {QStringLiteral("error"), QString::fromUtf8(errorBuffer)},
        {QStringLiteral("exitCode"), exitCode}
    };
    
    ActionReply reply = successReply(resultData);
    if (exitCode != 0) {
        reply.setErrorDescription(QStringLiteral("Process exited with code: ") + QString::number(exitCode));
    }
    
    return reply;
}

bool PortageAuthHelper::validatePortagePath(const QString &path)
{
    // Only allow access to /etc/portage and /var/lib/portage
    return path.startsWith(QLatin1String(PortagePaths::ETC_PORTAGE) + QLatin1Char('/')) ||
           path.startsWith(QLatin1String(PortagePaths::VAR_LIB_PORTAGE) + QLatin1Char('/'));
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

ActionReply PortageAuthHelper::errorReply(const QString &message)
{
    ActionReply reply = ActionReply::HelperErrorReply();
    reply.setErrorDescription(message);
    return reply;
}

ActionReply PortageAuthHelper::successReply(const QVariantMap &data)
{
    ActionReply reply = ActionReply::SuccessReply();
    for (auto it = data.begin(); it != data.end(); ++it) {
        reply.addData(it.key(), it.value());
    }
    return reply;
}

void PortageAuthHelper::removeAtomFromFile(const QString &filePath, const QString &atom)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QStringList lines;
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QString trimmedLine = line.trimmed();
        
        // Keep the line if it doesn't contain the atom or is a comment
        if (StringUtils::isCommentOrEmptyTrimmed(trimmedLine)) {
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
        return;
    }
    
    QTextStream out(&file);
    for (const QString &line : lines) {
        out << line << "\n";
    }
    file.close();
}

bool PortageAuthHelper::removeAtomFromAllFiles(const QString &packageUseDir, const QString &atom)
{
    QDir dir(packageUseDir);
    if (!dir.exists()) {
        return true; // Nothing to remove
    }
    
    const QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fileInfo : files) {
        removeAtomFromFile(fileInfo.absoluteFilePath(), atom);
    }
    
    return true;
}

//=============================================================================
// Repository Management Operations
//=============================================================================

ActionReply PortageAuthHelper::repositoryEnable(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::repositoryEnable called");
    
    const QString repoName = args.value(QStringLiteral("name")).toString();
    
    if (repoName.isEmpty()) {
        return errorReply(QStringLiteral("Repository name is required"));
    }
    
    syslog(LOG_INFO, "eselect repository enable %s", qPrintable(repoName));
    
    QStringList cmdArgs;
    cmdArgs << QStringLiteral("repository") << QStringLiteral("enable") << repoName;
    
    return runProcess(QStringLiteral("/usr/bin/eselect"), cmdArgs, 30000);
}

ActionReply PortageAuthHelper::repositoryDisable(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::repositoryDisable called");
    
    const QString repoName = args.value(QStringLiteral("name")).toString();
    
    if (repoName.isEmpty()) {
        return errorReply(QStringLiteral("Repository name is required"));
    }
    
    // Don't allow disabling the main gentoo repository
    if (repoName == QStringLiteral("gentoo")) {
        return errorReply(QStringLiteral("Cannot disable the main Gentoo repository"));
    }
    
    syslog(LOG_INFO, "eselect repository disable %s", qPrintable(repoName));
    
    QStringList cmdArgs;
    cmdArgs << QStringLiteral("repository") << QStringLiteral("disable") << repoName;
    
    return runProcess(QStringLiteral("/usr/bin/eselect"), cmdArgs, 30000);
}

ActionReply PortageAuthHelper::repositoryRemove(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::repositoryRemove called");
    
    const QString repoName = args.value(QStringLiteral("name")).toString();
    
    if (repoName.isEmpty()) {
        return errorReply(QStringLiteral("Repository name is required"));
    }
    
    // Don't allow removing the main gentoo repository
    if (repoName == QStringLiteral("gentoo")) {
        return errorReply(QStringLiteral("Cannot remove the main Gentoo repository"));
    }
    
    syslog(LOG_INFO, "eselect repository remove -f %s", qPrintable(repoName));
    
    QStringList cmdArgs;
    cmdArgs << QStringLiteral("repository") << QStringLiteral("remove") 
            << QStringLiteral("-f") << repoName;
    
    return runProcess(QStringLiteral("/usr/bin/eselect"), cmdArgs, 30000);
}

ActionReply PortageAuthHelper::repositoryAdd(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::repositoryAdd called");
    
    const QString repoName = args.value(QStringLiteral("name")).toString();
    const QString syncType = args.value(QStringLiteral("syncType")).toString();
    const QString syncUri = args.value(QStringLiteral("syncUri")).toString();
    
    if (repoName.isEmpty() || syncType.isEmpty() || syncUri.isEmpty()) {
        return errorReply(QStringLiteral("Repository name, sync type, and URI are required"));
    }
    
    syslog(LOG_INFO, "eselect repository add %s %s %s", qPrintable(repoName), qPrintable(syncType), qPrintable(syncUri));
    
    QStringList cmdArgs;
    cmdArgs << QStringLiteral("repository") << QStringLiteral("add") << repoName << syncType << syncUri;
    
    return runProcess(QStringLiteral("/usr/bin/eselect"), cmdArgs, 60000);
}

ActionReply PortageAuthHelper::repositorySync(const QVariantMap &args)
{
    syslog(LOG_INFO, "PortageAuthHelper::repositorySync called");
    
    const QString repoName = args.value(QStringLiteral("repository")).toString();
    const bool runEixUpdate = args.value(QStringLiteral("runEixUpdate"), true).toBool();
    
    ActionReply reply;
    
    // Run emaint sync
    if (repoName.isEmpty()) {
        // Sync all repositories
        syslog(LOG_INFO, "emaint sync --auto");
        reply = runProcess(QStringLiteral("/usr/sbin/emaint"), 
                          QStringList() << QStringLiteral("sync") << QStringLiteral("--auto"),
                          600000); // 10 min timeout
    } else {
        // Sync specific repository
        syslog(LOG_INFO, "emaint sync -r %s", qPrintable(repoName));
        reply = runProcess(QStringLiteral("/usr/sbin/emaint"),
                          QStringList() << QStringLiteral("sync") << QStringLiteral("-r") << repoName,
                          600000); // 10 min timeout
    }
    
    if (reply.failed()) {
        return reply;
    }
    
    // Run eix-update if requested and available
    if (runEixUpdate && QFile::exists(QStringLiteral("/usr/bin/eix-update"))) {
        syslog(LOG_INFO, "Running eix-update");
        ActionReply eixReply = runProcess(QStringLiteral("/usr/bin/eix-update"),
                                         QStringList(),
                                         600000); // 10 min timeout
        
        // Don't fail the whole operation if eix-update fails
        if (eixReply.failed()) {
            syslog(LOG_WARNING, "eix-update failed but continuing");
        }
    }
    
    return successReply();
}

KAUTH_HELPER_MAIN("org.kde.discover.portagebackend", PortageAuthHelper)

#include "moc_PortageAuthHelper.cpp"
