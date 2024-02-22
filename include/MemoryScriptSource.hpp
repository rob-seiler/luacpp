#ifndef SCRIPTING_MEMORYSCRIPTSOURCE_HPP
#define SCRIPTING_MEMORYSCRIPTSOURCE_HPP

#include "ScriptSource.hpp"

namespace Promess {
namespace UFM6 {

/**
 * @brief A script source that is stored in memory
 * 
 * The data is stored as a string_view, so the data must be available as long as the source is used.
*/
class MemoryScriptSource : public ScriptSource {
public:
	constexpr static const std::string_view DefaultScriptName = "LUA_Script";
	MemoryScriptSource(std::string_view script, std::string_view name = DefaultScriptName) : m_script(script), m_name(name) {}

	std::string_view getScriptData() const override { return m_script; }
	std::string_view getName() const override { return m_name; }

private:
	std::string_view m_script;
	std::string_view m_name;
};

} // namespace UFM6
} // namespace Promess

#endif // SCRIPTING_MEMORYSCRIPTSOURCE_HPP