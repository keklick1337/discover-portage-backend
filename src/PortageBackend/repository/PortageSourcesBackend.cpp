/*
 * SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 * SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageSourcesBackend.h"
#include "PortageRepositoryConfig.h"
#include "../backend/PortageBackend.h"
#include "../auth/PortageAuthClient.h"
#include "../utils/PortagePaths.h"
#include "../utils/QmlEngineUtils.h"

#include <resources/DiscoverAction.h>

#include <KLocalizedString>
#include <QDebug>
#include <QProcess>
#include <QXmlStreamReader>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardItemModel>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QQuickItem>
#include <QWindow>
#include <QGuiApplication>
#include <QCoreApplication>

PortageSourcesBackend::PortageSourcesBackend(AbstractResourcesBackend *parent)
    : AbstractSourcesBackend(parent)
    , m_sources(new QStandardItemModel(this))
    , m_refreshAction(new DiscoverAction(QStringLiteral("view-refresh"), i18n("Refresh Repositories"), this))
    , m_addOverlayAction(new DiscoverAction(QStringLiteral("list-add"), i18n("Add Overlay"), this))
    , m_noSourcesItem(new QStandardItem(i18n("No repositories configured")))
{
    m_noSourcesItem->setEnabled(false);
    connect(m_refreshAction, &DiscoverAction::triggered, this, &PortageSourcesBackend::refreshSources);
    connect(m_addOverlayAction, &DiscoverAction::triggered, this, &PortageSourcesBackend::showAddOverlayDialog);

    loadEnabledRepositories();
    
    // Try to load available repos from eselect first, fallback to XML API
    m_officialRepos = loadAvailableRepositoriesFromEselect();
    if (m_officialRepos.isEmpty()) {
        qDebug() << "Portage: Falling back to XML API for official repositories";
        loadOfficialRepositories();
    }
}

PortageSourcesBackend::~PortageSourcesBackend()
{
    if (!m_noSourcesItem->model())
        delete m_noSourcesItem;
}

QAbstractItemModel *PortageSourcesBackend::sources()
{
    return m_sources;
}

QString PortageSourcesBackend::idDescription()
{
    return QString(); // Return empty to hide text field in standard UI
}

QVariantList PortageSourcesBackend::actions() const
{
    return {
        QVariant::fromValue<QObject *>(m_refreshAction),
        QVariant::fromValue<QObject *>(m_addOverlayAction)
    };
}

void PortageSourcesBackend::showAddOverlayDialog()
{
    qDebug() << "Portage: Opening Add Overlay dialog";
    
    // Get QML engine using the shared utility
    QQmlEngine *engine = QmlEngineUtils::findQmlEngine();
    if (!engine) {
        qWarning() << "Portage: No QML engine available";
        return;
    }
    
    qDebug() << "Portage: Found QML engine with" << m_officialRepos.size() << "official repositories";
    
    // Find the main window to use as parent
    QQuickWindow *mainWindow = nullptr;
    const auto windows = QGuiApplication::allWindows();
    for (QWindow *window : windows) {
        if (auto *quickWindow = qobject_cast<QQuickWindow *>(window)) {
            if (quickWindow->isVisible()) {
                mainWindow = quickWindow;
                break;
            }
        }
    }
    
    if (!mainWindow) {
        qWarning() << "Portage: No main window found";
        return;
    }
    
    // Create a NEW context for the dialog (don't modify root context!)
    QQmlContext *dialogContext = new QQmlContext(engine->rootContext());
    
    // Convert RepositoryInfo list to QVariantList for QML
    QVariantList repoList;
    for (const auto &info : std::as_const(m_officialRepos)) {
        QVariantMap repoMap;
        repoMap[QStringLiteral("name")] = info.name;
        repoMap[QStringLiteral("description")] = info.description;
        repoMap[QStringLiteral("homepage")] = info.homepage;
        repoMap[QStringLiteral("ownerEmail")] = info.ownerEmail;
        repoMap[QStringLiteral("ownerName")] = info.ownerName;
        repoMap[QStringLiteral("sourceUrl")] = info.sourceUrl;
        repoMap[QStringLiteral("feed")] = info.feed;
        repoMap[QStringLiteral("quality")] = info.quality;
        repoMap[QStringLiteral("status")] = info.status;
        repoMap[QStringLiteral("enabled")] = info.enabled;
        repoList.append(repoMap);
    }
    
    // Set context properties ONLY for this dialog
    dialogContext->setContextProperty(QStringLiteral("sourcesBackend"), this);
    dialogContext->setContextProperty(QStringLiteral("officialRepositories"), repoList);
    
    qDebug() << "Portage: Context set with" << repoList.size() << "official repositories";
    
    // Load the dialog component
    QQmlComponent component(engine, QUrl(QStringLiteral("qrc:/qml/AddRepositoryDialog.qml")));
    
    if (component.isError()) {
        qWarning() << "Portage: Failed to load AddRepositoryDialog.qml:" << component.errors();
        delete dialogContext;
        return;
    }
    
    // Create the dialog instance with our context
    QObject *dialog = component.create(dialogContext);
    if (!dialog) {
        qWarning() << "Portage: Failed to create AddRepositoryDialog instance";
        delete dialogContext;
        return;
    }
    
    // Set the context as parent so it gets deleted with the dialog
    dialogContext->setParent(dialog);
    
    qDebug() << "Portage: Dialog type:" << dialog->metaObject()->className();
    qDebug() << "Portage: Is QWindow?" << (qobject_cast<QWindow*>(dialog) != nullptr);
    qDebug() << "Portage: Is QQuickWindow?" << (qobject_cast<QQuickWindow*>(dialog) != nullptr);
    qDebug() << "Portage: Is QQuickItem?" << (qobject_cast<QQuickItem*>(dialog) != nullptr);
    
    // Try different approaches based on actual type
    if (auto *dialogWindow = qobject_cast<QQuickWindow *>(dialog)) {
        // It's a Window - set transient parent
        dialogWindow->setTransientParent(mainWindow);
        qDebug() << "Portage: Set dialog as transient window";
    } else if (auto *dialogItem = qobject_cast<QQuickItem *>(dialog)) {
        // It's an Item (Popup) - set parent item
        QQuickItem *contentItem = mainWindow->contentItem();
        if (contentItem) {
            dialogItem->setParentItem(contentItem);
            qDebug() << "Portage: Set dialog parent item to contentItem";
        }
    } else {
        qWarning() << "Portage: Dialog is neither Window nor Item - checking for 'parent' property";
        // Last resort - try to set via property
        dialog->setProperty("parent", QVariant::fromValue(mainWindow->contentItem()));
    }
    
    qDebug() << "Portage: Invoking open()...";
    
    // Call open() method
    bool openResult = QMetaObject::invokeMethod(dialog, "open");
    qDebug() << "Portage: invokeMethod(open) returned:" << openResult;
}


void PortageSourcesBackend::loadEnabledRepositories()
{
    m_sources->clear();
    
    // Use eselect repository list -i to get installed repositories
    loadRepositoriesFromEselect();
    
    // If eselect didn't return anything, fallback to PortageRepositoryConfig
    if (m_sources->rowCount() == 0) {
        qDebug() << "Portage: Falling back to PortageRepositoryConfig";
        
        PortageRepositoryConfig::instance().reload();
        const QStringList repoNames = PortageRepositoryConfig::instance().getAllRepositoryNames();
        
        if (repoNames.isEmpty()) {
            qDebug() << "Portage: No repositories found in configuration";
            m_sources->appendRow(m_noSourcesItem);
            return;
        }
        
        for (const QString &repoName : repoNames) {
            const auto repo = PortageRepositoryConfig::instance().getRepository(repoName);
            
            auto *item = new QStandardItem(repoName);
            item->setData(repoName, IdRole);
            item->setData(true, EnabledRole);
            
            // Build description with sync info
            QString desc;
            if (repoName == QLatin1String(PortagePaths::DEFAULT_REPO)) {
                desc = i18n("Official Gentoo package repository");
            } else {
                desc = i18n("Portage repository");
            }
            
            if (!repo.syncType.isEmpty()) {
                desc += QStringLiteral(" | ") + i18n("Type: %1", repo.syncType);
            }
            if (!repo.syncUri.isEmpty()) {
                desc += QStringLiteral(" | ") + i18n("Remote: %1", repo.syncUri);
            }
            
            item->setData(desc, DescriptionRole);
            
            if (!repo.location.isEmpty())
                item->setData(repo.location, HomepageRole);
            if (!repo.syncType.isEmpty())
                item->setData(repo.syncType, StatusRole);
            if (!repo.syncUri.isEmpty())
                item->setData(repo.syncUri, OwnerRole);
            
            const bool isDeletable = (repoName != QLatin1String(PortagePaths::DEFAULT_REPO));
            item->setData(isDeletable, DeletableRole);
            
            m_sources->appendRow(item);
        }
    }
    
    qDebug() << "Portage: Loaded" << m_sources->rowCount() << "enabled repositories";
}

void PortageSourcesBackend::loadOfficialRepositories()
{
    auto *nam = new QNetworkAccessManager(this);
    QNetworkRequest request(QUrl(QString::fromLatin1(PortagePaths::GENTOO_REPOSITORIES_API)));
    QNetworkReply *reply = nam->get(request);
    
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            parseRepositoriesXml(reply->readAll());
            handleOfficialReposDownloaded();
        } else {
            qWarning() << "Portage: Failed to download official repositories:" << reply->errorString();
        }
        reply->deleteLater();
    });
}

void PortageSourcesBackend::parseRepositoriesXml(const QByteArray &xmlData)
{
    QXmlStreamReader xml(xmlData);
    m_officialRepos.clear();

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement() && xml.name() == QStringLiteral("repo")) {
            RepositoryInfo info;
            const auto attributes = xml.attributes();
            
            if (attributes.hasAttribute(QStringLiteral("quality")))
                info.quality = attributes.value(QStringLiteral("quality")).toString();
            if (attributes.hasAttribute(QStringLiteral("status")))
                info.status = attributes.value(QStringLiteral("status")).toString();

            while (!(xml.isEndElement() && xml.name() == QStringLiteral("repo"))) {
                xml.readNext();
                if (xml.isStartElement()) {
                    const QString elementName = xml.name().toString();
                    if (elementName == QLatin1String("name"))
                        info.name = xml.readElementText();
                    else if (elementName == QLatin1String("description")) {
                        if (xml.attributes().value(QStringLiteral("lang")) == QLatin1String("en"))
                            info.description = xml.readElementText();
                        else if (info.description.isEmpty())
                            info.description = xml.readElementText();
                        else
                            xml.skipCurrentElement();
                    } else if (elementName == QLatin1String("homepage"))
                        info.homepage = xml.readElementText();
                    else if (elementName == QLatin1String("owner")) {
                        while (!(xml.isEndElement() && xml.name() == QStringLiteral("owner"))) {
                            xml.readNext();
                            if (xml.isStartElement()) {
                                if (xml.name() == QStringLiteral("email"))
                                    info.ownerEmail = xml.readElementText();
                                else if (xml.name() == QStringLiteral("name"))
                                    info.ownerName = xml.readElementText();
                            }
                        }
                    } else if (elementName == QLatin1String("source")) {
                        if (info.sourceUrl.isEmpty() || xml.attributes().value(QStringLiteral("type")) == QLatin1String("git"))
                            info.sourceUrl = xml.readElementText();
                        else
                            xml.skipCurrentElement();
                    } else if (elementName == QLatin1String("feed")) {
                        if (info.feed.isEmpty())
                            info.feed = xml.readElementText();
                        else
                            xml.skipCurrentElement();
                    }
                }
            }

            if (!info.name.isEmpty())
                m_officialRepos.append(info);
        }
    }

    if (xml.hasError())
        qWarning() << "Portage: XML parsing error:" << xml.errorString();
    else
        qDebug() << "Portage: Parsed" << m_officialRepos.size() << "official repositories";
}

void PortageSourcesBackend::handleOfficialReposDownloaded()
{
    for (int i = 0; i < m_sources->rowCount(); ++i) {
        auto *item = m_sources->item(i);
        const QString repoName = item->data(IdRole).toString();
        
        for (const auto &info : std::as_const(m_officialRepos)) {
            if (info.name == repoName) {
                // Build description with sync info
                QString desc = info.description;
                const QString syncType = item->data(StatusRole).toString();
                const QString syncUri = item->data(OwnerRole).toString();
                
                if (!syncType.isEmpty()) {
                    desc += QStringLiteral(" | ") + i18n("Type: %1", syncType);
                }
                if (!syncUri.isEmpty()) {
                    desc += QStringLiteral(" | ") + i18n("Remote: %1", syncUri);
                }
                
                item->setData(desc, DescriptionRole);
                item->setData(info.homepage, HomepageRole);
                item->setData(info.quality, QualityRole);
                break;
            }
        }
    }
}

QStandardItem *PortageSourcesBackend::findSourceByName(const QString &name) const
{
    for (int i = 0; i < m_sources->rowCount(); ++i) {
        auto *item = m_sources->item(i);
        if (item->data(IdRole).toString() == name)
            return item;
    }
    return nullptr;
}

bool PortageSourcesBackend::addSource(const QString &id)
{
    if (id.isEmpty())
        return false;
    
    if (findSourceByName(id)) {
        Q_EMIT passiveMessage(i18n("Repository '%1' is already enabled", id));
        return false;
    }

    qDebug() << "Portage: Enabling repository" << id << "via KAuth";
    PortageAuthClient *client = new PortageAuthClient(this);
    
    client->repositoryEnable(id, [this, id, client](bool success, const QString &/*output*/, const QString &error) {
        client->deleteLater();
        if (success) {
            qDebug() << "Portage: Repository" << id << "enabled successfully";
            syncRepository(id);
        } else {
            qWarning() << "Portage: Failed to enable repository" << id << ":" << error;
            Q_EMIT passiveMessage(i18n("Failed to enable repository '%1': %2", id, error));
        }
    });
    
    return true;
}

bool PortageSourcesBackend::removeSource(const QString &id)
{
    if (id.isEmpty() || id == QLatin1String(PortagePaths::DEFAULT_REPO)) {
        Q_EMIT passiveMessage(i18n("Cannot remove the main Gentoo repository"));
        return false;
    }

    auto *item = findSourceByName(id);
    if (!item)
        return false;

    qDebug() << "Portage: Removing repository" << id << "via KAuth";
    PortageAuthClient *client = new PortageAuthClient(this);
    
    client->repositoryRemove(id, [this, id, client](bool success, const QString &/*output*/, const QString &error) {
        client->deleteLater();
        if (success) {
            qDebug() << "Portage: Repository" << id << "removed successfully";
            refreshSources();
            
            auto *backend = qobject_cast<PortageBackend *>(parent());
            if (backend)
                backend->reloadPackages();
            
            Q_EMIT passiveMessage(i18n("Repository '%1' has been removed", id));
        } else {
            qWarning() << "Portage: Failed to remove repository" << id << ":" << error;
            Q_EMIT passiveMessage(i18n("Failed to remove repository '%1': %2", id, error));
        }
    });
    
    return true;
}

void PortageSourcesBackend::syncRepository(const QString &id)
{
    qDebug() << "Portage: Syncing repository" << id << "via KAuth";
    Q_EMIT passiveMessage(i18n("Syncing repository '%1'...", id));

    PortageAuthClient *client = new PortageAuthClient(this);
    client->repositorySync(id, true, [this, id, client](bool success, const QString &/*output*/, const QString &error) {
        client->deleteLater();
        if (success) {
            qDebug() << "Portage: Repository" << id << "synced successfully";
            refreshSources();
            
            auto *backend = qobject_cast<PortageBackend *>(parent());
            if (backend)
                backend->reloadPackages();
            
            Q_EMIT passiveMessage(i18n("Repository '%1' has been enabled and synced", id));
        } else {
            qWarning() << "Portage: Failed to sync repository" << id << ":" << error;
            Q_EMIT passiveMessage(i18n("Failed to sync repository '%1': %2", id, error));
        }
    });
}

void PortageSourcesBackend::refreshSources()
{
    loadEnabledRepositories();
    
    // Reload official repos list
    m_officialRepos = loadAvailableRepositoriesFromEselect();
    if (m_officialRepos.isEmpty()) {
        loadOfficialRepositories();
    }
}

bool PortageSourcesBackend::addManualSource(const QString &name, const QString &syncType, const QString &syncUri)
{
    if (name.isEmpty() || syncUri.isEmpty()) {
        Q_EMIT passiveMessage(i18n("Repository name and sync URI cannot be empty"));
        return false;
    }
    
    if (findSourceByName(name)) {
        Q_EMIT passiveMessage(i18n("Repository '%1' already exists", name));
        return false;
    }

    if (syncType == QLatin1String("mercurial")) {
        QProcess eq;
        eq.start(QStringLiteral("equery"), QStringList{QStringLiteral("u"), QStringLiteral("app-eselect/eselect-repository")});
        eq.waitForFinished(5000);
        
        const QString out = QString::fromUtf8(eq.readAllStandardOutput());
        const bool mercurialEnabled = out.contains(QStringLiteral("+mercurial"));
        
        if (!mercurialEnabled) {
            Q_EMIT passiveMessage(i18n("Mercurial support requires app-eselect/eselect-repository with USE=mercurial. Please rebuild: emerge -av app-eselect/eselect-repository"));
            return false;
        }
    }

    qDebug() << "Portage: Adding manual repository" << name << "with" << syncType << "from" << syncUri;
    PortageAuthClient *client = new PortageAuthClient(this);
    
    client->repositoryAdd(name, syncType, syncUri, [this, name, client](bool success, const QString &/*output*/, const QString &error) {
        client->deleteLater();
        if (success) {
            qDebug() << "Portage: Manual repository" << name << "added successfully";
            syncRepository(name);
        } else {
            qWarning() << "Portage: Failed to add manual repository" << name << ":" << error;
            Q_EMIT passiveMessage(i18n("Failed to add repository '%1': %2", name, error));
        }
    });
    
    return true;
}

void PortageSourcesBackend::loadRepositoriesFromEselect()
{
    QProcess proc;
    proc.start(QStringLiteral("eselect"), QStringList{QStringLiteral("repository"), QStringLiteral("list"), QStringLiteral("-i")});
    
    if (!proc.waitForFinished(5000)) {
        qWarning() << "Portage: eselect repository list -i timed out";
        return;
    }
    
    if (proc.exitCode() != 0) {
        qWarning() << "Portage: eselect repository list -i failed with exit code" << proc.exitCode();
        return;
    }
    
    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        // Skip header and warnings
        if (line.startsWith(QLatin1String("Available repositories:")) ||
            line.startsWith(QLatin1String("warning:")) ||
            line.trimmed().isEmpty()) {
            continue;
        }
        
        // Format: "  [129] gentoo * (https://gentoo.org/)"
        // or:     "  [180] kek-overlay @"
        const QString trimmed = line.trimmed();
        
        // Extract repository name (between ] and marker * or @)
        const int closeBracket = trimmed.indexOf(QLatin1Char(']'));
        if (closeBracket == -1) continue;
        
        QString remaining = trimmed.mid(closeBracket + 1).trimmed();
        
        // Find first space after repo name
        int spaceIdx = remaining.indexOf(QLatin1Char(' '));
        if (spaceIdx == -1) continue;
        
        const QString repoName = remaining.left(spaceIdx);
        remaining = remaining.mid(spaceIdx + 1).trimmed();
        
        // Check marker (* = synced, @ = local)
        QString syncType;
        if (remaining.startsWith(QLatin1Char('*'))) {
            syncType = QStringLiteral("remote");
            remaining = remaining.mid(1).trimmed();
        } else if (remaining.startsWith(QLatin1Char('@'))) {
            syncType = QStringLiteral("local");
            remaining = remaining.mid(1).trimmed();
        }
        
        // Extract URL from parentheses (optional)
        QString url;
        if (remaining.startsWith(QLatin1Char('('))) {
            const int closeP = remaining.indexOf(QLatin1Char(')'));
            if (closeP != -1) {
                url = remaining.mid(1, closeP - 1);
            }
        }
        
        // Create item without checkbox
        auto *item = new QStandardItem(repoName);
        item->setData(repoName, IdRole);
        item->setData(true, EnabledRole);
        
        // Build description with sync info
        QString desc;
        if (repoName == QLatin1String(PortagePaths::DEFAULT_REPO)) {
            desc = i18n("Official Gentoo package repository");
        } else {
            desc = i18n("Portage repository");
        }
        
        if (!syncType.isEmpty()) {
            desc += QStringLiteral(" | ") + i18n("Type: %1", syncType);
        }
        if (!url.isEmpty()) {
            desc += QStringLiteral(" | ") + i18n("Remote: %1", url);
        }
        
        item->setData(desc, DescriptionRole);
        
        if (!url.isEmpty()) {
            item->setData(url, HomepageRole);
            item->setData(url, OwnerRole);
        }
        
        item->setData(syncType, StatusRole);
        
        const bool isDeletable = (repoName != QLatin1String(PortagePaths::DEFAULT_REPO));
        item->setData(isDeletable, DeletableRole);
        
        m_sources->appendRow(item);
    }
    
    qDebug() << "Portage: Loaded" << m_sources->rowCount() << "repositories from eselect";
}

QList<RepositoryInfo> PortageSourcesBackend::loadAvailableRepositoriesFromEselect()
{
    QList<RepositoryInfo> repos;
    
    QProcess proc;
    proc.start(QStringLiteral("eselect"), QStringList{QStringLiteral("repository"), QStringLiteral("list")});
    
    if (!proc.waitForFinished(5000)) {
        qWarning() << "Portage: eselect repository list timed out";
        return repos;
    }
    
    if (proc.exitCode() != 0) {
        qWarning() << "Portage: eselect repository list failed with exit code" << proc.exitCode();
        return repos;
    }
    
    const QString output = QString::fromUtf8(proc.readAllStandardOutput());
    const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    
    for (const QString &line : lines) {
        // Skip header and warnings
        if (line.startsWith(QLatin1String("Available repositories:")) ||
            line.startsWith(QLatin1String("warning:")) ||
            line.trimmed().isEmpty()) {
            continue;
        }
        
        const QString trimmed = line.trimmed();
        
        // Extract repository name
        const int closeBracket = trimmed.indexOf(QLatin1Char(']'));
        if (closeBracket == -1) continue;
        
        QString remaining = trimmed.mid(closeBracket + 1).trimmed();
        
        // Find first space after repo name
        int spaceIdx = remaining.indexOf(QLatin1Char(' '));
        QString repoName;
        if (spaceIdx == -1) {
            // No space, entire string is repo name
            repoName = remaining;
            remaining.clear();
        } else {
            repoName = remaining.left(spaceIdx);
            remaining = remaining.mid(spaceIdx + 1).trimmed();
        }
        
        // Check marker (* = synced, @ = local)
        bool isEnabled = false;
        if (remaining.startsWith(QLatin1Char('*'))) {
            isEnabled = true;
            remaining = remaining.mid(1).trimmed();
        } else if (remaining.startsWith(QLatin1Char('@'))) {
            isEnabled = true;
            remaining = remaining.mid(1).trimmed();
        }
        
        // Extract URL from parentheses (optional)
        QString url;
        if (remaining.startsWith(QLatin1Char('('))) {
            const int closeP = remaining.indexOf(QLatin1Char(')'));
            if (closeP != -1) {
                url = remaining.mid(1, closeP - 1);
            }
        }
        
        RepositoryInfo info;
        info.name = repoName;
        info.homepage = url;
        info.sourceUrl = url;
        info.enabled = isEnabled;
        
        repos.append(info);
    }
    
    qDebug() << "Portage: Found" << repos.size() << "available repositories from eselect";
    return repos;
}
