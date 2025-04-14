add_executable(nesemu "main.c" "util.c" "bus.c" "cpu.c")
target_include_directories(nesemu PRIVATE CMAKE_CURRENT_SOURCE_DIR)

add_custom_command(TARGET nesemu POST_BUILD
    COMMAND cp ${PROJECT_SOURCE_DIR}/wozmon/wozmon.bin ${PROJECT_BINARY_DIR}/wozmon.bin
)

target_link_libraries(nesemu PRIVATE SDL2::SDL2)
if (TARGET SDL2::SDL2main)
    target_link_libraries(nesemu PRIVATE SDL2::SDL2main)
endif()

install(TARGETS nesemu DESTINATION bin)