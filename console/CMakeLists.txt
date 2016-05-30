include_directories(
    ${GLIB2_INCLUDE_DIRS} 
    ${GIO2_INCLUDE_DIRS}
    ${GIOUNIX_INCLUDE_DIRS}
    ${CMAKE_CURRENT_BINARY_DIR}
)

execute_process(COMMAND ${GDBUS_CODEGEN_EXECUTABLE} --generate-c-code
                        ${CMAKE_CURRENT_BINARY_DIR}/update-generated --c-namespace Update --interface-prefix org.isoftlinux.
                        ${CMAKE_CURRENT_SOURCE_DIR}/../data/org.isoftlinux.Update.xml
                        RESULT_VARIABLE codegen_failed)
if(codegen_failed)
    message(FATAL_ERROR "Error in generating code for update-generated using gdbus-codegen")
endif()

add_executable(isoft-update-console 
    main.c
    ${CMAKE_CURRENT_BINARY_DIR}/update-generated.c
)

target_link_libraries(isoft-update-console
    ${GLIB2_LIBRARIES}
    ${GIO2_LIBRARIES}
)

install(TARGETS isoft-update-console RUNTIME DESTINATION bin)
