#include "Vector2D.hpp"
#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Basics.hpp>

namespace Lua {

void Metatable<::Vector2D>::registerConstructor(State& state) {
    state.createTable("Vector", [&state](Table& vectorTable) {
        state.createMetaTable("VectorConstructorMT", [](Table& mt) {
            int (*callFunc)(lua_State*) = [](lua_State* lvm) -> int {
                State L(lvm);
                float x = static_cast<float>(L.getArgument<double>(2));
                float y = static_cast<float>(L.getArgument<double>(3));
                Metatable<::Vector2D>::create(L, x, y);
                return 1;
            };
            mt.setElement(State::MetaTable::Call, callFunc);
        });
        vectorTable.assignMetaTable("VectorConstructorMT");
    });
}

// Note: Template methods are implemented in the header to allow implicit instantiation

} // namespace Lua
