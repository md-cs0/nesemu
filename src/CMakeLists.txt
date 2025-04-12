add_executable(nesemu "main.c" "util.c" "nes.c" "cpu.c")
target_include_directories(nesemu PRIVATE CMAKE_CURRENT_SOURCE_DIR)

target_link_libraries(nesemu PRIVATE SDL2::SDL2)
if (TARGET SDL2::SDL2main)
    target_link_libraries(nesemu PRIVATE SDL2::SDL2main)
endif()

install(TARGETS nesemu DESTINATION bin)