cmake_minimum_required(VERSION 3.15)
project(praat_wasm_yd)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# 设置 Praat 根目录（指向工作区根目录）
set(PRAAT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# 添加 extern 子目录（包含 fmt 等依赖）
add_subdirectory(${PRAAT_ROOT}/extern extern_build)

# 添加 Praat 库
add_subdirectory(${PRAAT_ROOT}/praat praat_build)

# 添加 WASM 主程序
add_subdirectory(src)