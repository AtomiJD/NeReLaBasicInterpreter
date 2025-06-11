#include "BuiltinFunctions.hpp"
#include "Commands.hpp"
#include "NeReLaBasic.hpp"
#include <chrono>
#include <cmath> // For sin, cos, etc.
#include <conio.h>
#include <algorithm>    // For std::transform
#include <string>       // For std::string, std::to_string
#include <vector>       // For std::vector

// We need access to the helper functions for type conversion
bool to_bool(const BasicValue& val);
double to_double(const BasicValue& val);
std::string to_string(const BasicValue& val);


//=========================================================
// C++ Implementations of our Native BASIC Functions
//=========================================================

// --- String Functions ---

// LEN(string_expression)
BasicValue builtin_len(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0; // Return 0 on error
    return static_cast<double>(to_string(args[0]).length());
}

// LEFT$(string, n)
BasicValue builtin_left_str(const std::vector<BasicValue>& args) {
    if (args.size() != 2) return std::string("");
    std::string source = to_string(args[0]);
    int count = static_cast<int>(to_double(args[1]));
    if (count < 0) count = 0;
    return source.substr(0, count);
}

// RIGHT$(string, n)
BasicValue builtin_right_str(const std::vector<BasicValue>& args) {
    if (args.size() != 2) return std::string("");
    std::string source = to_string(args[0]);
    int count = static_cast<int>(to_double(args[1]));
    if (count < 0) count = 0;
    if (count > source.length()) count = source.length();
    return source.substr(source.length() - count);
}

// MID$(string, start, [length]) - Overloaded
BasicValue builtin_mid_str(const std::vector<BasicValue>& args) {
    if (args.size() < 2 || args.size() > 3) return std::string("");

    std::string source = to_string(args[0]);
    int start = static_cast<int>(to_double(args[1])) - 1; // BASIC is 1-indexed
    if (start < 0) start = 0;

    if (args.size() == 2) { // MID$(str, start) -> get rest of string
        return source.substr(start);
    }
    else { // MID$(str, start, len)
        int length = static_cast<int>(to_double(args[2]));
        if (length < 0) length = 0;
        return source.substr(start, length);
    }
}

// LCASE$(string)
BasicValue builtin_lcase_str(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    std::string s = to_string(args[0]);
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

// UCASE$(string)
BasicValue builtin_ucase_str(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    std::string s = to_string(args[0]);
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return s;
}

// TRIM$(string)
BasicValue builtin_trim_str(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    std::string s = to_string(args[0]);
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    s.erase(s.find_last_not_of(" \t\n\r") + 1);
    return s;
}

// CHR$(number)
BasicValue builtin_chr_str(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    char c = static_cast<char>(static_cast<int>(to_double(args[0])));
    return std::string(1, c);
}

// ASC(string)
BasicValue builtin_asc(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    std::string s = to_string(args[0]);
    if (s.empty()) return 0.0;
    return static_cast<double>(static_cast<unsigned char>(s[0]));
}

// INSTR([start], haystack$, needle$) - Overloaded
BasicValue builtin_instr(const std::vector<BasicValue>& args) {
    if (args.size() < 2 || args.size() > 3) return 0.0;

    size_t start_pos = 0;
    std::string haystack, needle;

    if (args.size() == 2) {
        haystack = to_string(args[0]);
        needle = to_string(args[1]);
    }
    else {
        start_pos = static_cast<size_t>(to_double(args[0])) - 1;
        haystack = to_string(args[1]);
        needle = to_string(args[2]);
    }

    if (start_pos >= haystack.length()) return 0.0;

    size_t found_pos = haystack.find(needle, start_pos);

    if (found_pos == std::string::npos) {
        return 0.0; // Not found
    }
    else {
        return static_cast<double>(found_pos + 1); // Return 1-indexed position
    }
}
// INKEY$()
BasicValue builtin_inkey(const std::vector<BasicValue>& args) {
    // This function takes no arguments
    if (!args.empty()) {
        // Optional: Set an error for "Too many arguments"
        return std::string(""); // Return empty string on error
    }

    // _kbhit() checks if a key has been pressed without waiting.
    if (_kbhit()) {
        // A key is waiting in the buffer. _getch() reads it without echoing to screen.
        char c = _getch();
        return std::string(1, c); // Return a string containing the single character
    }

    // No key was pressed, return an empty string.
    return std::string("");
}


// --- Arithmetic Functions ---

// SIN(numeric_expression)
BasicValue builtin_sin(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    return std::sin(to_double(args[0]));
}

// COS(numeric_expression)
BasicValue builtin_cos(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    return std::cos(to_double(args[0]));
}

// TAN(numeric_expression)
BasicValue builtin_tan(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    return std::tan(to_double(args[0]));
}

// SQR(numeric_expression) - Square Root
BasicValue builtin_sqr(const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    double val = to_double(args[0]);
    return (val < 0) ? 0.0 : std::sqrt(val); // Return 0 for negative input
}

// --- Time Functions ---
// TICK() -> returns milliseconds since the program started
BasicValue builtin_tick(const std::vector<BasicValue>& args) {
    // This function takes no arguments
    if (!args.empty()) {
        // Optional: Set an error for "Wrong number of arguments"
        return 0.0;
    }

    // Get the current time point from a steady (monotonic) clock
    auto now = std::chrono::steady_clock::now();

    // Calculate the duration since the clock's epoch (usually program start)
    // and return it as a double representing milliseconds.
    return static_cast<double>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count()
        );
}


// --- The Registration Function ---

void register_builtin_functions(NeReLaBasic& vm) {
    // Helper lambda to make registration cleaner
    auto register_func = [&](const std::string& name, int arity, NativeFunction func_ptr) {
        NeReLaBasic::FunctionInfo info;
        info.name = name;
        info.arity = arity;
        info.native_impl = func_ptr;
        vm.function_table[to_upper(info.name)] = info;
    };

    // --- Register String Functions ---
    register_func("LEFT$", 2, builtin_left_str);
    register_func("RIGHT$", 2, builtin_right_str);
    register_func("MID$", -1, builtin_mid_str); // -1 for variable args
    register_func("LEN", 1, builtin_len);
    register_func("ASC", 1, builtin_asc);
    register_func("CHR$", 1, builtin_chr_str);
    register_func("INSTR", -1, builtin_instr); // -1 for variable args
    register_func("LCASE$", 1, builtin_lcase_str);
    register_func("UCASE$", 1, builtin_ucase_str);
    register_func("TRIM$", 1, builtin_trim_str);
    register_func("INKEY$", 0, builtin_inkey);
    
    // --- Register Math Functions ---
    register_func("SIN", 1, builtin_sin);
    register_func("COS", 1, builtin_cos);
    register_func("TAN", 1, builtin_tan);
    register_func("SQR", 1, builtin_sqr);

    // --- Register Time Functions ---
    register_func("TICK", 0, builtin_tick);

}