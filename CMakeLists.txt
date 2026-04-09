cmake_minimum_required(VERSION 3.16)
project(OpenClawCpp VERSION 1.0.0 LANGUAGES CXX)

# Android NDK support
if(ANDROID)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++")
    add_definitions(-DANDROID)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Threads REQUIRED)
find_package(CURL QUIET)

if(CURL_FOUND AND NOT ANDROID)
    add_definitions(-DUSE_CURL)
    include_directories(${CURL_INCLUDE_DIRS})
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                   ${CMAKE_CURRENT_SOURCE_DIR}/include
                   ${CMAKE_CURRENT_SOURCE_DIR}/src
                   ${CMAKE_CURRENT_SOURCE_DIR}/third_party/nlohmann_json/include)

# Android: build simple chat client only (no curl/openssl needed)
if(ANDROID)
    # Unix socket client source
    set(SOCKET_CLIENT_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/unix_socket_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/logger.cpp
    )

    add_executable(openclaw-chat ${CMAKE_CURRENT_SOURCE_DIR}/src/simple_chat.cpp ${SOCKET_CLIENT_SOURCES})
    target_link_libraries(openclaw-chat Threads::Threads)

    # Also build the regular version but without model_client (for reference)
    set(SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/client/unix_socket_client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/session/session_manager.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/config.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/utils/logger.cpp
    )
    add_executable(openclaw-cpp ${SOURCES})
    target_link_libraries(openclaw-cpp Threads::Threads)
else()
    # Desktop: build full version
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
endif()