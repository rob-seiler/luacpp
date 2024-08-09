# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

project(lua)
set(CMAKE_CXX_STANDARD 99)
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

add_custom_target(lua_cleanSources
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${PROJECT_SOURCE_DIR}/src
    COMMENT "Removing include and src directories"
)
