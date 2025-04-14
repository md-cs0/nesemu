add_library(nesemu_mappers STATIC "mapper_base.c") 
target_include_directories(nesemu_mappers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

install(TARGETS nesemu_mappers DESTINATION ${CMAKE_INSTALL_LIBDIR})