# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

project(lua)
set(CMAKE_CXX_STANDARD 17)
add_library(${PROJECT_NAME} STATIC)

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/src")
	message(STATUS "src directory not found. Extracting lua sources from package...")

	file(GLOB LUA_TARBALLS "${PROJECT_SOURCE_DIR}/lua*.tar.gz")
	if(LUA_TARBALLS)
		list(GET LUA_TARBALLS 0 LUA_TARBALL) #use the first tarball found
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E tar xzf ${LUA_TARBALL}
			WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
		)
		#move the src dir one level up
		string(REGEX REPLACE "\\.tar\\.gz$" "" EXTRACTED_DIR ${LUA_TARBALL})
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E rename ${EXTRACTED_DIR}/src ${PROJECT_SOURCE_DIR}/src
		)
		execute_process(
			COMMAND ${CMAKE_COMMAND} -E remove_directory ${EXTRACTED_DIR}
		)
	else()
		message(FATAL_ERROR "No lua*.tar.gz file found in ${PROJECT_SOURCE_DIR}")
	endif()
endif()

file(
	GLOB_RECURSE
	SRC_FILES
	CONFIGURE_DEPENDS
	LIST_DIRECTORIES true
	${PROJECT_SOURCE_DIR}/src/*.c
)
list(FILTER SRC_FILES EXCLUDE REGEX ".*main\.c") # standalone lua interpreter
list(FILTER SRC_FILES EXCLUDE REGEX ".*luac\.c") # standalone lua compiler
target_sources(${PROJECT_NAME} PRIVATE ${SRC_FILES})

# Force C++ compilation of Lua sources. This makes Lua errors propagate as
# C++ exceptions (luaconf.h: LUAI_THROW = throw when __cplusplus is defined),
# which is required for C++ destructors in binding code (notably the Guard
# in State::anchorOwned) to run reliably across platforms. Under C compilation
# Lua uses setjmp/longjmp; on Linux+g++/clang that bypasses destructors and
# turns an OOM in the middle of anchorOwned into use-after-free.
set_source_files_properties(${SRC_FILES} PROPERTIES LANGUAGE CXX)

target_include_directories(
	${PROJECT_NAME}
	INTERFACE
	${PROJECT_SOURCE_DIR}/include
)

if(UNIX AND NOT APPLE)
	target_compile_definitions(
		${PROJECT_NAME}
		PRIVATE
		LUA_USE_LINUX
	)
elseif(CMAKE_SYSTEM_NAME STREQUAL "QNX")
	target_compile_definitions(
		${PROJECT_NAME}
		PRIVATE
		LUA_USE_POSIX
		LUA_USE_READLINE
	)
else()
	target_compile_definitions(
		${PROJECT_NAME}
		PRIVATE
	)
endif()


#We didn't change the build structure lua provided but it isn't that good to work with when compiling lua as a library.
#Because of this we do some restructuring here.
file(GLOB HEADER_FILES "${PROJECT_SOURCE_DIR}/src/*.h*")
file(MAKE_DIRECTORY ${PROJECT_SOURCE_DIR}/include/lua)
file(COPY ${HEADER_FILES} DESTINATION ${PROJECT_SOURCE_DIR}/include/lua) # Copy all header files to the new directory

# Force the public Lua API to C linkage even though we compile Lua as C++.
# This makes Lua-as-C++ binary-compatible with external C consumers — most
# importantly LuaRocks-installed C modules, which include lua.hpp / extern "C"
# wrappers and expect to find unmangled symbols like 'lua_settop'.
#
# Mechanism: a force-included header that runs BEFORE every Lua TU. It first
# includes luaconf.h (so its include guard fires and its LUA_API definition
# lands), then undef/redefines LUA_API and its aliases to be extern "C". The
# .c source's own later '#include "luaconf.h"' is then a no-op (guard hit),
# so our overrides survive into all subsequent declarations and definitions.
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/lua_cxx_extern_c.h
"// Auto-generated. Force-included into every Lua TU when compiled as C++.
#include \"luaconf.h\"
#ifdef __cplusplus
#  undef LUA_API
#  define LUA_API extern \"C\"
#  undef LUALIB_API
#  define LUALIB_API extern \"C\"
#  undef LUAMOD_API
#  define LUAMOD_API extern \"C\"
#endif
")

# luaconf.h lives in src/, so the include path must reach it for the
# force-include's '#include \"luaconf.h\"' to resolve.
target_include_directories(${PROJECT_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/src)

target_compile_options(${PROJECT_NAME} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/FI${CMAKE_CURRENT_BINARY_DIR}/lua_cxx_extern_c.h>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-include${CMAKE_CURRENT_BINARY_DIR}/lua_cxx_extern_c.h>
)

add_custom_target(lua_cleanSources
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/src
    COMMENT "Removing include and src directories"
)
