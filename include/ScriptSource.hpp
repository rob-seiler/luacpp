#ifndef SCRIPTING_SCRIPTSOURCE_HPP
#define SCRIPTING_SCRIPTSOURCE_HPP

#include <string_view>

namespace Promess {
namespace UFM6 {

class ScriptSource {
public:
	virtual ~ScriptSource() = default;
	
	virtual std::string_view getScriptData() const = 0;
	virtual std::string_view getName() const = 0;
};

} // namespace UFM6
} // namespace Promess

#endif // SCRIPTING_SCRIPTSOURCE_HPP