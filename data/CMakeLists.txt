install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/isoft-update.conf" DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/org.isoftlinux.Update.xml" DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbus-1/interfaces")

install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/org.isoftlinux.Update.conf" DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/dbus-1/system.d")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/org.isoftlinux.Update.service.in" "${CMAKE_CURRENT_BINARY_DIR}/org.isoftlinux.Update.service")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/org.isoftlinux.Update.service" DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/dbus-1/system-services")

if(SYSTEMD_FOUND)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/isoft-update-daemon.service.in" "${CMAKE_CURRENT_BINARY_DIR}/isoft-update-daemon.service")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/isoft-update-daemon.service" DESTINATION "${SYSTEMD_SYSTEM_UNIT_DIR}")
endif()
