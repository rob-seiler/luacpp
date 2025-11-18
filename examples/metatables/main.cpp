#include <luacpp/State.hpp>
#include <luacpp/Metatable.hpp>
#include <luacpp/Basics.hpp>
#include "Vector2D.hpp"
#include "Transform2D.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

using namespace Lua;

// Helper to safely retrieve userdata with type checking
template <typename T>
T* getUserData(lua_State* lvm, int index) {
    const char* tname = Metatable<T>::metatableName();
    void* ud = Basics::checkUserData(lvm, index, tname);
    if (ud == nullptr) {
        return nullptr;
    }
    return static_cast<T*>(ud);
}

void printVector(const Vector2D& v, const char* name) {
    if (name == nullptr) {
        name = "unnamed";
    }
    std::cout << name << ": (" << v.x << ", " << v.y << ")" << std::endl;
}

void printTransform(const Transform2D& t, const char* name) {
    if (name == nullptr) {
        name = "unnamed";
    }
    std::cout << name << ":" << std::endl;
    std::cout << "  Position: (" << t.position.x << ", " << t.position.y << ")" << std::endl;
    std::cout << "  Rotation: " << t.rotation << " rad" << std::endl;
    std::cout << "  Scale: (" << t.scale.x << ", " << t.scale.y << ")" << std::endl;
}

std::string loadScriptFile(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open script file: " << filename << std::endl;
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

int main() {
    State lua(State::LibBase);

    std::cout << "=== Metatable Example: Vector2D and Transform2D ===" << std::endl;
    std::cout << std::endl;

    // Register metatables
    std::cout << "Registering metatables..." << std::endl;
    Metatable<Vector2D>::registerMetatable(lua);
    Metatable<Transform2D>::registerMetatable(lua);
    std::cout << "Metatables registered successfully!" << std::endl;
    std::cout << std::endl;

    // Register factory functions
    std::cout << "Registering factory functions..." << std::endl;

    lua.registerNativeFunction("createVector", [](lua_State* lvm) -> int {
        State L(lvm);
        float x = static_cast<float>(L.getArgument<double>(1));
        float y = static_cast<float>(L.getArgument<double>(2));
        Metatable<Vector2D>::create(L, x, y);
        return 1;
    });

    lua.registerNativeFunction("createTransform", [](lua_State* lvm) -> int {
        State L(lvm);
        Vector2D* pos = getUserData<Vector2D>(lvm, 1);
        float rotation = static_cast<float>(L.getArgument<double>(2));
        Vector2D* scale = getUserData<Vector2D>(lvm, 3);
        Metatable<Transform2D>::create(L, *pos, rotation, *scale);
        return 1;
    });

    lua.registerNativeFunction("printVec", [](lua_State* lvm) -> int {
        State L(lvm);
        Vector2D* v = getUserData<Vector2D>(lvm, 1);
        const char* name = L.getArgument<const char*>(2);
        if (v) {
            printVector(*v, name);
        }
        return 0;
    });

    lua.registerNativeFunction("printTransform", [](lua_State* lvm) -> int {
        State L(lvm);
        Transform2D* t = getUserData<Transform2D>(lvm, 1);
        const char* name = L.getArgument<const char*>(2);
        if (t) {
            printTransform(*t, name);
        }
        return 0;
    });

    std::cout << "Factory functions registered!" << std::endl;
    std::cout << std::endl;

    // Load and execute Lua script
    std::cout << "=== Executing Lua Script ===" << std::endl;
    std::cout << std::endl;

    std::string script = loadScriptFile("demo.lua");
    if (script.empty()) {
        std::cerr << "Failed to load demo.lua" << std::endl;
        return 1;
    }

    int ret = lua.loadAndExecuteScript(script.c_str());

    if (ret != 0) {
        std::cerr << "Error executing script!" << std::endl;
        const auto& errors = lua.getErrorList();
        for (const auto& err : errors) {
            std::cerr << "  " << err << std::endl;
        }
        return 1;
    }

    std::cout << std::endl;
    std::cout << "=== Example finished ===" << std::endl;

    return 0;
}
