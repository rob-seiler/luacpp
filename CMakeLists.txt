cmake_minimum_required(VERSION 3.10)

# Off by default: favors the embedded/single-binary deployment model — no
# shared libraries to ship, one artifact to distribute. Turn on when scripts
# need to load C modules at runtime via require().
#
# Implications when ON:
#   POSIX:   Lua stays static. The host executable gets ENABLE_EXPORTS
#            (-rdynamic on Linux) so dlopen'd modules resolve lua_* against
#            its exported symbols at load time.
#   Windows: Lua is built as a SHARED library (lua.dll). The host EXE and
#            all C modules link against the import library; the DLL must
#            be deployed alongside the application. Windows PE/COFF has no
#            equivalent to POSIX's process-wide symbol table, so a single
#            shared Lua image is the only clean way to share state.
option(LUACPP_ENABLE_CMODULE_LOADING
       "Allow Lua scripts to require() compiled C modules at runtime" OFF)

# Off by default to keep the toolchain bar low (CMake 3.10 / C++17). Turning
# this on additionally builds a C++20 named-module surface for the library —
# consumers can then write `import luacpp;` instead of including individual
# headers. The static library and its headers stay authoritative; the module
# is a thin re-export wrapper. Requires CMake >= 3.28 and a compiler with
# C++20 module support (Clang 16+, GCC 14+, MSVC 17.5+).
option(LUACPP_BUILD_MODULE
       "Build the C++20 named module 'luacpp' alongside the static library" OFF)

if(LUACPP_ENABLE_CMODULE_LOADING)
	# Unify runtime artifacts in build/bin so that lua.dll, the test EXE
	# and any plugin DLL land in the same directory — required on Windows
	# for the loader to find lua.dll without PATH manipulation.
	set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
	set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

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
	# module_consumer.cpp uses `import luacpp;` and only compiles as a
	# C++20 module consumer — it has no business in the gtest exe, which
	# is plain C++17 header-based code.
	list(FILTER TEST_SRC_FILES EXCLUDE REGEX "module_consumer\\.cpp$")
	target_sources(${PROJECT_NAME}_test PRIVATE ${TEST_SRC_FILES})

	target_link_libraries(
		${PROJECT_NAME}_test
		PRIVATE
		${PROJECT_NAME}
		gtest
		gtest_main
	)

	# Absolute path to the directory holding Lua test-fixture files
	# (valid.lua / syntax_error.lua / runtime_error.lua, consumed by
	# FileLoadingTest). Resolved at configure time so tests can be invoked
	# from any working directory.
	target_compile_definitions(${PROJECT_NAME}_test PRIVATE
		LUACPP_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/test/data"
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

	# Build the bundled UUID plugin used as the require()-based smoke
	# test. Only wired up when LUACPP_ENABLE_CMODULE_LOADING is ON —
	# without it there is no plugin loading mechanism to exercise.
	if(LUACPP_ENABLE_CMODULE_LOADING AND EXISTS ${PROJECT_SOURCE_DIR}/plugins/uuid/CMakeLists.txt)
		add_subdirectory(plugins/uuid)
		# On POSIX: ENABLE_EXPORTS makes the host EXE expose its lua_*
		# symbols so dlopen'd modules can resolve against them. Harmless
		# on Windows (where module loading goes through lua.dll instead).
		set_target_properties(${PROJECT_NAME}_test PROPERTIES ENABLE_EXPORTS ON)
		target_compile_definitions(${PROJECT_NAME}_test PRIVATE
			LUACPP_UUID_MODULE_DIR="$<TARGET_FILE_DIR:uuid_module>"
			LUACPP_HAVE_UUID_MODULE
		)
		add_dependencies(${PROJECT_NAME}_test uuid_module)
	endif()
else()
	message("No test project configured")
endif()

if(LUACPP_BUILD_MODULE)
	# Module support in CMake is a moving target; 3.28 is the first version
	# that handles CXX_MODULES FILE_SETs in a portable way.
	if(CMAKE_VERSION VERSION_LESS "3.28")
		message(FATAL_ERROR "LUACPP_BUILD_MODULE requires CMake 3.28 or newer "
		                    "(detected ${CMAKE_VERSION}).")
	endif()

	add_library(${PROJECT_NAME}_module STATIC)
	set_target_properties(${PROJECT_NAME}_module PROPERTIES
		CXX_STANDARD 20
		CXX_STANDARD_REQUIRED ON
		CXX_EXTENSIONS OFF
		CXX_SCAN_FOR_MODULES ON
	)
	target_sources(${PROJECT_NAME}_module
		PUBLIC FILE_SET CXX_MODULES FILES
		${CMAKE_CURRENT_SOURCE_DIR}/module/luacpp.cppm
	)
	# Re-exports the public headers — link against the static library so the
	# module surface ends up with the same symbol set consumers would get
	# from #include.
	target_link_libraries(${PROJECT_NAME}_module PUBLIC ${PROJECT_NAME})

	add_library(luacpp::module ALIAS ${PROJECT_NAME}_module)

	# Consumer-side smoke test: a separate TU that reaches the library only
	# through `import luacpp;` (no header includes). Validates that the
	# module surface actually resolves at link/run time — building the
	# `.cppm` itself only verifies the module definition is parseable.
	if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/module_consumer.cpp)
		add_executable(${PROJECT_NAME}_module_consumer
		               ${CMAKE_CURRENT_SOURCE_DIR}/test/module_consumer.cpp)
		set_target_properties(${PROJECT_NAME}_module_consumer PROPERTIES
			CXX_STANDARD 20
			CXX_STANDARD_REQUIRED ON
			CXX_EXTENSIONS OFF
			CXX_SCAN_FOR_MODULES ON
		)
		target_link_libraries(${PROJECT_NAME}_module_consumer
		                      PRIVATE luacpp::module)
		# enable_testing() may already have been called by the googletest
		# block above, but is idempotent — call it unconditionally so the
		# module smoke test runs even when the googletest submodule is absent.
		enable_testing()
		add_test(NAME Module.ConsumerSmokeTest
		         COMMAND ${PROJECT_NAME}_module_consumer)
	endif()
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