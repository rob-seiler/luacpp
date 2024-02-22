cmake_minimum_required(VERSION 3.10)

add_subdirectory(./lua)

project(luacpp)
set(CMAKE_CXX_STANDARD 17)
add_library(${PROJECT_NAME})
target_sources(${PROJECT_NAME} PRIVATE
    src/FileScriptSource.cpp
    src/LuaScript.cpp
    src/LuaTable.cpp
    src/ScriptingInterface.cpp
)
