cmake_minimum_required(VERSION 3.24 FATAL_ERROR)

include(FetchContent)

if (NOT TARGET Jolt::Jolt)
	set(DEBUG_RENDERER_IN_DISTRIBUTION ON CACHE BOOL "" FORCE)
	set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)
	FetchContent_Declare(Jolt
		GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
		GIT_TAG v5.3.0
		SOURCE_SUBDIR Build
	)
	FetchContent_MakeAvailable(Jolt)
endif()

