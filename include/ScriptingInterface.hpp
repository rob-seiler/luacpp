#ifndef SCRIPTING_SCRIPTINGINTERFACE_HPP
#define SCRIPTING_SCRIPTINGINTERFACE_HPP

#include <string>

//forward declarations
namespace Promess::OS { class ThreadPool; }
namespace Promess::UFM6 { class UserIoBitList; }
namespace Promess::UFM6 { class LuaScript; }
namespace Promess::UFM6 { struct ConnectionInfo; }

namespace Promess {
namespace UFM6 {

/**
 * \brief This is an interface for controlling the firmware via scripting
*/
//ToDo: inherit from IControlInterface to establish it as a real interface
class ScriptingInterface /*: public IControlInterface*/
{
public:
	ScriptingInterface(const std::string& configPath, OS::ThreadPool& threadpool);
	~ScriptingInterface();

	int listAllScripts();

	void setUserIoLists(UserIoBitList* inputs, UserIoBitList* outputs);

private:
	constexpr static const char* const ScriptingDir = "scripting";

	void initDevConsole();
	void initWebSocket();

	/**
	 * \brief creates a new lua script
	 * This method creates and configures a new lua script
	*/
	LuaScript createLuaScript();
	void registerNativeFunctions(LuaScript& script);

	int executeLuaFile(const std::string& filepath);

	int setUserIo(LuaScript& script);

	const std::string m_configPath;
	OS::ThreadPool& m_threadpool;

	//stuff used for implementing native functions
	UserIoBitList* m_userInputs;
	UserIoBitList* m_userOutputs;
};

} //namespace UFM6
} //namespace Promess

#endif // SCRIPTING_SCRIPTINGINTERFACE_HPP