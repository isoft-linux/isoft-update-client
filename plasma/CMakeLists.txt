plasma_install_package(package org.kde.plasma.isoftupdate)

add_definitions(-std=c++11 -DTRANSLATION_DOMAIN=\"${GETTEXT_PACKAGE}\")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

install(FILES 
    ${CMAKE_CURRENT_SOURCE_DIR}/qmldir 
    DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/isoftupdate)

set(plasma_applet_isoftupdate_SRCS
    main.cpp
    isoftupdate.cpp
)

qt5_add_dbus_interface(plasma_applet_isoftupdate_SRCS 
    ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.isoftlinux.Update.xml
    updategenerated
)

add_library(plasma_applet_isoftupdate SHARED ${plasma_applet_isoftupdate_SRCS})

target_link_libraries(plasma_applet_isoftupdate
    KF5::KDELibs4Support
    Qt5::QuickWidgets
    Qt5::DBus
    Qt5::Xml
    KF5::Service
    PW::KWorkspace
)

install(TARGETS plasma_applet_isoftupdate
        DESTINATION ${QML_INSTALL_DIR}/org/kde/plasma/isoftupdate)
