/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageBackend.h"
#include "PortageResource.h"
#include "PortageTransaction.h"

#include <Category/Category.h>
#include <resources/StandardBackendUpdater.h>
#include <KLocalizedString>
#include <KPluginFactory>
#include <QDebug>
#include <QTimer>
#include "repository/PortageRepositoryReader.h"
#include "installed/PortageInstalledReader.h"

DISCOVER_BACKEND_PLUGIN(PortageBackend)

PortageBackend::PortageBackend(QObject *parent)
    : AbstractResourcesBackend(parent)
    , m_updater(new StandardBackendUpdater(this))
    , m_initialized(false)
{
    qDebug() << "Portage: Initializing backend";

    // Load repository packages
    PortageRepositoryReader repoReader(this, this);
    repoReader.loadRepository();
    const auto repoPackages = repoReader.packages();
    
    // Collect known atoms for better version parsing
    QSet<QString> knownAtoms;
    for (auto it = repoPackages.constBegin(); it != repoPackages.constEnd(); ++it) {
        PortageResource *r = it.value();
        // insert by atom (category/package, lowercase)
        QString atom = r->atom().toLower();
        m_resources.insert(atom, r);
        knownAtoms.insert(atom);
    }

    // Load installed packages and update resource states
    PortageInstalledReader instReader(this, this);
    instReader.setKnownPackages(knownAtoms); // Pass known packages for better parsing
    instReader.loadInstalledPackages();
    const auto installed = instReader.installedVersions();
    const auto installedInfo = instReader.installedPackagesInfo();
    
    for (auto it = installedInfo.constBegin(); it != installedInfo.constEnd(); ++it) {
        const QString atom = it.key();
        const InstalledPackageInfo &info = it.value();
        
        // Look up by atom (category/package)
        PortageResource *r = m_resources.value(atom.toLower(), nullptr);
        if (r) {
            r->setInstalledVersion(info.version);
            r->setState(AbstractResource::Installed);
            r->setRepository(info.repository);
            r->setSlot(info.slot);
            r->setInstalledUseFlags(info.useFlags);
            r->setAvailableUseFlags(info.availableUseFlags);
        } else {
            // Create resource for installed package not present in repo scan
            const QString pkg = atom.section(QLatin1Char('/'), 1);
            PortageResource *nr = new PortageResource(atom, pkg, QString(), this);
            nr->setInstalledVersion(info.version);
            nr->setState(AbstractResource::Installed);
            nr->setRepository(info.repository);
            nr->setSlot(info.slot);
            nr->setInstalledUseFlags(info.useFlags);
            nr->setAvailableUseFlags(info.availableUseFlags);
            m_resources.insert(atom.toLower(), nr);
        }
    }

    m_initialized = true;
    qDebug() << "Portage: Backend initialized with" << m_resources.size() << "packages";
    Q_EMIT contentsChanged();
}

QString PortageBackend::displayName() const
{
    return QStringLiteral("Portage");
}

void PortageBackend::populateTestPackages()
{
    auto firefox = new PortageResource(
        QStringLiteral("www-client/firefox"),
        QStringLiteral("firefox"),
        QStringLiteral("Mozilla Firefox Web Browser"),
        this
    );
    firefox->setState(AbstractResource::Installed);
    firefox->setInstalledVersion(QStringLiteral("115.0"));
    firefox->setAvailableVersion(QStringLiteral("120.0"));
    firefox->setSize(200 * 1024 * 1024);
    m_resources.insert(firefox->packageName().toLower(), firefox);
    
    auto vlc = new PortageResource(
        QStringLiteral("media-video/vlc"),
        QStringLiteral("vlc"),
        QStringLiteral("VLC media player"),
        this
    );
    vlc->setState(AbstractResource::None);
    vlc->setAvailableVersion(QStringLiteral("3.0.20"));
    vlc->setSize(50 * 1024 * 1024);
    m_resources.insert(vlc->packageName().toLower(), vlc);
    
    auto gimp = new PortageResource(
        QStringLiteral("media-gfx/gimp"),
        QStringLiteral("gimp"),
        QStringLiteral("GNU Image Manipulation Program"),
        this
    );
    gimp->setState(AbstractResource::Upgradeable);
    gimp->setInstalledVersion(QStringLiteral("2.10.34"));
    gimp->setAvailableVersion(QStringLiteral("2.10.36"));
    gimp->setSize(120 * 1024 * 1024);
    m_resources.insert(gimp->packageName().toLower(), gimp);
    
    qDebug() << "Portage: Created test packages: firefox, vlc, gimp";
}

ResultsStream *PortageBackend::search(const AbstractResourcesBackend::Filters &filter)
{
    QVector<AbstractResource *> results;
    
    if (!filter.search.isEmpty()) {
        const QString searchTerm = filter.search.toLower();
        for (auto *res : m_resources) {
            if (res->name().toLower().contains(searchTerm) ||
                res->packageName().toLower().contains(searchTerm) ||
                res->comment().toLower().contains(searchTerm)) {
                results << res;
            }
        }
    }
    else if (filter.category) {
        const auto categories = filter.category->involvedCategories();
        for (auto *res : m_resources) {
            for (const auto &cat : categories) {
                if (res->hasCategory(cat)) {
                    results << res;
                    break;
                }
            }
        }
    }
    else {
        // No filter - don't return everything, it's too slow
        // Return empty or limited results
        // TODO: return popular packages or recently updated packages
    }

    auto stream = new ResultsStream(QStringLiteral("Portage-search"));
    
    if (results.isEmpty()) {
        QTimer::singleShot(0, stream, [stream]() {
            stream->finish();
        });
        return stream;
    }
    
    // Send results in batches to avoid UI freeze
    const int batchSize = 100;
    int totalBatches = (results.size() + batchSize - 1) / batchSize;
    
    for (int i = 0; i < results.size(); i += batchSize) {
        QVector<StreamResult> batch;
        for (int j = i; j < qMin(i + batchSize, results.size()); ++j) {
            batch << results[j];
        }
        
        int batchNum = i / batchSize;
        bool isLast = (batchNum == totalBatches - 1);
        
        QTimer::singleShot(batchNum * 10, stream, [stream, batch, isLast]() {
            Q_EMIT stream->resourcesFound(batch);
            if (isLast) {
                stream->finish();
            }
        });
    }
    
    return stream;
}

QList<std::shared_ptr<Category>> PortageBackend::category() const
{
    // Root category (all Portage packages)
    CategoryFilter rootFlt{CategoryFilter::FilterType::CategoryNameFilter, QLatin1String("portage_packages")};

    // Collect unique portage categories (the part before the slash)
    QSet<QString> portageCats;
    for (PortageResource *res : m_resources.values()) {
        const QString cat = res->section();
        if (!cat.isEmpty())
            portageCats.insert(cat);
    }

    // Create child categories for each portage category
    QList<std::shared_ptr<Category>> children;
    for (const QString &cat : portageCats) {
        CategoryFilter f{CategoryFilter::FilterType::CategoryNameFilter, cat};
        auto c = std::make_shared<Category>(
            i18nc("Portage subcategory", "%1", cat),
            QStringLiteral("package-x-generic"),
            f,
            QSet<QString>{displayName()},
            QList<std::shared_ptr<Category>>{},
            false);
        children << c;
    }

    auto root = std::make_shared<Category>(
        i18nc("Root category name", "Portage Packages"),
        QStringLiteral("package-x-generic"),
        rootFlt,
        QSet<QString>{displayName()},
        children,
        false);

    return {root};
}

int PortageBackend::updatesCount() const
{
    int count = 0;
    for (auto *res : m_resources) {
        if (res->state() == AbstractResource::Upgradeable) {
            count++;
        }
    }
    return count;
}

AbstractBackendUpdater *PortageBackend::backendUpdater() const
{
    return m_updater;
}

void PortageBackend::checkForUpdates()
{
    qDebug() << "Portage: checkForUpdates() stub";
    Q_EMIT updatesCountChanged();
}

Transaction *PortageBackend::installApplication(AbstractResource *app)
{
    qDebug() << "Portage: installApplication()" << app->name();
    return new PortageTransaction(qobject_cast<PortageResource *>(app), Transaction::InstallRole);
}

Transaction *PortageBackend::installApplication(AbstractResource *app, const AddonList &addons)
{
    qDebug() << "Portage: installApplication() with addons" << app->name();
    return new PortageTransaction(qobject_cast<PortageResource *>(app), addons, Transaction::InstallRole);
}

Transaction *PortageBackend::removeApplication(AbstractResource *app)
{
    qDebug() << "Portage: removeApplication()" << app->name();
    return new PortageTransaction(qobject_cast<PortageResource *>(app), Transaction::RemoveRole);
}

#include "PortageBackend.moc"
#include "moc_PortageBackend.cpp"
