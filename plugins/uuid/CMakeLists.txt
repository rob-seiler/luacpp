# Mini UUID plugin for luacpp. Built as a SHARED Lua module.
#
# Symbol resolution at runtime:
#   POSIX:   the .so has UNRESOLVED lua_* references. The dynamic loader
#            resolves them at dlopen time against the host executable's
#            exported symbols (which ENABLE_EXPORTS provides).
#   Windows: links against the lua DLL's import library at build time.
#            The PE/COFF loader has no equivalent to POSIX's globally
#            searchable symbol table, so the DLL must be a real dependency.
#
# This file is only added to the build when LUACPP_ENABLE_CMODULE_LOADING
# is ON (gated by the outer add_subdirectory call in the root CMakeLists),
# so no platform skip is needed here.

add_library(uuid_module SHARED uuid.c)

# Land the plugin in bin/plugins/, separate from the host EXE so that
# Lua's default cpath (which on Windows includes ".\\?.dll") doesn't
# find it without our addModuleSearchPath. That keeps the negative-
# control test meaningful and mirrors real-world deployment where
# plugins live in a dedicated directory.
set_target_properties(uuid_module PROPERTIES
	OUTPUT_NAME "uuid"
	PREFIX ""  # produces "uuid.so" / "uuid.dll", not "libuuid.so"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
	LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/plugins
)

target_include_directories(uuid_module PRIVATE
	${CMAKE_SOURCE_DIR}/extern/lua/include
)

if(WIN32)
	# On Windows the DLL cannot have unresolved symbols; we must link
	# against the lua DLL's import library so the loader knows where
	# lua_* comes from at LoadLibrary time.
	target_link_libraries(uuid_module PRIVATE lua)
endif()

if(APPLE)
	# macOS specifics:
	#   - Lua's default package.cpath looks for ".so", not ".dylib".
	#   - dynamic_lookup defers lua_* resolution to load time so the
	#     library can be linked without seeing the host's symbols.
	set_target_properties(uuid_module PROPERTIES
		SUFFIX ".so"
		LINK_FLAGS "-undefined dynamic_lookup"
	)
endif()
