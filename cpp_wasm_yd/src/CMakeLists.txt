cmake_minimum_required(VERSION 3.15)
project(praat_wasm)

# C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

# 设置 Praat 根目录
get_filename_component(PRAAT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/.. ABSOLUTE)

# 创建 WASM 可执行文件
add_executable(praat_wasm main.cpp)

# Emscripten 编译设置
if (EMSCRIPTEN)
	# WASM 输出设置
	set(CMAKE_EXECUTABLE_SUFFIX ".html")

	# 链接选项
	set_target_properties(praat_wasm PROPERTIES
		LINK_FLAGS "--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/shell.html -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s MODULARIZE=1 -s EXPORT_NAME='PraatWasm' -s EXPORTED_RUNTIME_METHODS=['FS','cwrap','UTF8ToString','getValue','allocateUTF8','HEAP32','HEAPF32'] -s EXPORTED_FUNCTIONS=['_init_praat','_load_sound_from_memory','_load_sound','_get_sound_samples','_get_sound_time_axis','_get_sound_info','_get_spectrum','_get_spectrogram','_get_pitch','_malloc','_free'] -sNO_DISABLE_EXCEPTION_CATCHING"
	)
endif ()

# 设置 pra目标和fmt目标的属性
set_target_properties(praat PROPERTIES
	POSITION_INDEPENDENT_CODE TRUE
	C_VISIBILITY_PRESET hidden
	CXX_VISIBILITY_PRESET hidden
)

# 添加 Praat 头文件路径
target_include_directories(praat SYSTEM INTERFACE ${PRAAT_ROOT})
target_include_directories(praat_wasm PRIVATE ${PRAAT_ROOT}/praat)

# 链接 Praat 库
target_link_libraries(praat_wasm PRIVATE praat)
