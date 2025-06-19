// NeReLaBasicInterpreter.cpp
#include "NeReLaBasic.hpp"
#include "Commands.hpp" // Required for Commands::do_run
#include "Error.hpp"    // Required for Error::print
#include <windows.h> 
#ifdef JDCOM         // also define: NOMINMAX!!!!!
#include <objbase.h> // Required for CoInitializeEx, CoUninitialize
#endif 

int main(int argc, char* argv[]) {
    // Initialize COM for the current thread
    // COINIT_APARTMENTTHREADED for single-threaded apartment (most common for UI components like Excel)
    // COINIT_MULTITHREADED for multi-threaded apartment (less common for OLE Automation)
#ifdef JDCOM
    HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    if (FAILED(hr)) {
        // Handle COM initialization error (e.g., print a message and exit)
        MessageBoxA(NULL, "Failed to initialize COM!", "Error", MB_ICONERROR);
        return 1;
    }
#endif

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Create an instance of our interpreter
    NeReLaBasic interpreter;

    // Check if a command-line argument (a filename) was provided
    if (argc > 1) {
        std::string filename = argv[1];

        // Use the new method to load the source file
        if (interpreter.loadSourceFromFile(filename)) {
            // File loaded successfully. Now, we can execute the same logic
            // as the RUN command to compile and execute the code.
            Commands::do_run(interpreter);
        }
        // Note: If do_run encounters a runtime error, it is handled internally
        // by the Error::print() call within the execution loop.
    }
    else {
        // No filename was provided, so start the standard interactive REPL
        interpreter.start();
    }

    // Uninitialize COM when the application exits
#ifdef JDCOM
    CoUninitialize();
#endif

    return 0;
}