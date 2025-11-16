set(portage-backend_SRCS
    backend/PortageBackend.cpp
    backend/PortageQmlInjector.cpp
    resources/PortageResource.cpp
    transaction/PortageTransaction.cpp
    resources/PortageUseFlags.cpp
    auth/PortageAuthClient.cpp
    repository/PortageRepositoryReader.cpp
    installed/PortageInstalledReader.cpp
    emerge/EmergeRunner.cpp
    emerge/UnmaskManager.cpp
    portageui.qrc
)

ecm_qt_declare_logging_category(portage-backend_SRCS 
    HEADER libdiscover_portage_debug.h 
    IDENTIFIER LIBDISCOVER_PORTAGE_LOG 
    CATEGORY_NAME org.kde.plasma.libdiscover.portage 
    DESCRIPTION "libdiscover portage backend" 
    EXPORT DISCOVER
)

add_executable(portage_backend_helper 
    auth/PortageAuthHelper.cpp
    auth/PortageAuthHelper.h
    config/org.kde.discover.portagebackend.policy
)
set_source_files_properties(
    config/org.kde.discover.portagebackend.policy
    PROPERTIES HEADER_FILE_ONLY ON
)
set_target_properties(portage_backend_helper PROPERTIES
    AUTOMOC ON
)
target_link_libraries(portage_backend_helper
    KF6::AuthCore
    Qt::Core
)

kauth_install_actions(org.kde.discover.portagebackend config/org.kde.discover.portagebackend.policy)
kauth_install_helper_files(portage_backend_helper org.kde.discover.portagebackend root)

install(TARGETS portage_backend_helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})

# Install DBus service file
install(FILES config/org.kde.discover.portagebackend.service 
    DESTINATION ${KDE_INSTALL_DBUSDIR}/system-services)

# Install DBus policy file
install(FILES config/org.kde.discover.portagebackend.conf 
    DESTINATION ${KDE_INSTALL_SYSCONFDIR}/dbus-1/system.d)

kcoreaddons_add_plugin(portage-backend SOURCES ${portage-backend_SRCS} INSTALL_NAMESPACE "discover")

target_link_libraries(portage-backend
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
        Qt::Qml
        Qt::Quick
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::I18n
        KF6::AuthCore
        Discover::Common
)

