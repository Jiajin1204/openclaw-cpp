cmake_minimum_required(VERSION 3.16)
project(OpenClawCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_CURRENT_SOURCE_DIR}/include
                   ${CMAKE_CURRENT_SOURCE_DIR}/src
                   ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json/include)

# 只编译 main 和基础文件
set(SOURCES
    src/main.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
)

add_executable(openclaw-cpp ${SOURCES})
target_link_libraries(openclaw-cpp Threads::Threads)