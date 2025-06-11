// NeReLaBasicInterpreter.cpp
#include "NeReLaBasic.hpp"
#include <windows.h> 

int main() {

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // Create an instance of our interpreter
    NeReLaBasic interpreter;

    // Start the main loop
    interpreter.start();

    return 0;
}