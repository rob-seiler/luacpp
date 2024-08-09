cmake_minimum_required(VERSION 3.10)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/lua)

project(luacpp)
add_library(${PROJECT_NAME} STATIC)

function(supportsModules result)
	#first check if the version is greater than 3.28 if not return false
	if (${CMAKE_VERSION} VERSION_LESS "3.28")
		set(${result} FALSE PARENT_SCOPE)
		return()
	endif()

	#check for compiler version
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		#currently this implementation doesnt compile with gcc and c++20 modules
		#if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "14.0")
			set(${result} FALSE PARENT_SCOPE)
			return()
		#endif()
	elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
		#currently this implementation doesnt compile with clang and c++20 modules
		#if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "16.0")
			set(${result} FALSE PARENT_SCOPE)
			return()
		#endif()
	elseif (MSVC)
		if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS "13.34")
			set(${result} FALSE PARENT_SCOPE)
			return()
		endif()
	else()
		message(FATAL_ERROR "unsupported compiler")
		return()
	endif()
	set(${result} TRUE PARENT_SCOPE)
endfunction()

supportsModules(MODULES_SUPPORTED)

if (${MODULES_SUPPORTED})
	option(USE_CPP20_MODULES "Compile with C++20 modules" OFF)
	if(USE_CPP20_MODULES)
		target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_20)
		target_compile_definitions(${PROJECT_NAME} PUBLIC USE_CPP20_MODULES)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			target_compile_options(${PROJECT_NAME} PRIVATE -fmodules-ts)
		elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			target_compile_options(${PROJECT_NAME} PRIVATE -fmodules)
		endif()

		# Add the .ixx files to the library
		target_sources(luacpp
			PUBLIC
			FILE_SET CXX_MODULES FILES
			${CMAKE_SOURCE_DIR}/modules/Version.ixx
			${CMAKE_SOURCE_DIR}/modules/Type.ixx
			${CMAKE_SOURCE_DIR}/modules/TypeMismatchException.ixx
			${CMAKE_SOURCE_DIR}/modules/Basics.ixx
			${CMAKE_SOURCE_DIR}/modules/Generic.ixx
			${CMAKE_SOURCE_DIR}/modules/Debug.ixx
			${CMAKE_SOURCE_DIR}/modules/Table.ixx
			${CMAKE_SOURCE_DIR}/modules/Registry.ixx
			${CMAKE_SOURCE_DIR}/modules/State.ixx
			${CMAKE_SOURCE_DIR}/modules/Literals.ixx
		)
	endif()
endif()

# If USE_CPP20_MODULES is not defined or is OFF, use C++17
if(NOT ${MODULES_SUPPORTED} OR NOT ${USE_CPP20_MODULES})
	if (NOT ${MODULES_SUPPORTED} AND ${USE_CPP20_MODULES})
		message(FATAL_ERROR "C++20 modules are not supported by your configuration. Please build without the option USE_CPP20_MODULES or upgrade your compiler/cmake.")
	endif()

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

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra)
endif()

if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/extern/googletest/CMakeLists.txt)
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

	if (${MODULES_SUPPORTED} AND USE_CPP20_MODULES)
		if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
			target_compile_options(${PROJECT_NAME} PRIVATE -fmodules-ts)
		elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
			target_compile_options(${PROJECT_NAME} PRIVATE -fmodules)
		endif()
	endif()

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

option(BUILD_EXAMPLES "Build examples" ON)
if(BUILD_EXAMPLES)
	SET (EXAMPLEDIR ${CMAKE_CURRENT_SOURCE_DIR}/examples)

	# Get a list of all subdirectories
	file(GLOB children RELATIVE ${EXAMPLEDIR} ${EXAMPLEDIR}/*)
	foreach(child ${children})
		if(IS_DIRECTORY ${EXAMPLEDIR}/${child})
			SET (EXENAME LuaExample_${child})
			file(GLOB src_files "${EXAMPLEDIR}/${child}/*.cpp")
			if(src_files)
				add_executable(${EXENAME} ${src_files})
				target_link_libraries(
					${EXENAME}
					PRIVATE
					${PROJECT_NAME}
				)
			endif()
		endif()
	endforeach()
endif()