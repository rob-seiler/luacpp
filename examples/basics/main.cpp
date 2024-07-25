#include <luacpp/State.hpp>
#include <cmath>

class LuaExample {
public:
	LuaExample() {
		m_state.registerMethod("pythagoras", [](Lua::State& state) {
			double a = state.getArgument<double>(1);
			double b = state.getArgument<double>(2);
			return state.setReturnValue(std::sqrt(a * a + b * b));
		});
		m_state.registerMethod("store", [this](Lua::State& state) {
			m_result = state.getArgument<double>(1);
			return 0;
		});
	}

	int run() {
		const char* script = R"(
			result = pythagoras(3, 4)
			store(result)
		)";

		m_state.loadAndExecuteScript(script);
		double result = m_state.readVariable<double>("result");
		return result == m_result ? 0 : 1;
	}

private:
	Lua::State m_state;
	double m_result;
};

int main(int argc, char** argv) {
	LuaExample example;
	return example.run();
}