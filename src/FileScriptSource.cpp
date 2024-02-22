#include <scripting/FileScriptSource.hpp>
#include <fstream>
#include <filesystem>


namespace Promess {
namespace UFM6 {

FileScriptSource::FileScriptSource(std::string_view filePath)
 : m_script(""), m_name(filePath)
{
	loadData();
}

void FileScriptSource::loadData() {
	// load the script from the file
	std::ifstream file(m_name);
	if (!file.is_open()) {
	 	return;
	}

	m_script = std::string((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
}

} // namespace UFM6
} // namespace Promess