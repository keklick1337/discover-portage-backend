set(portage-backend_SRCS
    PortageBackend.cpp
    PortageResource.cpp
    PortageTransaction.cpp
    PortageUseFlags.cpp
    repository/PortageRepositoryReader.cpp
    installed/PortageInstalledReader.cpp
    portageui.qrc
)

ecm_qt_declare_logging_category(portage-backend_SRCS 
    HEADER libdiscover_portage_debug.h 
    IDENTIFIER LIBDISCOVER_PORTAGE_LOG 
    CATEGORY_NAME org.kde.plasma.libdiscover.portage 
    DESCRIPTION "libdiscover portage backend" 
    EXPORT DISCOVER
)

kcoreaddons_add_plugin(portage-backend SOURCES ${portage-backend_SRCS} INSTALL_NAMESPACE "discover")

target_link_libraries(portage-backend
    PRIVATE
        Qt::Core
        Qt::Widgets
        Qt::Xml
        KF6::CoreAddons
        KF6::ConfigCore
        KF6::I18n
        Discover::Common
)
