/*
 *   SPDX-FileCopyrightText: 2025 keklick1337 <gentoo@trustcrypt.com>
 *   SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
 */

#pragma once

namespace PortagePaths
{
    // Base directories
    constexpr const char* ETC_PORTAGE = "/etc/portage";
    constexpr const char* VAR_LIB_PORTAGE = "/var/lib/portage";
    
    // Configuration files
    constexpr const char* MAKE_CONF = "/etc/portage/make.conf";
    constexpr const char* PACKAGE_USE = "/etc/portage/package.use";
    constexpr const char* PACKAGE_ACCEPT_KEYWORDS = "/etc/portage/package.accept_keywords";
    constexpr const char* PACKAGE_MASK = "/etc/portage/package.mask";
    constexpr const char* PACKAGE_LICENSE = "/etc/portage/package.license";
    
    // Database paths
    constexpr const char* PKG_DB = "/var/db/pkg";
    constexpr const char* REPOS_DB = "/var/db/repos";
    constexpr const char* WORLD_FILE = "/var/lib/portage/world";
    
    // Default repository
    constexpr const char* DEFAULT_REPO = "gentoo";
}
