add_executable(nesemu "main.c")
target_include_directories(nesemu PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(nesemu PRIVATE SDL2)
if (WIN32)
    target_link_libraries(nesemu PRIVATE SDL2main)
endif()

install(TARGETS nesemu DESTINATION bin)