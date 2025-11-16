/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#include "UseFlagsDialog.h"
#include "../resources/PortageUseFlags.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QFile>
#include <QRegularExpression>
#include <QDebug>
#include <KLocalizedString>

UseFlagsDialog::UseFlagsDialog(const QString &packageAtom, const QString &version, QWidget *parent)
    : QDialog(parent)
    , m_packageAtom(packageAtom)
    , m_version(version)
{
    setWindowTitle(i18n("Configure USE Flags - %1", packageAtom));
    setMinimumSize(600, 500);
    
    setupUI();
    loadUseFlags();
}

void UseFlagsDialog::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    
    // Header
    auto *headerLabel = new QLabel(i18n("Select USE flags for <b>%1-%2</b>", m_packageAtom, m_version));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    // Scroll area for flags
    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    auto *scrollWidget = new QWidget;
    m_flagsLayout = new QVBoxLayout(scrollWidget);
    m_flagsLayout->setSpacing(4);
    
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea, 1);
    
    // Custom flags input
    auto *customGroup = new QGroupBox(i18n("Custom USE Flags"));
    auto *customLayout = new QVBoxLayout(customGroup);
    
    m_customFlagsInput = new QLineEdit;
    m_customFlagsInput->setPlaceholderText(i18n("Example: newflag -disableflag"));
    customLayout->addWidget(m_customFlagsInput);
    
    auto *hintLabel = new QLabel(i18n("<b>Note:</b> Checked flags above will be <i>enabled</i>. Unchecked flags are omitted (global USE applies).<br>"
                                      "To explicitly <i>disable</i> a flag, add it here with '-' prefix (e.g., -flag). Separate with spaces."));
    hintLabel->setWordWrap(true);
    QFont hintFont = hintLabel->font();
    hintFont.setPointSize(hintFont.pointSize() - 1);
    hintLabel->setFont(hintFont);
    customLayout->addWidget(hintLabel);
    
    mainLayout->addWidget(customGroup);
    
    // Buttons
    auto *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    
    auto *okButton = new QPushButton(i18n("OK"));
    okButton->setDefault(true);
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(okButton);
    
    auto *cancelButton = new QPushButton(i18n("Cancel"));
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
}

void UseFlagsDialog::loadUseFlags()
{
    qDebug() << "UseFlagsDialog: Loading USE flags for" << m_packageAtom << "version" << m_version;
    
    PortageUseFlags useFlags;
    
    // Check if package is installed
    bool isInstalled = QFile::exists(QStringLiteral("/var/db/pkg/%1-%2").arg(m_packageAtom, m_version));
    
    // Compute effective USE flags from all sources (make.conf, IUSE defaults, package.use, installed)
    PortageUseFlags::EffectiveUseFlags effective = useFlags.computeEffectiveUseFlags(m_packageAtom, m_version, isInstalled);
    
    qDebug() << "UseFlagsDialog: IUSE flags:" << effective.iuse;
    qDebug() << "UseFlagsDialog: Enabled flags:" << effective.enabled;
    qDebug() << "UseFlagsDialog: Disabled flags:" << effective.disabled;
    
    // Create checkboxes for all IUSE flags
    if (!effective.iuse.isEmpty()) {
        auto *iuseGroup = new QGroupBox(i18n("Standard USE Flags"));
        auto *iuseLayout = new QVBoxLayout(iuseGroup);
        
        for (const QString &flag : effective.iuse) {
            auto *checkbox = new QCheckBox(flag);
            checkbox->setChecked(effective.enabled.contains(flag));
            
            QString desc = effective.descriptions.value(flag);
            if (!desc.isEmpty()) {
                checkbox->setToolTip(desc);
            }
            
            iuseLayout->addWidget(checkbox);
            
            UseFlagCheckbox ufcb;
            ufcb.checkbox = checkbox;
            ufcb.description = desc;
            ufcb.isExpanded = false;
            m_flagCheckboxes[flag] = ufcb;
        }
        
        m_flagsLayout->addWidget(iuseGroup);
    }
}

QStringList UseFlagsDialog::getSelectedFlags() const
{
    QStringList result;
    
    // Get flags from checkboxes - only write enabled flags (without "-")
    // Disabled flags are simply omitted to avoid overriding global USE
    for (auto it = m_flagCheckboxes.constBegin(); it != m_flagCheckboxes.constEnd(); ++it) {
        const QString &flagName = it.key();
        const UseFlagCheckbox &ufcb = it.value();
        
        if (ufcb.checkbox->isChecked()) {
            result << flagName;
        }
    }
    
    // Add custom flags (user can specify -flag here if needed)
    QString customFlags = m_customFlagsInput->text().trimmed();
    if (!customFlags.isEmpty()) {
        QStringList customList = customFlags.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        result.append(customList);
    }
    
    return result;
}
