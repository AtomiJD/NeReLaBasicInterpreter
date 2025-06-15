#include "Commands.hpp"
#include "NeReLaBasic.hpp"
#include "BuiltinFunctions.hpp"
#include "TextIO.hpp"
#include "Error.hpp"
#include <chrono>
#include <cmath> // For sin, cos, etc.
#include <conio.h>
#include <algorithm>    // For std::transform
#include <string>       // For std::string, std::to_string
#include <vector>       // For std::vector
#include <filesystem>   
#include <iostream>     
#include <regex>
#include <iomanip> 
#include <sstream>

namespace fs = std::filesystem;

// We need access to the helper functions for type conversion
bool to_bool(const BasicValue& val);
double to_double(const BasicValue& val);
std::string to_string(const BasicValue& val);

// Converts a simple wildcard string (*, ?) to a valid ECMA-style regex string.
std::string wildcard_to_regex(const std::string& wildcard) {
    std::string regex_str;
    // Anchor the pattern to match the whole string.
    regex_str += '^';
    for (char c : wildcard) {
        switch (c) {
        case '*':
            regex_str += ".*"; // * matches any sequence of characters
            break;
        case '?':
            regex_str += '.';  // ? matches any single character
            break;
            // Escape special regex characters
        case '.':
        case '\\':
        case '+':
        case '(':
        case ')':
        case '{':
        case '}':
        case '[':
        case ']':
        case '^':
        case '$':
        case '|':
            regex_str += '\\';
            regex_str += c;
            break;
        default:
            regex_str += c;
        }
    }
    // Anchor the pattern to match the whole string.
    regex_str += '$';
    return regex_str;
}

//=========================================================
// C++ Implementations of our Native BASIC Functions
//=========================================================

// --- String Functions ---

// LEN(string_expression)
BasicValue builtin_len(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, 0); // Wrong number of arguments
        return 0.0;
    }

    const BasicValue& val = args[0];
    // Check if the argument is a string that matches an array name
    if (std::holds_alternative<std::string>(val)) {
        std::string name = to_upper(std::get<std::string>(val));
        if (vm.arrays.count(name)) {
            // It's a reference to an array! Return the array's size.
            return static_cast<double>(vm.arrays.at(name).size());
        }
    }
    // Otherwise, treat it as a normal string and return its length.
    return static_cast<double>(to_string(val).length());
}

// LEFT$(string, n)
BasicValue builtin_left_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) return std::string("");
    std::string source = to_string(args[0]);
    int count = static_cast<int>(to_double(args[1]));
    if (count < 0) count = 0;
    return source.substr(0, count);
}

// RIGHT$(string, n)
BasicValue builtin_right_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) return std::string("");
    std::string source = to_string(args[0]);
    int count = static_cast<int>(to_double(args[1]));
    if (count < 0) count = 0;
    if (count > source.length()) count = source.length();
    return source.substr(source.length() - count);
}

// MID$(string, start, [length]) - Overloaded
BasicValue builtin_mid_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
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
BasicValue builtin_lcase_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    std::string s = to_string(args[0]);
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

// UCASE$(string)
BasicValue builtin_ucase_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    std::string s = to_string(args[0]);
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::toupper(c); });
    return s;
}

// TRIM$(string)
BasicValue builtin_trim_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    std::string s = to_string(args[0]);
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    s.erase(s.find_last_not_of(" \t\n\r") + 1);
    return s;
}

// CHR$(number)
BasicValue builtin_chr_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return std::string("");
    char c = static_cast<char>(static_cast<int>(to_double(args[0])));
    return std::string(1, c);
}

// ASC(string)
BasicValue builtin_asc(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    std::string s = to_string(args[0]);
    if (s.empty()) return 0.0;
    return static_cast<double>(static_cast<unsigned char>(s[0]));
}

// INSTR([start], haystack$, needle$) - Overloaded
BasicValue builtin_instr(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
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
BasicValue builtin_inkey(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
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

// VAL(string_expression) -> number
BasicValue builtin_val(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, 0); // Wrong number of arguments
        return 0.0;
    }
    std::string s = to_string(args[0]);
    try {
        // std::stod will parse the string until it finds a non-numeric character.
        // This behavior is very similar to classic BASIC VAL().
        return std::stod(s);
    }
    catch (const std::exception&) {
        // If the string is not a valid number at all (e.g., "hello")
        return 0.0;
    }
}

// STR$(numeric_expression) -> string
BasicValue builtin_str_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, 0); // Wrong number of arguments
        return std::string("");
    }
    // to_string is already a helper in your project that does this conversion.
    return to_string(args[0]);
}


// --- Arithmetic Functions ---

// SIN(numeric_expression)
BasicValue builtin_sin(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    return std::sin(to_double(args[0]));
}

// COS(numeric_expression)
BasicValue builtin_cos(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    return std::cos(to_double(args[0]));
}

// TAN(numeric_expression)
BasicValue builtin_tan(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    return std::tan(to_double(args[0]));
}

// SQR(numeric_expression) - Square Root
BasicValue builtin_sqr(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) return 0.0;
    double val = to_double(args[0]);
    return (val < 0) ? 0.0 : std::sqrt(val); // Return 0 for negative input
}

// RND(numeric_expression) -> returns a random number between 0.0 and 1.0
BasicValue builtin_rnd(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line); // Wrong number of arguments
        return 0.0;
    }

    // Classic BASIC RND(1) returns a value between 0.0 and 0.999...
    // We can ignore the argument's value for this simple, standard implementation.
    // RAND_MAX is a constant defined in <cstdlib>.
    return static_cast<double>(rand()) / (RAND_MAX + 1.0);
}

// --- Date and Time Functions ---
// 
// TICK() -> returns milliseconds since the program started
BasicValue builtin_tick(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
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
// NOW() -> returns a DateTime object for the current moment
BasicValue builtin_now(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (!args.empty()) {
        Error::set(8, vm.runtime_current_line); // Wrong number of arguments
        return false;
    }
    return DateTime{}; // Creates a new DateTime object, which defaults to now
}

// DATE$() -> returns the current date as a string "YYYY-MM-DD"
BasicValue builtin_date_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (!args.empty()) {
        Error::set(8, vm.runtime_current_line);
        return std::string("");
    }
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
#pragma warning(suppress : 4996) // Suppress warning for std::localtime
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d");
    return ss.str();
}

// TIME$() -> returns the current time as a string "HH:MM:SS"
BasicValue builtin_time_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (!args.empty()) {
        Error::set(8, vm.runtime_current_line);
        return std::string("");
    }
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
#pragma warning(suppress : 4996) // Suppress warning for std::localtime
    ss << std::put_time(std::localtime(&in_time_t), "%H:%M:%S");
    return ss.str();
}

// Helper to safely add time units to a time_t
time_t add_to_tm(time_t base_time, int years, int months, int days, int hours, int minutes, int seconds) {
    struct tm timeinfo;
    localtime_s(&timeinfo, &base_time); // Use safe version

    timeinfo.tm_year += years;
    timeinfo.tm_mon += months;
    timeinfo.tm_mday += days;
    timeinfo.tm_hour += hours;
    timeinfo.tm_min += minutes;
    timeinfo.tm_sec += seconds;

    return mktime(&timeinfo); // mktime normalizes the date/time components
}

// DATEADD(part$, number, dateValue)
BasicValue builtin_dateadd(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 3) {
        Error::set(8, vm.runtime_current_line); // Wrong number of arguments
        return false;
    }

    std::string part = to_upper(to_string(args[0]));
    int number = static_cast<int>(to_double(args[1]));

    DateTime start_date;
    if (std::holds_alternative<DateTime>(args[2])) {
        start_date = std::get<DateTime>(args[2]);
    }
    else {
        Error::set(15, vm.runtime_current_line); // Type mismatch for 3rd arg
        return false;
    }

    time_t start_time_t = std::chrono::system_clock::to_time_t(start_date.time_point);
    time_t new_time_t;

    if (part == "YYYY") new_time_t = add_to_tm(start_time_t, number, 0, 0, 0, 0, 0);
    else if (part == "M") new_time_t = add_to_tm(start_time_t, 0, number, 0, 0, 0, 0);
    else if (part == "D") new_time_t = add_to_tm(start_time_t, 0, 0, number, 0, 0, 0);
    else if (part == "H") new_time_t = add_to_tm(start_time_t, 0, 0, 0, number, 0, 0);
    else if (part == "N") new_time_t = add_to_tm(start_time_t, 0, 0, 0, 0, number, 0);
    else if (part == "S") new_time_t = add_to_tm(start_time_t, 0, 0, 0, 0, 0, number);
    else {
        Error::set(1, vm.runtime_current_line); // Invalid interval string
        return false;
    }

    return DateTime{ std::chrono::system_clock::from_time_t(new_time_t) };
}


// --- Procedures ---
BasicValue builtin_cls(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    // Procedures can still take arguments, but CLS doesn't need any.
    // We could add error checking for args.size() if we wanted.
    TextIO::clearScreen();
    return false; // Procedures must return something; the value is ignored.
}

BasicValue builtin_locate(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line);
        return false;
    }

    int row = static_cast<int>(to_double(args[0]));
    int col = static_cast<int>(to_double(args[1]));

    TextIO::locate(row, col);

    return false; // Procedures return a dummy value.
}

// SLEEP milliseconds
BasicValue builtin_sleep(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line); // Wrong number of arguments
        return false;
    }
    int milliseconds = static_cast<int>(to_double(args[0]));
    if (milliseconds > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }
    return false;
}

// CURSOR state (0 for off, 1 for on)
BasicValue builtin_cursor(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line);
        return false;
    }
    bool state = to_bool(args[0]); // to_bool handles 0/1 conversion nicely
    TextIO::setCursor(state);
    return false;
}

// --- Filesystem ---
// DIR [path_string]
BasicValue builtin_dir(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    try {
        fs::path target_path("."); // Default to current directory
        std::string wildcard = "*";   // Default to all files

        if (!args.empty()) {
            fs::path full_arg_path(to_string(args[0]));
            if (full_arg_path.has_filename() && full_arg_path.filename().string().find_first_of("*?") != std::string::npos) {
                wildcard = full_arg_path.filename().string();
                target_path = full_arg_path.has_parent_path() ? full_arg_path.parent_path() : ".";
            }
            else {
                target_path = full_arg_path;
            }
        }

        if (!fs::exists(target_path) || !fs::is_directory(target_path)) {
            TextIO::print("Directory not found: " + target_path.string() + "\n");
            return false;
        }

        // Create the regex object from our wildcard pattern
        std::regex pattern(wildcard_to_regex(wildcard), std::regex::icase); // std::regex::icase for case-insensitivity

        for (const auto& entry : fs::directory_iterator(target_path)) {
            std::string filename_str = entry.path().filename().string();

            // Check if the filename matches our regex pattern
            if (std::regex_match(filename_str, pattern)) {
                std::string size_str;
                if (!entry.is_directory()) {
                    try {
                        size_str = std::to_string(fs::file_size(entry));
                    }
                    catch (fs::filesystem_error&) {
                        size_str = "<ERR>";
                    }
                }
                else {
                    size_str = "<DIR>";
                }

                TextIO::print(filename_str);
                int padding_needed = 25 - static_cast<int>(filename_str.length());

                // Only print padding if the filename is shorter than the column width.
                if (padding_needed > 0) {
                    for (int i = 0; i < padding_needed; ++i) {
                        TextIO::print(" ");
                    }
                }
                TextIO::print(size_str + "\n");
            }
        }
    }
    catch (const std::regex_error& e) {
        TextIO::print("Invalid wildcard pattern: " + std::string(e.what()) + "\n");
    }
    catch (const fs::filesystem_error& e) {
        TextIO::print("Error accessing directory: " + std::string(e.what()) + "\n");
    }

    return false; // Procedures return a dummy value
}

// CD path_string
BasicValue builtin_cd(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, 0); // Wrong number of arguments
        return false;
    }

    std::string path_str = to_string(args[0]);
    try {
        fs::current_path(path_str); // This function changes the current working directory
        TextIO::print("Current directory is now: " + fs::current_path().string() + "\n");
    }
    catch (const fs::filesystem_error& e) {
        TextIO::print("Error changing directory: " + std::string(e.what()) + "\n");
    }
    return false;
}

// PWD
BasicValue builtin_pwd(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    try {
        TextIO::print(fs::current_path().string() + "\n");
    }
    catch (const fs::filesystem_error& e) {
        TextIO::print("Error getting current directory: " + std::string(e.what()) + "\n");
    }
    return false;
}

// MKDIR path_string
BasicValue builtin_mkdir(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, 0); // Wrong number of arguments
        return false;
    }
    std::string path_str = to_string(args[0]);
    try {
        if (fs::create_directory(path_str)) {
            TextIO::print("Directory created: " + path_str + "\n");
        }
        else {
            TextIO::print("Directory already exists or error.\n");
        }
    }
    catch (const fs::filesystem_error& e) {
        TextIO::print("Error creating directory: " + std::string(e.what()) + "\n");
    }
    return false;
}

// KILL path_string (deletes a file)
BasicValue builtin_kill(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, 0); // Wrong number of arguments
        return false;
    }
    std::string path_str = to_string(args[0]);
    try {
        if (fs::remove(path_str)) {
            TextIO::print("File deleted: " + path_str + "\n");
        }
        else {
            TextIO::print("File not found or is a non-empty directory.\n");
        }
    }
    catch (const fs::filesystem_error& e) {
        TextIO::print("Error deleting file: " + std::string(e.what()) + "\n");
    }
    return false;
}


// --- GUI and Graphic and more ---
// Handles: COLOR fg, bg
BasicValue builtin_color(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line); // Wrong number of arguments
        return false; // Procedures return a dummy value
    }

    // Convert arguments to integers
    int fg = static_cast<int>(to_double(args[0]));
    int bg = static_cast<int>(to_double(args[1]));

    // Call the underlying TextIO function
    TextIO::setColor(fg, bg);

    return false; // Procedures must return something; the value is ignored.
}

// --- GRAPHICS PROCEDURES ---


// --- The Registration Function ---
void register_builtin_functions(NeReLaBasic& vm, NeReLaBasic::FunctionTable& table_to_populate) {
    // Helper lambda to make registration cleaner
    auto register_func = [&](const std::string& name, int arity, NeReLaBasic::NativeFunction func_ptr) {
        NeReLaBasic::FunctionInfo info;
        info.name = name;
        info.arity = arity;
        info.native_impl = func_ptr;
        table_to_populate[to_upper(info.name)] = info;
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
    register_func("VAL", 1, builtin_val);
    register_func("STR$", 1, builtin_str_str);
    
    // --- Register Math Functions ---
    register_func("SIN", 1, builtin_sin);
    register_func("COS", 1, builtin_cos);
    register_func("TAN", 1, builtin_tan);
    register_func("SQR", 1, builtin_sqr);
    register_func("RND", 1, builtin_rnd);


    // --- Register Time Functions ---
    register_func("TICK", 0, builtin_tick);
    register_func("NOW", 0, builtin_now);
    register_func("DATE$", 0, builtin_date_str);
    register_func("TIME$", 0, builtin_time_str);
    register_func("DATEADD", 3, builtin_dateadd);

    // --- Register Procedures ---

    auto register_proc = [&](const std::string& name, int arity, NeReLaBasic::NativeFunction func_ptr) {
        NeReLaBasic::FunctionInfo info;
        info.name = name;
        info.arity = arity;
        info.native_impl = func_ptr;
        info.is_procedure = true; // Mark this as a procedure
        table_to_populate[to_upper(info.name)] = info;
        };

    // --- Register Methods ---
    register_proc("CLS", 0, builtin_cls);
    register_proc("LOCATE", 2, builtin_locate);
    register_proc("SLEEP", 1, builtin_sleep);   
    register_proc("CURSOR", 1, builtin_cursor);

    register_proc("DIR", -1, builtin_dir);  // -1 for optional argument
    register_proc("CD", 1, builtin_cd);
    register_proc("PWD", 0, builtin_pwd);
    register_proc("COLOR", 2, builtin_color);
    register_proc("MKDIR", 1, builtin_mkdir); 
    register_proc("KILL", 1, builtin_kill);   

}

