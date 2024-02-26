# Specify the minimum version for CMake
cmake_minimum_required(VERSION 3.10)

project(lua)
set(CMAKE_CXX_STANDARD 99)
add_library(${PROJECT_NAME} STATIC)

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

# Copy all header files to the new directory
file(COPY ${HEADER_FILES} DESTINATION ${PROJECT_SOURCE_DIR}/include/lua)