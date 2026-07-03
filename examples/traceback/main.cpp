// Opt-in Lua traceback support: a mini "script editor console" that renders
// error text, source location, and the Lua call stack as separate UI
// elements — the way an engine-embedded script editor would.

#include <luacpp/State.hpp>
#include <iostream>

namespace {

// Stand-in for an editor UI: status bar, goto-line, stack panel.
struct EditorConsole {
	bool sawError = false;

	void showError(const Lua::LuaError& err) {
		sawError = true;

		// Status bar: just the error text, no prefix, no stack.
		std::cout << "[status] " << err.message.text() << '\n';

		// Jump target: chunk + line from the standard prefix.
		if (auto line = err.message.line()) {
			std::cout << "[goto  ] " << err.message.source().value_or("?")
			          << ":" << *line << '\n';
		}

		// Stack panel: one row per frame — a real editor would make these
		// clickable and jump to frame.source:frame.line on activation.
		if (auto tb = err.message.traceback()) {
			std::cout << "[stack ]\n";
			for (const auto& frame : tb->asList()) {
				std::cout << "    " << frame.raw;
				if (frame.line) {
					std::cout << "  -> jump to " << frame.source << ":" << *frame.line;
				}
				std::cout << '\n';
			}
		}
	}
};

} // namespace

int main() {
	Lua::State lua(Lua::State::LibBase);
	EditorConsole console;

	// The opt-in: failed calls now carry a stack traceback.
	lua.diagnostics.setTracebackEnabled(true);

	// Route errors into the "editor UI" instead of the default std::cerr log.
	lua.diagnostics.setLogger(nullptr);
	lua.diagnostics.installErrorHandler<Lua::CallbackHandler>(
	    [&console](const Lua::LuaError& err) { console.showError(err); });

	// An "editor script" with nested calls failing in the innermost function.
	const char* script = R"(
		function update_player(player)
			return player.position.x   -- fails: position is nil
		end

		function on_frame()
			update_player({ name = "hero" })
		end

		on_frame()
	)";

	lua.loadAndExecuteScript(script);
	return console.sawError ? 0 : 1; // the error is expected here
}
