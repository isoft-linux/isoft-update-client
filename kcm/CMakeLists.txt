add_definitions(-std=c++11 -DTRANSLATION_DOMAIN=\"${GETTEXT_PACKAGE}\")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(kcm_isoftupdate_PART_SRCS
    main.cpp
)

qt5_add_dbus_interface(kcm_isoftupdate_PART_SRCS
    ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.isoftlinux.Update.xml
    updategenerated
)

add_library(kcm_isoftupdate MODULE ${kcm_isoftupdate_PART_SRCS})

target_include_directories(kcm_isoftupdate PUBLIC "$<BUILD_INTERFACE:${Kdepasswd_BINARY_DIR}>")

target_link_libraries(kcm_isoftupdate
    KF5::KDELibs4Support
)

install(TARGETS kcm_isoftupdate DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kcm_isoftupdate.desktop DESTINATION ${SERVICES_INSTALL_DIR})
