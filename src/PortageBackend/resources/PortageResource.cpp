/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageResource.h"
#include "../backend/PortageBackend.h"
#include "../auth/PortageAuthClient.h"
#include "PortageUseFlags.h"
#include "../config/MakeConfReader.h"
#include "../repository/PortageRepositoryConfig.h"
#include "../repository/PortageRepositoryReader.h"
#include <KLocalizedString>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <QDir>
#include <QInputDialog>
#include <algorithm>

PortageResource::PortageResource(const QString &atom,
                                 const QString &name,
                                 const QString &summary,
                                 AbstractResourcesBackend *parent)
    : AbstractResource(parent)
    , m_atom(atom)
    , m_packageName(name)
    , m_name(name)
    , m_summary(summary)
    , m_availableVersion(QStringLiteral("0.0.0"))
    , m_installedVersion(QString())
    , m_size(0)
    , m_state(AbstractResource::None)
    , m_discoverCategories({QStringLiteral("portage_packages")})
    , m_keyword(QStringLiteral("amd64"))
    , m_availableVersions(QStringList())
    , m_requestedVersion(QString())
{
    int slashPos = atom.indexOf(QLatin1Char('/'));
    if (slashPos > 0) {
        m_category = atom.left(slashPos);
    } else {
        m_category = QStringLiteral("unknown");
    }
    
    if (!m_category.isEmpty()) {
        m_discoverCategories.insert(m_category);
    }
    //qDebug() << "Portage: Created resource" << m_atom << "category:" << m_category;
}

void PortageResource::requestInstallVersion(const QString &version)
{
    qDebug() << "PortageResource: requestInstallVersion(" << version << ") for" << m_atom;
    // Just set the version - don't call installApplication again!
    // installApplication() already called us from the dialog
    setRequestedVersion(version);
}

void PortageResource::requestReinstall()
{
    qDebug() << "PortageResource: requestReinstall() for" << m_atom;
    
    // Get backend and call installApplication() which will show all dialogs
    auto *backend = qobject_cast<PortageBackend*>(parent());
    if (!backend) {
        qWarning() << "PortageResource: Cannot get backend for reinstall";
        return;
    }
    
    // Clear any previously selected version to force dialog to show
    setRequestedVersion(QString());
    
    // Call installApplication - it will show version + USE flags dialogs
    Transaction *transaction = backend->installApplication(this);
    
    if (transaction) {
        qDebug() << "PortageResource: Reinstall transaction created, will start automatically";
    } else {
        qDebug() << "PortageResource: Reinstall cancelled by user";
    }
}

QString PortageResource::availableVersion() const
{
    // If installed, show installed version
    if (m_state == AbstractResource::Installed || m_state == AbstractResource::Upgradeable) {
        return m_availableVersion;
    }
    
    // For non-installed packages, check if multiple versions available
    QStringList versions = PortageRepositoryReader::getAvailableVersions(m_atom, m_repository);
    if (versions.size() > 1) {
        return QStringLiteral("multiple versions");
    } else if (versions.size() == 1) {
        return versions.first();
    }
    
    return m_availableVersion;
}

QString PortageResource::installedVersion() const
{
    // If not installed, return empty
    if (m_state != AbstractResource::Installed && m_state != AbstractResource::Upgradeable) {
        return QString();
    }
    
    // Return cached installed version
    return m_installedVersion;
}

QStringList PortageResource::availableVersions()
{
    // Lazy-load versions on first access to avoid scanning 20k+ packages at startup
    if (m_availableVersions.isEmpty()) {
        m_availableVersions = PortageRepositoryReader::getAvailableVersions(m_atom, m_repository);
        
        // Also set the latest as available version if not already set
        if (!m_availableVersions.isEmpty() && m_availableVersion == QStringLiteral("0.0.0")) {
            m_availableVersion = m_availableVersions.first();
        }
    }
    
    return m_availableVersions;
}

QString PortageResource::longDescription()
{
    if (m_longDescription.isEmpty()) {
        loadMetadata();
    }
    
    if (!m_longDescription.isEmpty()) {
        return m_longDescription;
    }
    
    return m_summary + QStringLiteral("\n\nThis is a Portage package. Full description is unavailable.");
}

QVariant PortageResource::icon() const
{
    return QStringLiteral("package-x-generic");
}

bool PortageResource::hasCategory(const QString &category) const
{
    return m_discoverCategories.contains(category);
}

QUrl PortageResource::homepage()
{
    return QUrl(QStringLiteral("https://github.com/keklick1337/discover-portage-backend/") + m_atom);
}

QUrl PortageResource::bugURL()
{
    return QUrl(QStringLiteral("https://github.com/keklick1337/discover-portage-backend/issues"));
}

QUrl PortageResource::url() const
{
    return QUrl(QStringLiteral("portage://") + m_atom);
}

QJsonArray PortageResource::licenses()
{
    QJsonArray array;
    array.append(QStringLiteral("GPL-2"));
    return array;
}

QString PortageResource::author() const
{
    // Use the first maintainer if available. The metadata parser
    // collects multiple maintainer names/emails into lists.
    QString name;
    QString email;
    if (!m_maintainerNames.isEmpty()) {
        name = m_maintainerNames.first();
    }
    if (!m_maintainerEmails.isEmpty()) {
        email = m_maintainerEmails.first();
    }

    if (!name.isEmpty()) {
        if (!email.isEmpty()) {
            return name + QStringLiteral(" <") + email + QStringLiteral(">");
        }
        return name;
    } else if (!email.isEmpty()) {
        // No name available, show email in angle brackets
        return QStringLiteral("<") + email + QStringLiteral(">");
    }

    return QStringLiteral("Gentoo Maintainers");
}

void PortageResource::invokeApplication() const
{
    qDebug() << "Portage: Launching" << m_packageName;
    QProcess::startDetached(m_packageName, QStringList());
}

void PortageResource::fetchChangelog()
{
    qDebug() << "Portage: fetchChangelog() stub";
    Q_EMIT changelogFetched(QStringLiteral("Changelog not yet implemented."));
}

void PortageResource::fetchScreenshots()
{
    qDebug() << "Portage: fetchScreenshots() stub";
    Q_EMIT screenshotsFetched(Screenshots{});
}

void PortageResource::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        Q_EMIT stateChanged();
        Q_EMIT versionChanged(); // Version display depends on state
    }
}

void PortageResource::setInstalledVersion(const QString &version)
{
    if (m_installedVersion != version) {
        m_installedVersion = version;
        Q_EMIT versionChanged();
        
        // Also trigger USE flags reload when version changes
        loadUseFlagInfo();
    }
}

void PortageResource::loadMetadata()
{
    // Try to read metadata.xml (maintainer, USE descriptions)
    QString pkgDirPath = PortageRepositoryReader::findPackagePath(m_atom, m_repository);
    
    // If found and repository wasn't set, update it
    if (!pkgDirPath.isEmpty() && m_repository.isEmpty()) {
        m_repository = PortageRepositoryReader::findPackageRepository(m_atom);
    }
    
    if (pkgDirPath.isEmpty()) {
        // Package directory not found in any repository
        return;
    }

    parseMetadataXml(pkgDirPath);
    parseEbuildDescription(pkgDirPath);
    m_longDescription = formatLongDescription();
}

void PortageResource::parseMetadataXml(const QString &pkgDirPath)
{
    QFile file(pkgDirPath + QStringLiteral("/metadata.xml"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QXmlStreamReader xml(&file);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            if (xml.name() == QLatin1String("maintainer")) {
                // inside maintainer block - collect one maintainer
                QString mEmail, mName;
                while (!(xml.isEndElement() && xml.name() == QLatin1String("maintainer"))) {
                    xml.readNext();
                    if (xml.isStartElement()) {
                        if (xml.name() == QLatin1String("email")) {
                            mEmail = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                        } else if (xml.name() == QLatin1String("name")) {
                            mName = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                        }
                    }
                }
                // Store this maintainer
                if (!mEmail.isEmpty()) {
                    m_maintainerEmails.append(mEmail);
                }
                if (!mName.isEmpty()) {
                    m_maintainerNames.append(mName);
                }
            } else if (xml.name() == QLatin1String("flag")) {
                const QString fname = xml.attributes().value(QLatin1String("name")).toString();
                const QString fdesc = xml.readElementText(QXmlStreamReader::IncludeChildElements).simplified();
                if (!fname.isEmpty()) {
                    m_useFlagDescriptions.insert(fname, fdesc);
                }
            }
        }
    }
    
    if (xml.hasError()) {
        qDebug() << "Portage: XML parse error for" << m_atom << ":" << xml.errorString();
    }
    file.close();
}

void PortageResource::parseEbuildDescription(const QString &pkgDirPath)
{
    QDir pkgDir(pkgDirPath);
    if (!pkgDir.exists()) {
        return;
    }
    
    const QStringList ebuilds = pkgDir.entryList(QStringList() << QStringLiteral("*.ebuild"), QDir::Files, QDir::Name);
    if (ebuilds.isEmpty()) {
        return;
    }
    
    // pick the lexicographically last ebuild (usually newest)
    const QString ebuildFile = pkgDir.absoluteFilePath(ebuilds.last());
    QFile ef(ebuildFile);
    if (!ef.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    const QString contents = QString::fromUtf8(ef.readAll());
    // Match DESCRIPTION="..." or DESCRIPTION='...' - may be multiline
    // Using non-greedy match and DOTALL-like behavior
    QRegularExpression re(QStringLiteral(R"(DESCRIPTION\s*=\s*[\"']([^\"']*)[\"'])"));
    QRegularExpressionMatch match = re.match(contents);
    if (match.hasMatch()) {
        QString desc = match.captured(1);
        // Normalize whitespace: replace newlines/tabs with spaces, collapse multiple spaces
        desc = desc.simplified();
        if (!desc.isEmpty()) {
            m_ebuildDescription = desc;
        }
    }
    ef.close();
}

QString PortageResource::formatLongDescription()
{
    QStringList parts;

    // Prefer ebuild DESCRIPTION when present, fall back to summary
    const QString descriptionText = !m_ebuildDescription.isEmpty() ? m_ebuildDescription : m_summary;
    parts << QStringLiteral("<div>") + descriptionText.toHtmlEscaped() + QStringLiteral("</div>");

    if (hasMaintainerInfo()) {
        parts << QStringLiteral("<p><strong>Maintainer(s):</strong></p>");
        parts << QStringLiteral("<ul>");

        int maxCount = qMax(m_maintainerNames.size(), m_maintainerEmails.size());
        for (int i = 0; i < maxCount; ++i) {
            QString maintLine;
            if (i < m_maintainerNames.size()) {
                maintLine = m_maintainerNames[i].toHtmlEscaped();
            }
            if (i < m_maintainerEmails.size()) {
                if (!maintLine.isEmpty()) {
                    maintLine += QStringLiteral(" &lt;") + m_maintainerEmails[i].toHtmlEscaped() + QStringLiteral("&gt;");
                } else {
                    maintLine = QStringLiteral("&lt;") + m_maintainerEmails[i].toHtmlEscaped() + QStringLiteral("&gt;");
                }
            }
            if (!maintLine.isEmpty()) {
                parts << QStringLiteral("<li>") + maintLine + QStringLiteral("</li>");
            }
        }
        parts << QStringLiteral("</ul>");
    }

    if (!m_useFlagDescriptions.isEmpty()) {
        parts << QStringLiteral("<p><strong>USE flags:</strong></p>");
        parts << QStringLiteral("<ul>");
        for (auto it = m_useFlagDescriptions.constBegin(); it != m_useFlagDescriptions.constEnd(); ++it) {
            const QString key = it.key().toHtmlEscaped();
            const QString val = it.value().toHtmlEscaped();
            parts << QStringLiteral("<li><strong>") + key + QStringLiteral("</strong>: ") + val + QStringLiteral("</li>");
        }
        parts << QStringLiteral("</ul>");
    }

    return parts.join(QStringLiteral("\n"));
}

bool PortageResource::hasMaintainerInfo() const
{
    return !m_maintainerNames.isEmpty() || !m_maintainerEmails.isEmpty();
}

void PortageResource::setConfiguredUseFlags(const QStringList &flags)
{
    if (m_configuredUseFlags != flags) {
        m_configuredUseFlags = flags;
        Q_EMIT useFlagsChanged();
    }
}

void PortageResource::setInstalledUseFlags(const QStringList &flags)
{
    if (m_installedUseFlags != flags) {
        m_installedUseFlags = flags;
        Q_EMIT useFlagsChanged();
    }
}

void PortageResource::setAvailableUseFlags(const QStringList &flags)
{
    if (m_availableUseFlags != flags) {
        m_availableUseFlags = flags;
        Q_EMIT useFlagsChanged();
    }
}

void PortageResource::setRepository(const QString &repo)
{
    if (m_repository != repo) {
        m_repository = repo;
        Q_EMIT metadataChanged();
    }
}

void PortageResource::setSlot(const QString &slot)
{
    if (m_slot != slot) {
        m_slot = slot;
        Q_EMIT metadataChanged();
    }
}

bool PortageResource::saveUseFlags(const QStringList &flags)
{
    qDebug() << "PortageResource::saveUseFlags() - saving flags for" << m_atom << ":" << flags;
    
    MakeConfReader makeConf;
    QStringList globalL10n = makeConf.readL10N();
    QStringList globalUse = makeConf.readGlobalUseFlags();
    QStringList packageUseGlobal = makeConf.readGlobalPackageUse();
    
    QStringList filteredFlags;
    for (const QString &flag : flags) {
        if (flag.startsWith(QLatin1Char('-'))) {
            filteredFlags << flag;
        }
        else if (flag.startsWith(QStringLiteral("l10n_")) && globalL10n.contains(flag)) {
            qDebug() << "PortageResource: Skipping L10N flag" << flag << "(already in make.conf)";
            continue;
        }
        else if (globalUse.contains(flag) || packageUseGlobal.contains(flag)) {
            qDebug() << "PortageResource: Skipping USE flag" << flag << "(already global)";
            continue;
        }
        else {
            filteredFlags << flag;
        }
    }
    
    qDebug() << "PortageResource: Filtered flags:" << filteredFlags;
    
    // If all flags were filtered out (all are already global), no need to write to package.use
    if (filteredFlags.isEmpty()) {
        qDebug() << "PortageResource: All USE flags are already global, skipping package.use write";
        m_configuredUseFlags.clear();
        return true;
    }
    
    auto *authClient = new PortageAuthClient(this);
    
    authClient->setUseFlags(m_atom, filteredFlags, [this, authClient, filteredFlags](bool ok, const QString &/*output*/, const QString &error) {
        if (ok) {
            qDebug() << "PortageResource: Successfully saved USE flags for" << m_atom;
            m_configuredUseFlags = filteredFlags;
        } else {
            qWarning() << "PortageResource: Failed to save USE flags:" << error;
        }
        authClient->deleteLater();
    });
    
    return true;
}

void PortageResource::loadUseFlagInfo()
{
    //qDebug() << "PortageResource::loadUseFlagInfo() for" << m_atom << "state:" << m_state;
    
    PortageUseFlags useFlagManager;
    
    if (m_state == AbstractResource::Installed || m_state == AbstractResource::Upgradeable) {
        // Read installed package USE flag information
        // Pass empty version to let it auto-detect from /var/db/pkg
        UseFlagInfo info = useFlagManager.readInstalledPackageInfo(m_atom, QString());
        
        // Update version from actual installed package
        if (!info.version.isEmpty() && m_installedVersion != info.version) {
            m_installedVersion = info.version;
            Q_EMIT versionChanged();
        }
        
        if (!info.activeFlags.isEmpty()) {
            m_installedUseFlags = info.activeFlags;
        }
        
        if (!info.availableFlags.isEmpty()) {
            m_availableUseFlags = info.availableFlags;
        }
        
        if (!info.descriptions.isEmpty()) {
            m_useFlagDescriptions = info.descriptions;
        }
        
        if (!info.repository.isEmpty()) {
            m_repository = info.repository;
        }
        
        if (!info.slot.isEmpty()) {
            m_slot = info.slot;
        }
    } else if (m_state == AbstractResource::None) {
        // Package was removed - clear USE flags
        m_installedUseFlags.clear();
        m_availableUseFlags.clear();
        m_useFlagDescriptions.clear();
        //qDebug() << "Cleared USE flags for removed package" << m_atom;
    } else {
        // For non-installed packages, read from repository
        QString version = m_availableVersion.isEmpty() ? QStringLiteral("9999") : m_availableVersion;
        
        // Find repository path using PortageRepositoryConfig
        QString repoPath;
        if (!m_repository.isEmpty()) {
            repoPath = PortageRepositoryConfig::instance().getRepositoryLocation(m_repository);
        } else {
            // Try to find package in any repository
            QString foundRepo = PortageRepositoryReader::findPackageRepository(m_atom);
            if (!foundRepo.isEmpty()) {
                m_repository = foundRepo;
                repoPath = PortageRepositoryConfig::instance().getRepositoryLocation(foundRepo);
            } else {
                // Default to gentoo repository
                repoPath = PortageRepositoryConfig::instance().getRepositoryLocation(QStringLiteral("gentoo"));
            }
        }
        
        UseFlagInfo info = useFlagManager.readRepositoryPackageInfo(m_atom, version, repoPath);
        
        if (!info.availableFlags.isEmpty()) {
            m_availableUseFlags = info.availableFlags;
        }
        
        if (!info.descriptions.isEmpty()) {
            m_useFlagDescriptions = info.descriptions;
        }
    }
    
    // Read configured USE flags from /etc/portage/package.use
    QMap<QString, QStringList> configured = useFlagManager.readPackageUseConfig(m_atom);
    if (!configured.isEmpty()) {
        // Combine all configured flags from all files
        QStringList allConfigured;
        for (auto it = configured.constBegin(); it != configured.constEnd(); ++it) {
            allConfigured << it.value();
        }
        m_configuredUseFlags = allConfigured;
    }
    
    //qDebug() << "Loaded USE flag info for" << m_atom 
    //         << "- Installed:" << m_installedUseFlags.size()
    //         << "Available:" << m_availableUseFlags.size()
    //         << "Configured:" << m_configuredUseFlags.size();
    
    // Emit signal to update UI
    Q_EMIT useFlagsChanged();
}

QList<PackageState> PortageResource::addonsInformation()
{
    return {};
}

QStringList PortageResource::topObjects() const
{
    return {
        QStringLiteral("qrc:/qml/PortageActionInjector.qml"),
        QStringLiteral("qrc:/qml/UseFlagsInfo.qml")
    };
}

QVariantList PortageResource::useFlagsInformation()
{
    qDebug() << "PortageResource::useFlagsInformation() called for" << m_atom
             << "state:" << m_state
             << "available:" << m_availableUseFlags.size()
             << "installed:" << m_installedUseFlags.size();
    
    QVariantList useFlags;
    
    // If package is not installed, show available USE flags from IUSE
    if (m_state != AbstractResource::Installed && m_state != AbstractResource::Upgradeable) {
        for (const QString &flag : m_availableUseFlags) {
            // Check if flag is in configured flags
            bool enabled = m_configuredUseFlags.contains(flag) || m_configuredUseFlags.contains(QLatin1Char('+') + flag);
            bool disabled = m_configuredUseFlags.contains(QLatin1Char('-') + flag);
            
            if (disabled) {
                enabled = false;
            }
            
            QString description = m_useFlagDescriptions.value(flag, flag);
            
            QVariantMap flagData;
            flagData[QStringLiteral("name")] = flag;
            flagData[QStringLiteral("packageName")] = flag;
            flagData[QStringLiteral("description")] = description;
            flagData[QStringLiteral("installed")] = enabled;
            
            useFlags << flagData;
        }
        return useFlags;
    }
    
    // For installed packages, show all available flags with current state
    for (const QString &flag : m_availableUseFlags) {
        bool isInstalled = m_installedUseFlags.contains(flag);
        QString description = m_useFlagDescriptions.value(flag, flag);
        
        QVariantMap flagData;
        flagData[QStringLiteral("name")] = flag;
        flagData[QStringLiteral("packageName")] = flag;
        flagData[QStringLiteral("description")] = description;
        flagData[QStringLiteral("installed")] = isInstalled;
        
        useFlags << flagData;
    }
    
    return useFlags;
}

#include "moc_PortageResource.cpp"
