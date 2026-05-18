#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Basics.hpp>
#include <lua/lua.hpp>

#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lua {
namespace {

int g_freeCalls = 0;
void* g_freedPtr = nullptr;
size_t g_freedSize = 0;

void* recordingDealloc(void* /*ud*/, void* ptr, size_t osize, size_t nsize) {
	if (nsize == 0) {
		++g_freeCalls;
		g_freedPtr = ptr;
		g_freedSize = osize;
	}
	return nullptr;
}

struct DestructionCounter {
	static int liveCount;
	DestructionCounter() { ++liveCount; }
	DestructionCounter(const DestructionCounter&) { ++liveCount; }
	DestructionCounter(DestructionCounter&&) noexcept { ++liveCount; }
	~DestructionCounter() { --liveCount; }
};
int DestructionCounter::liveCount = 0;

class ExternalStringTest : public ::testing::Test {
protected:
	void SetUp() override {
		g_freeCalls = 0;
		g_freedPtr = nullptr;
		g_freedSize = 0;
		DestructionCounter::liveCount = 0;
	}
};

TEST_F(ExternalStringTest, staticBufferWithNullDeallocator) {
	static constexpr const char kStr[] = "external static string";
	constexpr size_t kLen = sizeof(kStr) - 1;

	State lua(State::LibNone);
	Basics::pushExternalString(lua.getState(), kStr, kLen, nullptr, nullptr);

	size_t len = 0;
	const char* result = Basics::asString(lua.getState(), -1, &len);
	EXPECT_STREQ(result, kStr);
	EXPECT_EQ(len, kLen);
	EXPECT_EQ(result, kStr) << "Lua should reference the original buffer, not a copy";

	lua.popStack(1);
}

TEST_F(ExternalStringTest, deallocatorInvokedAtStateClose) {
	// Long enough (>40 chars) to avoid any potential internalization corner cases
	// and to make the size assertion meaningful.
	char buffer[] = "this external string is intentionally longer than forty chars";
	const size_t len = sizeof(buffer) - 1;

	{
		State lua(State::LibNone);
		Basics::pushExternalString(lua.getState(), buffer, len, recordingDealloc, nullptr);
		EXPECT_STREQ(Basics::asString(lua.getState(), -1), buffer);
	}

	EXPECT_EQ(g_freeCalls, 1);
	EXPECT_EQ(g_freedPtr, buffer);
	EXPECT_EQ(g_freedSize, len + 1) << "Lua passes osize as len including the null terminator";
}

TEST_F(ExternalStringTest, userDataIsForwardedToDeallocator) {
	static int sentinel = 0;
	auto dealloc = [](void* ud, void* /*ptr*/, size_t /*osize*/, size_t nsize) -> void* {
		if (nsize == 0) {
			*static_cast<int*>(ud) = 0xC0FFEE;
		}
		return nullptr;
	};

	char buffer[] = "payload-with-user-data-pointer-forwarded";
	{
		State lua(State::LibNone);
		Basics::pushExternalString(lua.getState(), buffer, sizeof(buffer) - 1, dealloc, &sentinel);
	}

	EXPECT_EQ(sentinel, 0xC0FFEE);
}

TEST_F(ExternalStringTest, pushExternalStringFromStdStringOverload) {
	State lua(State::LibNone);
	// Long enough to avoid any short-string special cases and to make
	// pointer identity meaningful even on implementations that might
	// otherwise intern.
	std::string s = "this is a sufficiently long std::string for external pushing";

	lua.pushExternalString(s);

	size_t len = 0;
	const char* result = Basics::asString(lua.getState(), -1, &len);
	EXPECT_EQ(len, s.size());
	EXPECT_EQ(result, s.data()) << "Lua must reference the original std::string buffer";

	lua.popStack(1);
}

TEST_F(ExternalStringTest, mapOutlivingState_noTransferNeeded) {
	// Buffers live in the outer scope; state is destroyed first.
	// pushExternalString uses nullptr dealloc — Lua never tries to free.
	std::map<std::string, std::string> dict = {
		{"greeting", "hello there, this value is comfortably long enough to be heap-stored"},
		{"farewell", "goodbye, this value is also comfortably long enough to be heap-stored"}
	};

	{
		State lua(State::LibNone);
		for (const auto& [k, v] : dict) {
			lua.pushExternalString(v);
			lua_setglobal(lua.getState(), k.c_str());
		}
		EXPECT_EQ(lua.readVariable<std::string>("greeting"), dict.at("greeting"));
		EXPECT_EQ(lua.readVariable<std::string>("farewell"), dict.at("farewell"));
	}
	// State destroyed before dict — no dangling refs because Lua never held ownership.
}

TEST_F(ExternalStringTest, transferOwnership_mapDiesBeforeStateButRefsStayValid) {
	State lua(State::LibNone);
	{
		std::map<std::string, std::string> dict = {
			{"a", "value a is deliberately long enough to bypass small string optimization"},
			{"b", "value b is also long enough to guarantee a heap allocation, no SBO here"},
		};

		for (const auto& [k, v] : dict) {
			lua.pushExternalString(v);
			lua_setglobal(lua.getState(), k.c_str());
		}

		// Transfer ownership: map nodes are address-stable under move, so
		// the external pointers Lua holds remain valid.
		lua.transferOwnership(std::move(dict));
	}
	// dict is now out of scope, but Lua-owned heap copy keeps the buffers alive.
	EXPECT_EQ(lua.readVariable<std::string>("a"),
	          "value a is deliberately long enough to bypass small string optimization");
	EXPECT_EQ(lua.readVariable<std::string>("b"),
	          "value b is also long enough to guarantee a heap allocation, no SBO here");
}

TEST_F(ExternalStringTest, transferOwnership_worksForUnorderedMap) {
	State lua(State::LibNone);
	{
		std::unordered_map<std::string, std::string> dict = {
			{"x", "externally managed string number one, long enough to be heap allocated"},
			{"y", "externally managed string number two, long enough to be heap allocated"},
		};

		for (const auto& [k, v] : dict) {
			lua.pushExternalString(v);
			lua_setglobal(lua.getState(), k.c_str());
		}

		lua.transferOwnership(std::move(dict));
	}
	EXPECT_EQ(lua.readVariable<std::string>("x"),
	          "externally managed string number one, long enough to be heap allocated");
	EXPECT_EQ(lua.readVariable<std::string>("y"),
	          "externally managed string number two, long enough to be heap allocated");
}

TEST_F(ExternalStringTest, pushExternalString_shortStringTakesCopyPath) {
	// Proves the SBO branch: a short std::string must be copied at push time,
	// because referencing its in-object bytes externally would tie Lua to the
	// std::string object's stack/heap address.
	State lua(State::LibNone);
	std::string s = "short";

	lua.pushExternalString(s);
	const char* result = Basics::asString(lua.getState(), -1, nullptr);
	EXPECT_NE(result, s.data())
	    << "SBO strings must be copied into Lua, not externally referenced";

	lua.popStack(1);
}

TEST_F(ExternalStringTest, transferOwnership_long_bufferStableAcrossSourceScope) {
	// The strong proof of the heap-path transfer: capture Lua's pointer before
	// the source goes out of scope, then re-check it after. Equal pointers
	// prove the heap buffer was preserved at its original address — and thus
	// that the swap-into-anchored-holder really happened.
	State lua(State::LibNone);
	const char* originalPtr = nullptr;
	{
		std::string s = "long source string that takes the external pointer path";
		originalPtr = s.data();
		lua.pushExternalString(s);
		lua_setglobal(lua.getState(), "longVal");
		lua.transferOwnership(std::move(s));
	}
	lua_getglobal(lua.getState(), "longVal");
	const char* afterPtr = Basics::asString(lua.getState(), -1, nullptr);
	EXPECT_EQ(afterPtr, originalPtr)
	    << "transferOwnership must preserve the heap buffer Lua references";
	lua.popStack(1);
}

TEST_F(ExternalStringTest, transferOwnership_long_dataSurvivesMemoryChurn) {
	// Without AddressSanitizer we cannot deterministically detect a use-after-
	// free, but heavy churn after the source's destruction makes it likely
	// that any freed buffer gets overwritten. If transferOwnership were a
	// no-op for long strings, the readVariable below would very likely return
	// garbage.
	const std::string expected = "long source string for the churn test, definitely heap";
	State lua(State::LibNone);
	{
		std::string s = expected;
		lua.pushExternalString(s);
		lua_setglobal(lua.getState(), "longVal");
		lua.transferOwnership(std::move(s));
	}
	std::vector<std::vector<char>> churn;
	for (int i = 0; i < 100; ++i) {
		churn.emplace_back(2048, 'Z');
	}
	EXPECT_EQ(lua.readVariable<std::string>("longVal"), expected);
}

TEST_F(ExternalStringTest, transferOwnership_singleStdStringShort_SBOPath) {
	// Short string fits in std::string's SBO buffer on all major standard
	// libraries. pushExternalString must detect this and have Lua make a copy
	// at push time; transferOwnership for std::string then becomes a no-op
	// because Lua already owns its data.
	State lua(State::LibNone);
	{
		std::string s = "short";
		lua.pushExternalString(s);
		lua_setglobal(lua.getState(), "shortVal");
		lua.transferOwnership(std::move(s));
	}
	// s is destroyed; Lua's own copy of the SBO bytes must still be readable.
	EXPECT_EQ(lua.readVariable<std::string>("shortVal"), "short");
}

TEST_F(ExternalStringTest, transferOwnership_singleStdStringLong_heapPath) {
	// Long string lives on the heap. pushExternalString uses the external-ref
	// path; transferOwnership swaps the heap buffer into a registry-anchored
	// holder, keeping Lua's pointer valid past the source's destruction.
	State lua(State::LibNone);
	{
		std::string s = "this string is intentionally long enough to be heap allocated and bypass SBO";
		lua.pushExternalString(s);
		lua_setglobal(lua.getState(), "longVal");
		lua.transferOwnership(std::move(s));
	}
	EXPECT_EQ(lua.readVariable<std::string>("longVal"),
	          "this string is intentionally long enough to be heap allocated and bypass SBO");
}

TEST_F(ExternalStringTest, transferOwnership_destructorFiresAtStateClose) {
	{
		State lua(State::LibNone);
		lua.transferOwnership(DestructionCounter{});
		// One heap copy owned by Lua; the temporary moved-into-heap counted +1
		// at construction and the temporary already destructed (-1) by the time
		// this line runs, leaving net +1 alive.
		EXPECT_EQ(DestructionCounter::liveCount, 1);
	}
	EXPECT_EQ(DestructionCounter::liveCount, 0) << "__gc must delete the heap copy";
}

TEST_F(ExternalStringTest, transferOwnership_multipleTransfersAllReleased) {
	{
		State lua(State::LibNone);
		lua.transferOwnership(DestructionCounter{});
		lua.transferOwnership(DestructionCounter{});
		lua.transferOwnership(DestructionCounter{});
		EXPECT_EQ(DestructionCounter::liveCount, 3);
	}
	EXPECT_EQ(DestructionCounter::liveCount, 0);
}

} // namespace
} // namespace Lua
