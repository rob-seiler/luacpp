cmake_minimum_required(VERSION 3.10)

add_subdirectory(./lua)

project(luacpp)
set(CMAKE_CXX_STANDARD 17)
add_library(${PROJECT_NAME})

file(
    GLOB_RECURSE
    SRC_FILES
    CONFIGURE_DEPENDS
    LIST_DIRECTORIES true
    ${PROJECT_SOURCE_DIR}/src/*.cpp
)
target_sources(${PROJECT_NAME} PRIVATE ${SRC_FILES})

target_include_directories(
    ${PROJECT_NAME}
    PUBLIC
	${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    lua
)
