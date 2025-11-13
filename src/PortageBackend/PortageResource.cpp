/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageResource.h"
#include "PortageBackend.h"
#include "PortageUseFlags.h"
#include <KLocalizedString>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QXmlStreamReader>
#include <QDir>

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
    qDebug() << "Portage: Created resource" << m_atom << "category:" << m_category;
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
    }
}

void PortageResource::loadMetadata()
{
    // Try to read metadata.xml (maintainer, USE descriptions)
    // Search across all repositories in /var/db/repos/
    QString pkgDirPath;
    
    // If we know the repository, check there first
    if (!m_repository.isEmpty()) {
        const QString repoPath = QStringLiteral("/var/db/repos/") + m_repository + QStringLiteral("/") + m_atom;
        if (QDir(repoPath).exists()) {
            pkgDirPath = repoPath;
        }
    }
    
    // Otherwise, search all repositories
    if (pkgDirPath.isEmpty()) {
        QDir reposDir(QStringLiteral("/var/db/repos"));
        const QStringList repos = reposDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QString &repo : repos) {
            const QString testPath = QStringLiteral("/var/db/repos/") + repo + QStringLiteral("/") + m_atom;
            if (QDir(testPath).exists()) {
                pkgDirPath = testPath;
                m_repository = repo; // remember which repo we found it in
                break;
            }
        }
    }
    
    if (pkgDirPath.isEmpty()) {
        // Package directory not found in any repository
        return;
    }

    QFile file(pkgDirPath + QStringLiteral("/metadata.xml"));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
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

    // Try to read DESCRIPTION from the latest ebuild in the package directory
    QDir pkgDir(pkgDirPath);
    if (pkgDir.exists()) {
        const QStringList ebuilds = pkgDir.entryList(QStringList() << QStringLiteral("*.ebuild"), QDir::Files, QDir::Name);
        if (!ebuilds.isEmpty()) {
            // pick the lexicographically last ebuild (usually newest)
            const QString ebuildFile = pkgDir.absoluteFilePath(ebuilds.last());
            QFile ef(ebuildFile);
            if (ef.open(QIODevice::ReadOnly | QIODevice::Text)) {
                const QString contents = QString::fromUtf8(ef.readAll());
                // Match DESCRIPTION="..." or DESCRIPTION='...' - may be multiline
                // Using non-greedy match and DOTALL-like behavior
                // TODO: improve to handle escaped quotes inside description?
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
        }
    }

    QStringList parts;

    // Prefer ebuild DESCRIPTION when present, fall back to summary
    const QString descriptionText = !m_ebuildDescription.isEmpty() ? m_ebuildDescription : m_summary;
    parts << QStringLiteral("<div>") + descriptionText.toHtmlEscaped() + QStringLiteral("</div>");

    if (!m_maintainerNames.isEmpty() || !m_maintainerEmails.isEmpty()) {
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

    m_longDescription = parts.join(QStringLiteral("\n"));
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
    
    PortageUseFlags useFlagManager;
    bool success = useFlagManager.writeUseFlags(m_atom, m_packageName, flags);
    
    if (success) {
        m_configuredUseFlags = flags;
        qDebug() << "Successfully saved USE flags for" << m_atom;
    } else {
        qDebug() << "Failed to save USE flags for" << m_atom;
    }
    
    return success;
}

void PortageResource::loadUseFlagInfo()
{
    qDebug() << "PortageResource::loadUseFlagInfo() for" << m_atom;
    
    if (m_state != AbstractResource::Installed) {
        // Only load USE flag info for installed packages
        return;
    }
    
    PortageUseFlags useFlagManager;
    
    // Read installed package USE flag information
    UseFlagInfo info = useFlagManager.readInstalledPackageInfo(m_atom, m_installedVersion);
    
    if (!info.activeFlags.isEmpty()) {
        m_installedUseFlags = info.activeFlags;
    }
    
    if (!info.availableFlags.isEmpty()) {
        m_availableUseFlags = info.availableFlags;
    }
    
    if (!info.repository.isEmpty()) {
        m_repository = info.repository;
    }
    
    if (!info.slot.isEmpty()) {
        m_slot = info.slot;
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
    
    qDebug() << "Loaded USE flag info for" << m_atom 
             << "- Installed:" << m_installedUseFlags.size()
             << "Available:" << m_availableUseFlags.size()
             << "Configured:" << m_configuredUseFlags.size();
}

#include "moc_PortageResource.cpp"
