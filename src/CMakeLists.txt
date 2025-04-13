add_executable(nesemu "main.c" "util.c" "bus.c" "cpu.c")
target_include_directories(nesemu PRIVATE CMAKE_CURRENT_SOURCE_DIR)

install(TARGETS nesemu DESTINATION bin)