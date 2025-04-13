add_executable(nesemu "main.c" "util.c" "bus.c" "cpu.c")
target_include_directories(nesemu PRIVATE CMAKE_CURRENT_SOURCE_DIR)

add_custom_command(TARGET nesemu POST_BUILD
    COMMAND cp ${PROJECT_SOURCE_DIR}/wozmon/wozmon.bin ${PROJECT_BINARY_DIR}/wozmon.bin
)

install(TARGETS nesemu DESTINATION bin)