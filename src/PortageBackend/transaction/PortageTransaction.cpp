/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "PortageTransaction.h"
#include "../resources/PortageResource.h"
#include "../backend/PortageBackend.h"
#include "../emerge/EmergeRunner.h"
#include "../emerge/UnmaskManager.h"
#include <KLocalizedString>
#include <QTimer>
#include <QPointer>
#include <QDir>
#include <QDebug>

PortageTransaction::PortageTransaction(PortageResource *app, Role role)
    : Transaction(app, app, role)
    , m_resource(app)
    , m_progress(0)
    , m_emergeRunner(nullptr)
    , m_unmaskManager(nullptr)
{
    qDebug() << "Portage: Transaction created for" << app->name();
    setCancellable(true);
    setStatus(QueuedStatus);
    
    m_emergeRunner = new EmergeRunner(this);
    m_unmaskManager = new UnmaskManager(this);
    
    connect(m_emergeRunner, &EmergeRunner::outputReceived, this, &PortageTransaction::onEmergeOutput);
    connect(m_emergeRunner, &EmergeRunner::errorReceived, this, &PortageTransaction::onEmergeError);
    connect(m_emergeRunner, &EmergeRunner::processFinished, this, &PortageTransaction::onEmergeFinished);
    connect(m_emergeRunner, &EmergeRunner::dependenciesChecked, this, &PortageTransaction::onDependenciesChecked);
    
    QTimer::singleShot(0, this, &PortageTransaction::proceed);
}

PortageTransaction::PortageTransaction(PortageResource *app, const AddonList &addons, Role role)
    : Transaction(app, app, role, addons)
    , m_resource(app)
    , m_addons(addons)
    , m_progress(0)
    , m_emergeRunner(nullptr)
    , m_unmaskManager(nullptr)
{
    qDebug() << "Portage: Transaction with addons created for" << app->name();
    setCancellable(true);
    setStatus(QueuedStatus);
    
    m_emergeRunner = new EmergeRunner(this);
    m_unmaskManager = new UnmaskManager(this);
    
    connect(m_emergeRunner, &EmergeRunner::outputReceived, this, &PortageTransaction::onEmergeOutput);
    connect(m_emergeRunner, &EmergeRunner::errorReceived, this, &PortageTransaction::onEmergeError);
    connect(m_emergeRunner, &EmergeRunner::processFinished, this, &PortageTransaction::onEmergeFinished);
    connect(m_emergeRunner, &EmergeRunner::dependenciesChecked, this, &PortageTransaction::onDependenciesChecked);
    
    QTimer::singleShot(0, this, &PortageTransaction::proceed);
}

void PortageTransaction::cancel()
{
    qDebug() << "Portage: Transaction cancelled";
    if (m_emergeRunner) {
        m_emergeRunner->cancel();
    }
    setStatus(CancelledStatus);
}

void PortageTransaction::proceed()
{
    qDebug() << "Portage: Transaction proceeding for" << m_resource->name();
    setStatus(CommittingStatus);
    
    QString atom = m_resource->atom();
    
    if (role() == InstallRole) {
        qDebug() << "Portage: Starting installation of" << atom;
        // First check dependencies
        // If a specific version was requested via UI, use exact versioned atom
        QString requested = m_resource->requestedVersion();
        if (!requested.isEmpty()) {
            QString exact = QStringLiteral("=") + atom + QStringLiteral("-") + requested;
            qDebug() << "Portage: Requested exact version:" << exact;
            m_emergeRunner->checkDependencies(exact);
        } else {
            // Auto-select first available version if available
            QStringList availableVersions = m_resource->availableVersions();
            if (!availableVersions.isEmpty()) {
                QString autoVersion = availableVersions.first();
                QString exact = QStringLiteral("=") + atom + QStringLiteral("-") + autoVersion;
                qDebug() << "Portage: Auto-selected version:" << exact;
                m_emergeRunner->checkDependencies(exact);
            } else {
                m_emergeRunner->checkDependencies(atom);
            }
        }
    } else if (role() == RemoveRole) {
        // For removal, use atom without version
        // emerge --depclean works with package atom, not specific versions
        qDebug() << "Portage: Starting removal of" << atom;
        m_emergeRunner->uninstallPackage(atom);
    }
}

void PortageTransaction::onEmergeOutput(const QString &line)
{
    qDebug() << "Emerge:" << line;
    // You can emit this to UI for real-time log display
}

void PortageTransaction::onEmergeError(const QString &line)
{
    qWarning() << "Emerge error:" << line;
}

void PortageTransaction::onEmergeFinished(bool success, int exitCode)
{
    qDebug() << "Portage: Emerge finished, success:" << success << "exitCode:" << exitCode;
    
    if (success) {
        if (role() == InstallRole) {
            // Check if package was really installed before marking as Installed
            QPointer<PortageResource> res = m_resource;
            QTimer::singleShot(500, [res]() {
                if (!res) {
                    qWarning() << "Portage: Resource was deleted before delayed reload";
                    return;
                }
                
                // Check if package exists in /var/db/pkg
                QString atom = res->atom();
                QString varDbPath = QStringLiteral("/var/db/pkg/") + atom;
                QDir varDbDir(varDbPath);
                
                if (varDbDir.exists()) {
                    qDebug() << "Portage: Package" << atom << "was installed successfully";
                    res->setState(AbstractResource::Installed);
                    res->loadUseFlagInfo();
                    qDebug() << "Portage: Installation completed for" << res->packageName();
                } else {
                    // Package directory doesn't exist - check if there's a versioned directory
                    QString categoryPath = QStringLiteral("/var/db/pkg/") + atom.section(QLatin1Char('/'), 0, 0);
                    QDir categoryDir(categoryPath);
                    QString packageName = atom.section(QLatin1Char('/'), 1);
                    
                    QStringList installed = categoryDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    bool found = false;
                    for (const QString &dir : installed) {
                        if (dir.startsWith(packageName + QLatin1Char('-'))) {
                            qDebug() << "Portage: Found installed package" << dir << "for" << atom;
                            res->setState(AbstractResource::Installed);
                            res->loadUseFlagInfo();
                            found = true;
                            break;
                        }
                    }
                    
                    if (!found) {
                        qWarning() << "Portage: Package" << atom << "was not found in /var/db/pkg after installation";
                        res->setState(AbstractResource::None);
                    }
                }
            });
            
        } else if (role() == RemoveRole) {
            m_resource->setState(AbstractResource::None);
            m_resource->setInstalledVersion(QString());
            qDebug() << "Portage: Removal completed for" << m_resource->packageName();
        }
        setStatus(DoneStatus);
    } else {
        setStatus(DoneWithErrorStatus);
    }
}

void PortageTransaction::onDependenciesChecked(const EmergeRunner::EmergeResult &result)
{
    qDebug() << "Portage: Dependencies checked, success:" << result.success;
    qDebug() << "Portage: Dependencies count:" << result.dependencies.size();
    qDebug() << "Portage: Needs unmask:" << result.needsUnmask;
    
    if (result.needsUnmask) {
        handleUnmaskRequest(result);
        return;
    }
    
    if (!result.dependencies.isEmpty()) {
        qDebug() << "Dependencies to install:";
        for (const auto &dep : result.dependencies) {
            qDebug() << "  -" << dep.atom << dep.version;
        }
        // TODO: Show dependency confirmation dialog
    }
    
    // Proceed with actual installation
    QString atom;
    if (!result.dependencies.isEmpty()) {
        // Use the first dependency's full atom (includes version)
        atom = result.dependencies.first().atom;
    } else {
        // Fallback to resource atom if no dependencies found
        atom = m_resource->atom();
    }
    
    QStringList useFlags; // Get from resource if needed
    m_emergeRunner->installPackage(atom, useFlags);
}

void PortageTransaction::handleUnmaskRequest(const EmergeRunner::EmergeResult &result)
{
    qWarning() << "Package needs unmasking:" << result.maskedPackages;
    
    // Use the full atoms from emerge --pretend output
    if (result.maskedPackages.isEmpty()) {
        qWarning() << "No masked packages to unmask!";
        setStatus(DoneWithErrorStatus);
        return;
    }
    
    // TODO: Show dialog asking user permission to unmask
    // TODO: Add version selection dialog in Discover UI (default: latest ~amd64)
    // For now, auto-unmask all masked packages
    bool allSuccess = true;
    QString atomToInstall;  // Full versioned atom for installation
    
    for (const QString &maskedEntry : result.maskedPackages) {
        // Parse "=category/package-version ~amd64" into atom and keyword
        QStringList parts = maskedEntry.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        if (parts.isEmpty()) continue;
        
        QString atom = parts.first();  // =www-client/google-chrome-beta-143.0.7499.4
        QString keyword = parts.size() > 1 ? parts.at(1) : QStringLiteral("~amd64");
        
        // Remember the first (latest) versioned atom for installation
        if (atomToInstall.isEmpty()) {
            atomToInstall = atom;
        }
        
        qDebug() << "Auto-unmasking package:" << atom << "with keyword:" << keyword;
        if (!m_unmaskManager->unmaskPackage(atom, keyword)) {
            qWarning() << "Failed to unmask package:" << atom;
            allSuccess = false;
        }
    }
    
    if (allSuccess && !atomToInstall.isEmpty()) {
        qDebug() << "Package unmasked successfully, proceeding with installation of" << atomToInstall;
        // Use the exact versioned atom from emerge --pretend (e.g., =www-client/google-chrome-beta-143.0.7499.4)
        if (role() == InstallRole) {
            m_emergeRunner->installPackage(atomToInstall);
        } else if (role() == RemoveRole) {
            m_emergeRunner->uninstallPackage(atomToInstall);
        }
    } else {
        qWarning() << "Failed to unmask one or more packages";
        setStatus(DoneWithErrorStatus);
    }
}

void PortageTransaction::simulateProgress()
{
    m_progress += 5;
    setProgress(m_progress);
    
    if (m_progress >= 100) {
        finishTransaction();
    }
}

void PortageTransaction::finishTransaction()
{
    qDebug() << "Portage: Transaction finished";
    
    if (role() == InstallRole) {
        m_resource->setState(AbstractResource::Installed);
        m_resource->setInstalledVersion(m_resource->availableVersion());
    } else if (role() == RemoveRole) {
        m_resource->setState(AbstractResource::None);
        m_resource->setInstalledVersion(QString());
    }
    
    setStatus(DoneStatus);
}

#include "moc_PortageTransaction.cpp"
