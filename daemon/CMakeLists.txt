include_directories(
    ${GLIB2_INCLUDE_DIRS} 
    ${GIO2_INCLUDE_DIRS}
    ${GIOUNIX_INCLUDE_DIRS}
    ${NM_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
    ${LIBXML2_INCLUDE_DIRS}
    ${LIBLZMA_INCLUDE_DIRS}
    ${LIBTAR_INCLUDE_DIRS}
    ${RPM_INCLUDE_DIRS}
    ${POPT_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

execute_process(COMMAND ${GDBUS_CODEGEN_EXECUTABLE} --generate-c-code
                        ${CMAKE_CURRENT_BINARY_DIR}/update-generated --c-namespace Update --interface-prefix org.isoftlinux.
                        ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.isoftlinux.Update.xml
                        RESULT_VARIABLE codegen_failed)
if(codegen_failed)
    message(FATAL_ERROR "Error in generating code for update-generated using gdbus-codegen")
endif()

add_executable(isoft-update-daemon 
    main.c
    daemon.c
    download.c
    archive.c
    md5.c
    utils.c
    rpm-helper.c
    extensions.c
    ${CMAKE_CURRENT_BINARY_DIR}/update-generated.c
)

target_link_libraries(isoft-update-daemon
    ${GLIB2_LIBRARIES}
    ${GIO2_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBLZMA_LIBRARIES}
    ${LIBTAR_LIBRARIES}
    ${RPM_LIBRARIES}
    ${POPT_LIBRARIES}
)

install(TARGETS isoft-update-daemon RUNTIME DESTINATION bin)

add_executable(isoft-offline-update
    isoft-offline-update.c
    packagekit-glib2/pk-progress-bar.c
    utils.c
    md5.c
    rpm-helper.c
)
target_link_libraries(isoft-offline-update
    ${GLIB2_LIBRARIES}
    ${GIO2_LIBRARIES}
    ${LIBCURL_LIBRARIES}
    ${LIBXML2_LIBRARIES}
    ${LIBLZMA_LIBRARIES}
    ${LIBTAR_LIBRARIES}
    ${RPM_LIBRARIES}
    ${POPT_LIBRARIES}
    systemd
)

