# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

project(lua)
set(CMAKE_CXX_STANDARD 99)
add_library(${PROJECT_NAME})

file(
    GLOB_RECURSE
    SRC_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES true
    ${PROJECT_SOURCE_DIR}/src/*.c
)
list(REMOVE_ITEM SRC_FILES "main.c") # standalone lua interpreter
list(REMOVE_ITEM SRC_FILES "luac.c") # standalone lua compiler
target_sources(${PROJECT_NAME} PRIVATE ${SRC_FILES})

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
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
