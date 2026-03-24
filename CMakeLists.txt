cmake_minimum_required(VERSION 3.16)
project(OpenClawCpp VERSION 1.0.0 LANGUAGES CXX)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# 输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 寻找依赖
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 可选依赖 - 使用条件编译
option(USE_BOOST "Use Boost libraries (Beast for WebSocket)" OFF)
option(USE_NLOHMANN "Use nlohmann/json" OFF)

# ============ 依赖处理 ============

# Boost (如果启用)
if(USE_BOOST)
    find_package(Boost 1.74 REQUIRED COMPONENTS system thread)
    set(BOOST_LIBS Boost::system Boost::thread)
endif()

# nlohmann/json (如果启用)
if(USE_NLOHMANN)
    find_package(nlohmann_json 3.10 REQUIRED)
endif()

# ============ 源文件 ============

# 头文件目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# 源文件列表
set(SOURCES
    src/main.cpp
    src/utils/logger.cpp
    src/utils/config.cpp
    src/gateway/ws_server.cpp
    src/gateway/gateway.cpp
    src/session/session_manager.cpp
    src/agent/model_client.cpp
    src/agent/agent_loop.cpp
    src/tools/tool_engine.cpp
)

# 头文件列表
set(HEADERS
    include/openclaw/types.h
    include/openclaw/message.h
    include/openclaw/session.h
    src/utils/logger.h
    src/utils/config.h
    src/gateway/ws_server.h
    src/gateway/gateway.h
    src/session/session_manager.h
    src/agent/model_client.h
    src/agent/agent_loop.h
    src/tools/tool_engine.h
    src/tools/tool.h
)

# 创建可执行文件
add_executable(openclaw-cpp ${SOURCES} ${HEADERS})

# 链接库
target_link_libraries(openclaw-cpp 
    Threads::Threads 
    OpenSSL::SSL 
    OpenSSL::Crypto
    ${BOOST_LIBS}
)

# 安装
install(TARGETS openclaw-cpp DESTINATION bin)
install(DIRECTORY include/openclaw DESTINATION include)

# ============ 测试 ============
enable_testing()
add_subdirectory(tests)
