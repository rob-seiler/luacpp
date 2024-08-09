#ifndef LUACPP_VERSION_HPP
#define LUACPP_VERSION_HPP

#include <cstdint>
#include <array>

struct lua_State;

namespace Lua {

class Version {
public:
	constexpr Version(uint8_t major, uint8_t minor, uint8_t patch) : m_major(major), m_minor(minor), m_patch(patch) {}

	constexpr uint8_t getMajor() const { return m_major; }
	constexpr uint8_t getMinor() const { return m_minor; }
	constexpr uint8_t getPatch() const { return m_patch; }
	constexpr size_t toLuaNumber() const { return m_major * 10000 + m_minor * 100 + m_patch; }
	constexpr uint32_t toNumber() const { return m_major << 16 | m_minor << 8 | m_patch; }
	constexpr std::array<const char, 9> asString() const {
		std::array<const char, 9> result{};
		char* data = const_cast<char*>(result.data());
		toArray(m_major, data);
		*(data++) = '.';
		toArray(m_minor, data);
		*(data++) = '.';
		toArray(m_patch, data);
		*(data++) = '\0';
		return result;
	}
	
	/**
	 * @brief returns the version of the underlying Lua library
	 * The version number is retrieved from the header file (lua.h) and not from the library itself.
	 */
	static Version getLuaVersion();

	/**
	 * @brief returns the version of the Lua library
	 * The version number is retrieved from the Lua library itself. Unfortunately, the patch version is not available.
	 */
	static Version readLuaVersion(lua_State* L = nullptr);

private:
	constexpr void toArray(uint8_t version, char*& data) const {
		if (version > 9) {
			*(data++) = static_cast<char>('0' + version / 10);
		}
		*(data++) = static_cast<char>('0' + version % 10);
	}
	uint8_t m_major;
	uint8_t m_minor;
	uint8_t m_patch;
};

#ifndef USE_CPP20_MODULES
constexpr inline static const Version LuaCppVersion{0, 1, 0};
#endif

} // namespace Lua

#endif // LUACPP_VERSION_HPP