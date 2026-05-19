# Mini UUID plugin for luacpp. Built as a SHARED Lua module whose
# undefined lua_* references are resolved at dlopen time against the
# host process's exported symbols.
#
# POSIX-only: on Windows the host executable cannot export its lua_*
# symbols to a LoadLibrary'd DLL without a shared Lua build. That mode
# is deferred to a separate change; until then this file is a no-op
# under Windows and the compile-time linkage guarantee on that platform
# is covered by test/c_linkage_check.c.
if(WIN32)
	return()
endif()

add_library(uuid_module SHARED uuid.c)

set_target_properties(uuid_module PROPERTIES
	OUTPUT_NAME "uuid"
	PREFIX ""  # produces "uuid.so", not "libuuid.so"
)

target_include_directories(uuid_module PRIVATE
	${CMAKE_SOURCE_DIR}/extern/lua/include
)

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
