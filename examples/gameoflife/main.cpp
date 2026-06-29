#include <luacpp/State.hpp>
#include <luacpp/Metatable.hpp>
#include <luacpp/Bind.hpp>
#include "Grid.hpp"

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

// Render mode: 1 = ANSI clear-screen + delay (animated), 0 = sequential printout
#ifndef LUACPP_GOL_ANIMATE
#define LUACPP_GOL_ANIMATE 1
#endif

using namespace Lua;

static void enableAnsi() {
#if defined(_WIN32) && LUACPP_GOL_ANIMATE
	HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
		SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
#endif
}

static int luaBeginFrame(lua_State*) {
#if LUACPP_GOL_ANIMATE
	std::this_thread::sleep_for(std::chrono::milliseconds(150));
	std::cout << "\033[2J\033[H" << std::flush;
#else
	std::cout << '\n';
#endif
	return 0;
}

static std::string loadScript(const char* filename) {
	std::ifstream file(filename);
	if (!file.is_open()) {
		std::cerr << "Failed to open " << filename << std::endl;
		return "";
	}
	std::stringstream buf;
	buf << file.rdbuf();
	return buf.str();
}

int main() {
	enableAnsi();

	State lua(State::LibBase | State::LibIO);
	auto& errorLog = lua.diagnostics.installLogger<MemoryLogger>();

	Metatable<Grid>::registerMetatable(lua);
	lua.binding.constructor<Grid, int, int>("Grid");
	lua.binding.method<Grid, &Grid::set>("set");
	lua.binding.method<Grid, &Grid::get>("get");
	lua.binding.method<Grid, &Grid::clear>("clear");
	lua.binding.method<Grid, &Grid::countAlive>("countAlive");
	lua.binding.method<Grid, &Grid::step>("step");
	lua.binding.property<Grid, &Grid::width>("width");
	lua.binding.property<Grid, &Grid::height>("height");
	lua.binding.property<Grid, &Grid::generation>("generation");

	lua.registerNativeFunction("beginFrame", luaBeginFrame);

	std::string script = loadScript("simulation.lua");
	if (script.empty()) {
		return 1;
	}

	lua.loadAndExecuteScript(script.c_str());
	if (!errorLog.entries().empty()) {
		std::cerr << "Script error" << std::endl;
		for (const auto& e : errorLog.entries()) std::cerr << "  " << e.message << std::endl;
		return 1;
	}
	return 0;
}
