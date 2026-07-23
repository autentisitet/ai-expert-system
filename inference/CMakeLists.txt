cmake_minimum_required(VERSION 3.15)
project(Inference VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================
# 平台检测
# ============================================================

if(WIN32)
    set(PLATFORM_NAME "Windows")
    
    if(MSVC)
        add_compile_definitions(_CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN)
        add_compile_options(/MP)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    endif()
    
elseif(UNIX)
    set(PLATFORM_NAME "Unix/Linux")
    
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(PLATFORM_NAME "Linux")
        add_compile_definitions(LINUX)
    endif()
    
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        add_compile_options(-Wall -Wextra -pedantic)
    endif()
    
else()
    message(WARNING "未知平台，请确保编译环境正确")
endif()

message(STATUS "检测到平台: ${PLATFORM_NAME}")

# ============================================================
# 编译选项
# ============================================================

option(BUILD_SHARED_LIBS "Build shared library instead of static" OFF)
option(ENABLE_IPO "Enable interprocedural optimization (LTO)" ON)

if(ENABLE_IPO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(WARNING "IPO/LTO 不支持: ${ipo_error}")
    endif()
endif()

# ============================================================
# 源文件
# ============================================================

set(INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)

set(SOURCES
    ${SRC_DIR}/conflict/ConflictResolver.cpp
    #TODO: 添加更多源文件
)

# ============================================================
# 创建库
# ============================================================

add_library(expert_inference ${SOURCES})

target_include_directories(expert_inference
    PUBLIC
        ${INCLUDE_DIR}
)

# ============================================================
# 链接库
# ============================================================

if(UNIX)
    find_package(Threads REQUIRED)
    target_link_libraries(expert_inference PUBLIC Threads::Threads)
endif()

# ============================================================
# 安装
# ============================================================

install(TARGETS expert_inference
    EXPORT InferenceTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${INCLUDE_DIR}/
    DESTINATION include
    FILES_MATCHING PATTERN "*.hpp"
)

install(EXPORT InferenceTargets
    DESTINATION lib/cmake/Inference
    NAMESPACE Inference::
)

# ============================================================
# 配置摘要
# ============================================================

message(STATUS "========================================")
message(STATUS "Inference 库配置摘要:")
message(STATUS "  平台: ${PLATFORM_NAME}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  共享库: ${BUILD_SHARED_LIBS}")
message(STATUS "  LTO: ${ENABLE_IPO}")
message(STATUS "========================================")