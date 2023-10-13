IF(ENABLE_EXAMPLES)
  INCLUDE_DIRECTORIES(${PROJ_SOURCE_DIR}/src ${PROJ_SOURCE_DIR}/models ${PROJ_BINARY_DIR}/src)

  add_executable(geometry geometry.cpp )
  TARGET_LINK_LIBRARIES(geometry orocos-kdl)
  
  add_executable(trajectory_example trajectory_example.cpp )
  TARGET_LINK_LIBRARIES(trajectory_example orocos-kdl)
  
  add_executable(chainiksolverpos_lma_demo chainiksolverpos_lma_demo.cpp )
  TARGET_LINK_LIBRARIES(chainiksolverpos_lma_demo orocos-kdl orocos-kdl-models)

ENDIF(ENABLE_EXAMPLES)  

