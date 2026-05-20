-- Fixture for FileLoadingTest: explicit runtime error on a known line.
-- The error() call below is on line 4; tracebacks should reference
-- "runtime_error.lua:4" once luaL_dofile sets the chunk name from the path.
error("boom from runtime_error.lua")
