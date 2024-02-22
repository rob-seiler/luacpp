#ifndef SCRIPTING_FILESCRIPTSOURCE_HPP
#define SCRIPTING_FILESCRIPTSOURCE_HPP

#include "ScriptSource.hpp"
#include <string>

namespace Promess {
namespace UFM6 {

/**
 * @brief A script source that is stored in a file
 * 
*/
class FileScriptSource : public ScriptSource {
public:
	FileScriptSource(std::string_view filePath);

	std::string_view getScriptData() const override { return m_script; }
	std::string_view getName() const override { return m_name; }

private:
    void loadData();

	std::string m_script; ///< the script data (loaded lazily)
	std::string m_name; ///< name of the script (equivalent to the filename)
};

} // namespace UFM6
} // namespace Promess

#endif // SCRIPTING_FILESCRIPTSOURCE_HPP