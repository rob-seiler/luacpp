#include <Literals.hpp>
#include <fstream>
#include <stdexcept>

namespace Lua {

std::string operator "" _load(const char* path, std::size_t) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file) {
		throw std::runtime_error("Failed to open file");
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::string buffer(size, '\0');
	if (!file.read(&buffer[0], size)) {
		throw std::runtime_error("Failed to read file");
	}

	return buffer;
}

} //namespace Lua