#include <scripting/ScriptingInterface.hpp>
#include <scripting/FileScriptSource.hpp>
#include <scripting/MemoryScriptSource.hpp>
#include <scripting/LuaScript.hpp>

#include <DeveloperConsole.h>
#include <Threading/ThreadPool.h>
#include <station/userIO/UserIoBitList.h>

#include <client/ClientEventHandler.h>
#include <util/JsonMessage.h>

#include <chrono>
#include <thread>
#include <fstream>

#ifndef __QNX__
#include <filesystem>
#else
#include <FileSystem.h>
#endif

namespace {

struct ThreadInfo {
	Promess::UFM6::ScriptingInterface* instance;
	std::string filepath;
};

void forFileInDirectoryDo(const std::string& path, std::function<void(const std::string&)> func) {
#ifndef __QNX__
	for (const auto& entry : std::filesystem::directory_iterator(path)) {
		func(entry.path().filename().string());
	}
#else
	std::vector<std::string> files;
	Promess::OS::FileSystem::readDirectory(path, files, true, nullptr);
	for (const std::string& file : files) {
		func(file);
	}
#endif
}

bool doesFileExist(const std::string& path) {
#ifndef __QNX__
	return std::filesystem::exists(path);
#else
	return Promess::OS::FileSystem::fileExists(path);
#endif
}

int printToDevConsole(lua_State* state) {
	Promess::UFM6::LuaScript lua(state);
	int numArgs = lua.getStackSize();
	for (int i = 1; i < numArgs; ++i) {
		const std::string arg = lua.getArgument<std::string>(i);
		Promess::UFM6::Console.formatAndPrint("{} ", arg);
	}
	const std::string lastArg = lua.getArgument<std::string>(numArgs);
	Promess::UFM6::Console.formatAndPrint("{}\n", lastArg);
	lua.popStack(numArgs);
	return 0;
}

int sleepS(lua_State* state) {
	constexpr static const float sec2msec = 1000;

	Promess::UFM6::LuaScript lua(state);
	float sleepTime = lua.getArgument<float>(1); //sleep time in seconds
	if (sleepTime <= 0) {
		return 0; //return immediately
	}

	const auto start = std::chrono::high_resolution_clock::now();
	std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<uint64_t>(sleepTime * sec2msec)));
	return 0;
}

int sleepUS(lua_State* state) {
	Promess::UFM6::LuaScript lua(state);
	int32_t sleepTime = lua.getArgument<int32_t>(1); //sleep time in seconds
	if (sleepTime <= 0) {
		return 0; //return immediately
	}

	const auto start = std::chrono::high_resolution_clock::now();
	std::this_thread::sleep_for(std::chrono::microseconds(static_cast<uint64_t>(sleepTime)));
	return 0;
}

std::string parseForScriptFile(Promess::Network::MessageObject* mo, Json::Value& answer) {
	constexpr static const char* const FileKey = "file";
	constexpr static const size_t FileKeySize = std::char_traits<char>::length(FileKey);

	Promess::Network::JsonMessage* json = dynamic_cast<Promess::Network::JsonMessage*>(mo);
	if (!json) {
		answer["success"] = false;
		answer["error"] = "invalid message type";
		return "";
	}

	const Json::Value* req = json->getAsJson().find(FileKey, FileKey + FileKeySize);
	if (!req) {
		answer["success"] = false;
		answer["error"] = "missing \"file\" key";
		return "";
	}

	return req->asString();
}

std::string parseForScriptFileAndCheckExistence(Promess::Network::MessageObject* mo, Json::Value& answer, const std::string& configPath) {
	const std::string filename = parseForScriptFile(mo, answer);
	const std::string filepath = configPath + filename;
	if (!doesFileExist(filepath)) {
		answer["success"] = false;
		answer["error"] = "file does not exist";
		return "";
	}

	return filepath;
}

} //namespace anonymous

std::string parseForScriptContent(Promess::Network::MessageObject* mo, Json::Value& answer) {
	constexpr static const char* const ScriptKey = "script";
	constexpr static const size_t ScriptKeySize = std::char_traits<char>::length(ScriptKey);

	Promess::Network::JsonMessage* json = dynamic_cast<Promess::Network::JsonMessage*>(mo);
	if (!json) {
		answer["success"] = false;
		answer["error"] = "invalid message type";
		return "";
	}

	const Json::Value* req = json->getAsJson().find(ScriptKey, ScriptKey + ScriptKeySize);
	if (!req) {
		answer["success"] = false;
		answer["error"] = "missing \"script\" key";
		return "";
	}

	return req->asString();

} // namespace anonymous

namespace Promess {
namespace UFM6 {

ScriptingInterface::ScriptingInterface(const std::string& configPath, OS::ThreadPool& threadpool)
: m_configPath(configPath + ScriptingDir + "/"),
  m_threadpool(threadpool),
  m_userInputs(nullptr),
  m_userOutputs(nullptr)
{ 
	initDevConsole();
	initWebSocket();
}

ScriptingInterface::~ScriptingInterface() { }

void ScriptingInterface::initDevConsole() {
	Console.registerCommand(
		"lua",
		[this](const std::vector<std::string>& args, const ConnectionInfo&)->int { 
			if (args.size() < 2) {
				Console.format("Usage: lua <lua file>\n");
				return 1;
			}
			if (args[1] == "list") {
				return listAllScripts();
			}

			ThreadInfo* info = new (std::nothrow)ThreadInfo{this, m_configPath + args[1]};
			if (!info) {
				Console.format("Failed to allocate memory\n");
				return 1;
			}
			if (!doesFileExist(info->filepath)) {
				Console.format("File does not exist [{}]\n", args[1]);
				delete info;
				return 2;
			}
			m_threadpool.runTask([](void* usrArg, OS::WorkerThread::Buffer&) {
				ThreadInfo* info = reinterpret_cast<ThreadInfo*>(usrArg);
				info->instance->executeLuaFile(info->filepath);
				delete info;
			}, info);
			return 0;
		},
		{"<lua file> | list", "<lua file>: executes the given lua file. list: lists all available lua files"}
	);
}

void ScriptingInterface::initWebSocket() {
	Network::ClientEventHandler& ceh = Network::ClientEventHandler::getInstance();
	ceh.registerFunc("ScriptingEngine::getScripts", [this](Network::MessageObject* mo) -> Network::WebMessage* {
		Json::Value answer = Json::arrayValue;
		forFileInDirectoryDo(m_configPath, [&answer](const std::string& file) {
			answer[answer.size()] = file;
		});
		return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
	});

	ceh.registerFunc("ScriptingEngine::loadScript", [this](Network::MessageObject* mo) -> Network::WebMessage* {
		Json::Value answer = Json::objectValue;
		const std::string filepath = parseForScriptFileAndCheckExistence(mo, answer, m_configPath);
		if (filepath.empty()) {
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}

		std::ifstream file(filepath);
		if (!file.is_open()) {
			answer["success"] = false;
			answer["error"] = "failed to open file";
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}

		answer["success"] = true;
		answer["script"] = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
	});

	ceh.registerFunc("ScriptingEngine::runScript", [this](Network::MessageObject* mo) -> Network::WebMessage* {
		Json::Value answer = Json::objectValue;
		const std::string filepath = parseForScriptFileAndCheckExistence(mo, answer, m_configPath);
		if (filepath.empty()) {
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}

		ThreadInfo* info = new (std::nothrow)ThreadInfo{this, filepath};
		if (!info) {
			answer["success"] = false;
			answer["error"] = "failed to allocate memory";
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}
		m_threadpool.runTask([](void* usrArg, OS::WorkerThread::Buffer&) {
			ThreadInfo* info = reinterpret_cast<ThreadInfo*>(usrArg);
			info->instance->executeLuaFile(info->filepath);
			delete info;
		}, info);

		answer["success"] = true;
		return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
	});

	ceh.registerFunc("ScriptingEngine::deleteScript", [this](Network::MessageObject* mo) -> Network::WebMessage* {
		Json::Value answer = Json::objectValue;
		const std::string filepath = parseForScriptFileAndCheckExistence(mo, answer, m_configPath);
		if (filepath.empty()) {
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}
		
		if (!std::remove(filepath.c_str())) {
			answer["success"] = true;
		} else {
			answer["success"] = false;
			answer["error"] = "failed to delete file";
		}

		return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
	});

	ceh.registerFunc("ScriptingEngine::saveScript", [this](Network::MessageObject* mo) -> Network::WebMessage* {
		constexpr static const char* const ScriptKey = "script";
		constexpr static const size_t ScriptKeySize = std::char_traits<char>::length(ScriptKey);

		Json::Value answer = Json::objectValue;
		const std::string filename = parseForScriptFile(mo, answer);
		if (filename.empty()) {
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}

		const std::string script = parseForScriptContent(mo, answer);

		const std::string filepath = m_configPath + filename;
		std::ofstream file(filepath);
		if (!file.is_open()) {
			answer["success"] = false;
			answer["error"] = "failed to open file";
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}

		file << script;
		file.close();

		answer["success"] = true;
		return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
	});

	ceh.registerFunc("ScriptingEngine::verifyScript", [this](Network::MessageObject* mo) -> Network::WebMessage* {
		Json::Value answer = Json::objectValue;
		const std::string code = parseForScriptContent(mo, answer);
		if (code.empty()) {
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}

		MemoryScriptSource source(code);
		LuaScript script = createLuaScript();
		int res = script.loadScriptIntoGlobal(source);
		if (res != 0) {
			answer["success"] = false;
			Json::Value& errorList = answer["errors"] = Json::arrayValue;
			for (const auto& err : script.getErrorList()) {
				errorList[errorList.size()] = err;
			}
			return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
		}

		answer["success"] = true;
		return Network::JsonMessage::createMessageFromJson(answer, mo->getMessageId(), mo->getAnswerId());
	});
}

int ScriptingInterface::listAllScripts() {
	Console.format("Available lua files:\n");
	forFileInDirectoryDo(m_configPath, [](const std::string& file) {
		Console.format("  {}\n", file);
	});
	return 0;
}

void ScriptingInterface::setUserIoLists(UserIoBitList* inputs, UserIoBitList* outputs) {
	m_userInputs = inputs;
	m_userOutputs = outputs;
}

LuaScript ScriptingInterface::createLuaScript() {
	LuaScript script;
	registerNativeFunctions(script);
	return script;	
}

void ScriptingInterface::registerNativeFunctions(LuaScript& script) {
	script.overrideLuaFunction("print", printToDevConsole);

	script.registerNativeFunction("sleep", sleepS);
	script.registerNativeFunction("usleep", sleepUS);

	script.registerNativeFunctionWithUpvalues("setUserIo", [](lua_State* state) {
		LuaScript lua(state);
		ScriptingInterface* self = lua.getUpValue<ScriptingInterface*>(1);
		return self->setUserIo(lua);
	}, this);
}

int ScriptingInterface::executeLuaFile(const std::string& filepath) {
	FileScriptSource source(filepath);
	if (source.getScriptData().empty()) {
		return 1;
	}
	LuaScript script = createLuaScript();
	int res = script.loadAndExecuteScript(source);
	if (res != 0) {
		Console.format("Error while executing script [{}]\n", filepath);
		for (const auto& err : script.getErrorList()) {
			Console.format("  {}\n", err);
		}
	}
	return res;
}

int ScriptingInterface::setUserIo(LuaScript& script) {
	if (!m_userInputs || !m_userOutputs) {
		Console.format("no user io lists available\n");
		return 1;
	}

	constexpr static const int IoSelectIndex = 1;
	const bool enabled = script.getArgument<bool>(2);
	if (script.isOfType(LuaScript::Type::Number, IoSelectIndex)) {
		//in this simple example we don't know if the index is given for the input or output list
		//so we assume the following pattern: 0 - numInputs-1: inputs, numInputs - numInputs+numOutputs-1: outputs
		const uint32_t pin = script.getArgument<uint32_t>(IoSelectIndex);
		const uint32_t inputThreshold = m_userInputs->getSize();
		const uint32_t outputThreshold = inputThreshold + m_userOutputs->getSize();
		if (pin < inputThreshold) {
			m_userInputs->get(pin)->set(enabled);
		} else if (pin < outputThreshold) {
			m_userOutputs->get(pin - inputThreshold)->set(enabled);
		} else {
			Console.format("Invalid pin index {}\n", pin);
			return 1;
		}
	} else {
		const std::string pin = script.getArgument<std::string>(IoSelectIndex);
		//here we know that the pin is given as a string, so we can use the name to find the correct pin
		//we search the input list first
		UserIo* io = m_userInputs->getByName(pin);
		if (!io) { //if the pin is not found in the input list, we search the output list
			io = m_userOutputs->getByName(pin);
		}
		if (!io) {
			Console.format("Invalid pin name {}\n", pin);
			return 1;
		}
		io->set(enabled);
	}

	return 0;
}

} //namespace UFM6
} //namespace Promess