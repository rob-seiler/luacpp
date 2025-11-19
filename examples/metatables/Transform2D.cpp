#include "Transform2D.hpp"
#include <luacpp/State.hpp>
#include <luacpp/Table.hpp>
#include <luacpp/Basics.hpp>

namespace Lua {

void Metatable<::Transform2D>::registerConstructor(State& state) {
    state.createTable("Transform", [&state](Table& transformTable) {
        state.createMetaTable("TransformConstructorMT", [](Table& mt) {
            int (*callFunc)(lua_State*) = [](lua_State* lvm) -> int {
                State L(lvm);
                void* posUd = Basics::checkUserData(lvm, 2, Metatable<::Vector2D>::metatableName());
                ::Vector2D* pos = static_cast<::Vector2D*>(posUd);
                float rotation = static_cast<float>(L.getArgument<double>(3));
                void* scaleUd = Basics::checkUserData(lvm, 4, Metatable<::Vector2D>::metatableName());
                ::Vector2D* scale = static_cast<::Vector2D*>(scaleUd);
                Metatable<::Transform2D>::create(L, *pos, rotation, *scale);
                return 1;
            };
            mt.setElement(State::MetaTable::Call, callFunc);
        });
        transformTable.assignMetaTable("TransformConstructorMT");
    });
}

// Note: Template methods are implemented in the header to allow implicit instantiation

} // namespace Lua
