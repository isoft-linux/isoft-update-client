include_directories(
    ${LIBXML2_INCLUDE_DIRS}
    ${LIBLZMA_INCLUDE_DIRS}
    ${LIBTAR_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon
    ${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(isoft-update-server-tool
    main.c
    env.c
    upd.c
    config.c
    packlist.c
    bill.c
    database.c
    update-check.c
    update-create.c
    update-demo.c
    update-gen-repos.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/utils.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/archive.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../daemon/md5.c
)

target_link_libraries(isoft-update-server-tool
    ${LIBXML2_LIBRARIES}
    ${LIBLZMA_LIBRARIES}
    ${LIBTAR_LIBRARIES}
)

install(TARGETS isoft-update-server-tool RUNTIME DESTINATION bin)
