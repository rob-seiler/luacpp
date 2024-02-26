cmake_minimum_required(VERSION 3.12)
add_subdirectory(./lua)

project(luacpp)
set(CMAKE_CXX_STANDARD 17)
add_library(${PROJECT_NAME} STATIC)

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
    PRIVATE
	${PROJECT_SOURCE_DIR}/include/luacpp
    INTERFACE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    ${PROJECT_NAME}
    PUBLIC
    lua
)

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/googletest)
    set(BUILD_GMOCK OFF)
    set(INSTALL_GTEST OFF)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        set(gtest_disable_pthreads ON) # there is a bug in cmake version >= 3.23 which detects pthreads in windows
    endif()
    add_subdirectory(extern/googletest)
    enable_testing()
    include_directories(${gtest_SOURCE_DIR}/include ${gtest_SOURCE_DIR})
    add_executable(${PROJECT_NAME}_test)

    file(
        GLOB_RECURSE
        TEST_SRC_FILES
        CONFIGURE_DEPENDS
        LIST_DIRECTORIES true
        ${PROJECT_SOURCE_DIR}/test/*.cpp
    )
    target_sources(${PROJECT_NAME}_test PRIVATE ${TEST_SRC_FILES})

    target_link_libraries(
        ${PROJECT_NAME}_test
        PRIVATE
        ${PROJECT_NAME}
        gtest
        gtest_main
    )

    include(GoogleTest)
    gtest_discover_tests(${PROJECT_NAME}_test)
else()
    message("No test project configured")
endif()
