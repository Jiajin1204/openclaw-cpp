cmake_minimum_required(VERSION 3.16)
project(OpenClawCpp VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)
find_package(CURL QUIET)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_CURRENT_SOURCE_DIR}/include
                   ${CMAKE_CURRENT_SOURCE_DIR}/src
                   ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json/include)

# 收集所有源文件（排除 tools.cpp.bak）
file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*/*.cpp
)
list(REMOVE_ITEM SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/tools.cpp.bak")

add_executable(openclaw-cpp ${SOURCES})

if(CURL_FOUND)
    target_link_libraries(openclaw-cpp Threads::Threads CURL::libcurl)
else()
    target_link_libraries(openclaw-cpp Threads::Threads)
endif()

find_package(OpenSSL QUIET)
if(OPENSSL_FOUND)
    target_link_libraries(openclaw-cpp OpenSSL::SSL OpenSSL::Crypto)
endif()