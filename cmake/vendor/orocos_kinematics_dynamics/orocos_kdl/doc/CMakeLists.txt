
CONFIGURE_FILE("${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in" "${CMAKE_CURRENT_BINARY_DIR}/Doxyfile" IMMEDIATE @ONLY)
ADD_CUSTOM_TARGET(docs "doxygen" "Doxyfile")
INSTALL(FILES ${CMAKE_CURRENT_BINARY_DIR}/kdl.tag DESTINATION share/doc/liborocos-kdl/ OPTIONAL) # only installs if found.
