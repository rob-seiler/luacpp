cmake_minimum_required(VERSION 3.10)
add_subdirectory(${CMAKE_SOURCE_DIR}/extern/lua)

project(luacpp)
add_library(${PROJECT_NAME} STATIC)

# Use C++17
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
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

if(MSVC)
	target_compile_options(${PROJECT_NAME} PRIVATE /W4)
	# MSVC's default /EHsc implies /EHc — the compiler assumes extern "C"
	# functions never throw and skips unwind tables at their call sites.
	# Since Lua's public API is now declared extern "C" (so LuaRocks C
	# modules can link against us, see extern/lua/CMakeLists.txt) yet still
	# throws C++ exceptions on OOM under our C++-compiled Lua, those unwind
	# tables ARE required — otherwise local C++ destructors (notably the
	# Guard in State::anchorOwned) do not run when Lua throws.
	#
	# Appending /EHs after the default /EHsc makes MSVC honor the last /EH
	# flag, dropping the /EHc assumption for this target.
	target_compile_options(${PROJECT_NAME} PRIVATE /EHs)
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

	target_link_libraries(
		${PROJECT_NAME}_test
		PRIVATE
		${PROJECT_NAME}
		gtest
		gtest_main
	)

	include(GoogleTest)
	gtest_discover_tests(${PROJECT_NAME}_test)

	add_test(NAME ${PROJECT_NAME}_test COMMAND ${PROJECT_NAME}_test)

	# C-linkage smoke check: a pure-C TU that includes lua.h and links
	# against the lua target. Builds only if Lua's public API exports
	# C-linkage symbols — the contract LuaRocks-installed C modules
	# depend on. Regression guard for the extern "C" force-include in
	# extern/lua/CMakeLists.txt.
	if(EXISTS ${PROJECT_SOURCE_DIR}/test/c_linkage_check.c)
		add_executable(${PROJECT_NAME}_c_linkage_check test/c_linkage_check.c)
		target_link_libraries(${PROJECT_NAME}_c_linkage_check PRIVATE lua)
		add_test(NAME LuaCApi.LinkageAndRoundtrip
		         COMMAND ${PROJECT_NAME}_c_linkage_check)
	endif()
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
				file(GLOB lua_files "${EXAMPLEDIR}/${child}/*.lua")
				foreach(lf ${lua_files})
					add_custom_command(TARGET ${EXENAME} POST_BUILD
						COMMAND ${CMAKE_COMMAND} -E copy_if_different
						${lf} $<TARGET_FILE_DIR:${EXENAME}>)
				endforeach()
			endif()
		endif()
	endforeach()
endif()