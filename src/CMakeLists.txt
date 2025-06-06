cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

add_library(mm_jolt INTERFACE
	./mm_jolt/utils.hpp

	./mm_jolt/components/jolt_context.hpp
	./mm_jolt/components/jolt_bodyid.hpp
)

target_include_directories(mm_jolt INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(mm_jolt INTERFACE cxx_std_20)

target_link_libraries(mm_jolt INTERFACE
	engine
	common_components
	Jolt::Jolt
)

########################################

add_library(mm_jolt_debug_renderer
	./mm_jolt/render_tasks/jolt_debug.hpp
	./mm_jolt/render_tasks/jolt_debug.cpp
)

target_include_directories(mm_jolt_debug_renderer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(mm_jolt_debug_renderer PUBLIC cxx_std_20)

target_link_libraries(mm_jolt_debug_renderer
	engine
	opengl_renderer_s
	common_components
	Jolt::Jolt
	mm_jolt
)


