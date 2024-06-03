cmake_minimum_required(VERSION 3.10)
add_subdirectory(./lua)

project(luacpp)
add_library(${PROJECT_NAME} STATIC)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.28")
	option(USE_CPP20_MODULES "Compile with C++20 modules" OFF)
	if(USE_CPP20_MODULES)
		target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
		target_compile_definitions(${PROJECT_NAME} PUBLIC USE_CPP20_MODULES)

		# Add the .ixx files to the library
		target_sources(luacpp
			PUBLIC
			FILE_SET CXX_MODULES FILES
			${CMAKE_SOURCE_DIR}/modules/Debug.ixx
			${CMAKE_SOURCE_DIR}/modules/Type.ixx
			${CMAKE_SOURCE_DIR}/modules/TypeMismatchException.ixx
			${CMAKE_SOURCE_DIR}/modules/Basics.ixx
			${CMAKE_SOURCE_DIR}/modules/Generic.ixx
			${CMAKE_SOURCE_DIR}/modules/Table.ixx
			${CMAKE_SOURCE_DIR}/modules/Script.ixx
			${CMAKE_SOURCE_DIR}/modules/Literals.ixx
		)
	endif()
endif()

# If USE_CPP20_MODULES is not defined or is OFF, use C++17
if(NOT DEFINED USE_CPP20_MODULES OR NOT USE_CPP20_MODULES)
	target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
	file(
		GLOB_RECURSE
		SRC_FILES
		CONFIGURE_DEPENDS
		LIST_DIRECTORIES true
		${PROJECT_SOURCE_DIR}/src/*.cpp
	)
	target_sources(${PROJECT_NAME} PRIVATE ${SRC_FILES})

endif()

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
