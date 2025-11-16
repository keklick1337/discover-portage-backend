/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

#include <QDialog>
#include <QStringList>
#include <QMap>

class QCheckBox;
class QLineEdit;
class QVBoxLayout;

class UseFlagsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UseFlagsDialog(const QString &packageAtom, 
                           const QString &version,
                           QWidget *parent = nullptr);
    
    // Get selected USE flags (includes custom flags from text input)
    QStringList getSelectedFlags() const;
    
private:
    void loadUseFlags();
    void setupUI();
    
    QString m_packageAtom;
    QString m_version;
    
    struct UseFlagCheckbox {
        QCheckBox *checkbox;
        QString description;
        bool isExpanded; // L10N, etc
    };
    
    QMap<QString, UseFlagCheckbox> m_flagCheckboxes;
    QLineEdit *m_customFlagsInput;
    QVBoxLayout *m_flagsLayout;
};
