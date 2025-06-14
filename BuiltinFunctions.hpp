#pragma once

#include <functional>
#include <vector>
#include <string>
#include "NeReLaBasic.hpp"

// Forward-declare the main classes/structs to avoid circular dependencies
class NeReLaBasic;
using FunctionTable = std::unordered_map<std::string, NeReLaBasic::FunctionInfo>;


// This is the single public function declaration for this file.
// It takes a reference to the interpreter and a reference to the specific
// function table (e.g., the main table or a module's table) that it should populate.
void register_builtin_functions(NeReLaBasic& vm, FunctionTable& table_to_populate);