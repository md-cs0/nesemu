add_subdirectory(mappers)

add_executable(nesemu "main.c" "util.c" "nes.c" "cpu.c" "ppu.c" "cartridge.c")
target_include_directories(nesemu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(nesemu PUBLIC nesemu_mappers)
target_include_directories(nesemu_mappers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(nesemu PRIVATE SDL2::SDL2)
if (TARGET SDL2::SDL2main)
    target_link_libraries(nesemu PRIVATE SDL2::SDL2main)
endif()

install(TARGETS nesemu DESTINATION ${CMAKE_INSTALL_BINDIR})