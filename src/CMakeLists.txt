add_executable(nesemu "main.c")
target_include_directories(nesemu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
install(TARGETS nesemu DESTINATION bin)