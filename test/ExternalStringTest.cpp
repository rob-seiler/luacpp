#include <gtest/gtest.h>

#include <luacpp/State.hpp>
#include <luacpp/Basics.hpp>

#include <cstring>

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

class ExternalStringTest : public ::testing::Test {
protected:
	void SetUp() override {
		g_freeCalls = 0;
		g_freedPtr = nullptr;
		g_freedSize = 0;
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

} // namespace
} // namespace Lua
