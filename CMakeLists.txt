# mkdir build
# cd build
# cmake ../ -DCMAKE_BUILD_TYPE=(Release|Debug) (if using single-configuration generators)
# cmake --build . --config (Release|Debug) (if using multi-configuration generators)
# cmake --install . (--prefix path_to_dir)

cmake_minimum_required(VERSION 3.8)
project(nesemu)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED true)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR})

add_subdirectory(submodules/SDL)
add_subdirectory(src)