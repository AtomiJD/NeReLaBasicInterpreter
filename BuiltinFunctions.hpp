#pragma once

class NeReLaBasic; // Forward declaration

// This single function will be responsible for adding all our built-in
// functions to the interpreter's function table.
void register_builtin_functions(NeReLaBasic& vm);
