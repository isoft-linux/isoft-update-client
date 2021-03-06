project(isoftupdateclient)
cmake_minimum_required(VERSION 2.8.12)

if(NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
    set(CMAKE_INSTALL_SYSCONFDIR "/etc")
endif()
if(NOT DEFINED CMAKE_INSTALL_LOCALSTATEDIR)
    set(CMAKE_INSTALL_LOCALSTATEDIR "/var")
endif()

set(PROJECT_NAME "isoftupdateclient")
set(PROJECT_VERSION "1.0.5")
set(GETTEXT_PACKAGE "plasma_applet_org.kde.plasma.isoftupdate")
set(PROJECT_LOCALEDIR ${CMAKE_INSTALL_PREFIX}/share/locale)
set(PROJECT_CONF_FILE ${CMAKE_INSTALL_SYSCONFDIR}/isoft-update.conf)
set(UPDATE_CONFDIR ${CMAKE_INSTALL_SYSCONFDIR}/update)
set(UPDATE_UPDATES_CONFDIR ${UPDATE_CONFDIR}/updates)
set(DOWNLOAD_DIR ${CMAKE_INSTALL_LOCALSTATEDIR}/cache/isoft-update)

add_definitions(-Werror)
add_definitions("-DPROJECT_NAME=\"${PROJECT_NAME}\"")
add_definitions("-DPROJECT_VERSION=\"${PROJECT_VERSION}\"")
add_definitions("-DGETTEXT_PACKAGE=\"${GETTEXT_PACKAGE}\"")
add_definitions("-DPROJECT_LOCALEDIR=\"${PROJECT_LOCALEDIR}\"")
add_definitions("-DPROJECT_CONF_FILE=\"${PROJECT_CONF_FILE}\"")
add_definitions("-DUPDATE_CONFDIR=\"${UPDATE_CONFDIR}\"")
add_definitions("-DUPDATE_UPDATES_CONFDIR=\"${UPDATE_UPDATES_CONFDIR}\"")
add_definitions("-DDOWNLOAD_DIR=\"${DOWNLOAD_DIR}\"")

option(ENABLE_DEBUG "Enable debug" OFF)
if(ENABLE_DEBUG)
    add_definitions(-g -DDEBUG)
endif()

option(ENABLE_TEST "Enable testcase" OFF)

find_program(GDBUS_CODEGEN_EXECUTABLE NAMES gdbus-codegen DOC "gdbus-codegen executable")
if(NOT GDBUS_CODEGEN_EXECUTABLE)
    message(FATAL_ERROR "Executable gdbus-codegen not found")
endif()

find_package(ECM 1.6.0 REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
find_package(PkgConfig REQUIRED)
if(NOT NO_SYSTEMD)
    pkg_check_modules(SYSTEMD "systemd")
endif()

if(SYSTEMD_FOUND)
    add_definitions(-DHAVE_SYSTEMD)
    set(CMAKE_AUTOMOC_MOC_OPTIONS -DHAVE_SYSTEMD)

    if (NOT DEFINED SYSTEMD_SYSTEM_UNIT_DIR)
        execute_process(COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=systemdsystemunitdir systemd OUTPUT_VARIABLE SYSTEMD_SYSTEM_UNIT_DIR)
        string(REGEX REPLACE "[ \t\n]+" "" SYSTEMD_SYSTEM_UNIT_DIR ${SYSTEMD_SYSTEM_UNIT_DIR})
    endif()
endif()

pkg_check_modules(GLIB2 REQUIRED glib-2.0)
pkg_check_modules(GIO2 REQUIRED gio-2.0)
pkg_check_modules(GIOUNIX REQUIRED gio-unix-2.0)
pkg_check_modules(NM REQUIRED NetworkManager)
pkg_check_modules(LIBCURL REQUIRED libcurl)
pkg_check_modules(LIBXML2 REQUIRED libxml-2.0)
pkg_check_modules(LIBLZMA REQUIRED liblzma)
pkg_check_modules(RPM REQUIRED rpm)
pkg_check_modules(POPT REQUIRED popt)

find_package(libtar REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets QuickWidgets DBus Xml)
find_package(KF5 REQUIRED COMPONENTS KDELibs4Support Plasma I18n)
find_package(LibKWorkspace CONFIG REQUIRED)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMInstallIcons)
include(ECMMarkAsTest)
include(FeatureSummary)

add_subdirectory(daemon)
add_subdirectory(console)
add_subdirectory(kcm)
add_subdirectory(kcm_viewer)
add_subdirectory(plasma)
add_subdirectory(data)
if(ENABLE_TEST)
add_subdirectory(test)
endif()
add_subdirectory(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
