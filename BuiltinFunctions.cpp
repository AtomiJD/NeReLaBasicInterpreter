#include "Commands.hpp"
#include "NeReLaBasic.hpp"
#include "BuiltinFunctions.hpp"
#include "TextIO.hpp"
#include "Error.hpp"
#include "Types.hpp"
#include <chrono>
#include <cmath> // For sin, cos, etc.
#include <conio.h>
#include <algorithm>    // For std::transform
#include <string>       // For std::string, std::to_string
#include <vector>       // For std::vector
#include <filesystem>   
#include <fstream>
#include <iostream>     
#include <regex>
#include <iomanip> 
#include <sstream>
#include <iostream>
#include <unordered_set>

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

// LEN(string_expression) or LEN(array_variable)
BasicValue builtin_len(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, 0); // Wrong number of arguments
        return 0.0;
    }

    const BasicValue& val = args[0];

    // --- Case 1: The argument is ALREADY an array ---
    if (std::holds_alternative<std::shared_ptr<Array>>(val)) {
        const auto& arr_ptr = std::get<std::shared_ptr<Array>>(val);
        if (arr_ptr) {
            // Create a new vector (1D Array) to hold the shape information
            auto shape_vector_ptr = std::make_shared<Array>();
            shape_vector_ptr->shape = { arr_ptr->shape.size() };
            for (size_t dim : arr_ptr->shape) {
                shape_vector_ptr->data.push_back(static_cast<double>(dim));
            }
            return shape_vector_ptr;
        }
        else {
            // This case is for a null array pointer, return 0.
            return 0.0;
        }
    }

    // --- Case 2: The argument is a string that might be a variable name ---
    if (std::holds_alternative<std::string>(val)) {
        std::string name = to_upper(std::get<std::string>(val));
        // Check if a variable with this name exists
        if (vm.variables.count(name)) {
            const BasicValue& var_val = vm.variables.at(name);
            // Check if that variable holds an array
            if (std::holds_alternative<std::shared_ptr<Array>>(var_val)) {
                const auto& arr_ptr = std::get<std::shared_ptr<Array>>(var_val);
                if (arr_ptr) {
                    auto shape_vector_ptr = std::make_shared<Array>();
                    shape_vector_ptr->shape = { arr_ptr->shape.size() };
                    for (size_t dim : arr_ptr->shape) {
                        shape_vector_ptr->data.push_back(static_cast<double>(dim));
                    }
                    return shape_vector_ptr;
                }
            }
        }
    }

    // --- Case 3: Fallback to original behavior (length of string representation) ---
    return static_cast<double>(to_string(val).length());
}


// --- SDL Integration ---

#ifdef SDL3
// SCREEN width, height, [title$]
// Initializes the graphics screen.
BasicValue builtin_screen(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() < 2 || args.size() > 3) {
        Error::set(8, vm.runtime_current_line);
        return false;
    }

    int width = static_cast<int>(to_double(args[0]));
    int height = static_cast<int>(to_double(args[1]));
    std::string title = "jdBasic Graphics";
    if (args.size() == 3) {
        title = to_string(args[2]);
    }

    if (!vm.graphics_system.init(title, width, height)) {
        Error::set(1, vm.runtime_current_line); // Generic error
    }

    return false; // Procedures return a dummy value
}

// PSET x, y, [r, g, b]
// Sets a pixel at a specific coordinate to a color.
BasicValue builtin_pset(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() < 2 || args.size() > 5) { Error::set(8, vm.runtime_current_line); return false; }

    int x = static_cast<int>(to_double(args[0]));
    int y = static_cast<int>(to_double(args[1]));
    Uint8 r = 255, g = 255, b = 255; // Default to white

    if (args.size() == 5) {
        r = static_cast<Uint8>(to_double(args[2]));
        g = static_cast<Uint8>(to_double(args[3]));
        b = static_cast<Uint8>(to_double(args[4]));
    }
    vm.graphics_system.pset(x, y, r, g, b);
    return false;
}

// SCREENFLIP
// Updates the screen to show all drawing done since the last flip.
BasicValue builtin_screenflip(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (!args.empty()) { Error::set(8, vm.runtime_current_line); return false; }
    vm.graphics_system.update_screen();
    return false;
}

// LINE x1, y1, x2, y2, [r, g, b]
BasicValue builtin_line(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() < 4 || args.size() > 7) { Error::set(8, vm.runtime_current_line); return false; }

    int x1 = static_cast<int>(to_double(args[0]));
    int y1 = static_cast<int>(to_double(args[1]));
    int x2 = static_cast<int>(to_double(args[2]));
    int y2 = static_cast<int>(to_double(args[3]));
    Uint8 r = 255, g = 255, b = 255;

    if (args.size() == 7) {
        r = static_cast<Uint8>(to_double(args[4]));
        g = static_cast<Uint8>(to_double(args[5]));
        b = static_cast<Uint8>(to_double(args[6]));
    }
    vm.graphics_system.line(x1, y1, x2, y2, r, g, b);
    return false;
}

// RECT x, y, w, h, [r, g, b], [fill_bool]
BasicValue builtin_rect(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() < 4 || args.size() > 8) { Error::set(8, vm.runtime_current_line); return false; }

    int x = static_cast<int>(to_double(args[0]));
    int y = static_cast<int>(to_double(args[1]));
    int w = static_cast<int>(to_double(args[2]));
    int h = static_cast<int>(to_double(args[3]));
    Uint8 r = 255, g = 255, b = 255;
    bool fill = false;

    if (args.size() >= 7) {
        r = static_cast<Uint8>(to_double(args[4]));
        g = static_cast<Uint8>(to_double(args[5]));
        b = static_cast<Uint8>(to_double(args[6]));
    }
    if (args.size() == 8) {
        fill = to_bool(args[7]);
    }
    vm.graphics_system.rect(x, y, w, h, r, g, b, fill);
    return false;
}

// CIRCLE x, y, radius, [r, g, b]
BasicValue builtin_circle(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() < 3 || args.size() > 6) { Error::set(8, vm.runtime_current_line); return false; }

    int x = static_cast<int>(to_double(args[0]));
    int y = static_cast<int>(to_double(args[1]));
    int radius = static_cast<int>(to_double(args[2]));
    Uint8 r = 255, g = 255, b = 255;

    if (args.size() == 6) {
        r = static_cast<Uint8>(to_double(args[3]));
        g = static_cast<Uint8>(to_double(args[4]));
        b = static_cast<Uint8>(to_double(args[5]));
    }
    vm.graphics_system.circle(x, y, radius, r, g, b);
    return false;
}

#endif


// --- String Functions ---

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

// SPLIT(source_string$, delimiter_string$) -> array
// Splits a string into an array of substrings based on a delimiter.
BasicValue builtin_split(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line);
        return {};
    }

    std::string source = to_string(args[0]);
    std::string delimiter = to_string(args[1]);

    if (delimiter.empty()) {
        Error::set(1, vm.runtime_current_line); // Cannot split by empty delimiter
        return {};
    }

    auto result_ptr = std::make_shared<Array>();
    size_t start = 0;
    size_t end = source.find(delimiter);

    while (end != std::string::npos) {
        result_ptr->data.push_back(source.substr(start, end - start));
        start = end + delimiter.length();
        end = source.find(delimiter, start);
    }
    // Add the last token
    result_ptr->data.push_back(source.substr(start, end));

    result_ptr->shape = { result_ptr->data.size() };
    return result_ptr;
}


// --- Vector and Matrx functions

// --- Array Reduction Functions ---

// Helper macro to reduce boilerplate for numeric reduction functions
#define NUMERIC_REDUCTION_BOILERPLATE(function_name, error_code_on_mismatch) \
    if (args.size() != 1) { \
        Error::set(8, vm.runtime_current_line); \
        return 0.0; \
    } \
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) { \
        Error::set(15, vm.runtime_current_line); \
        return 0.0; \
    } \
    const auto& arr_ptr = std::get<std::shared_ptr<Array>>(args[0]); \
    if (!arr_ptr || arr_ptr->data.empty()) { \
        return 0.0; \
    }

// SUM(array) -> number
BasicValue builtin_sum(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    NUMERIC_REDUCTION_BOILERPLATE(builtin_sum, 15);
    double total = 0.0;
    for (const auto& val : arr_ptr->data) {
        total += to_double(val);
    }
    return total;
}

// PRODUCT(array) -> number
BasicValue builtin_product(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    NUMERIC_REDUCTION_BOILERPLATE(builtin_product, 15);
    double total = 1.0;
    for (const auto& val : arr_ptr->data) {
        total *= to_double(val);
    }
    return total;
}

// MIN(array) -> number
BasicValue builtin_min(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    NUMERIC_REDUCTION_BOILERPLATE(builtin_min, 15);
    double min_val = to_double(arr_ptr->data[0]);
    for (size_t i = 1; i < arr_ptr->data.size(); ++i) {
        double current_val = to_double(arr_ptr->data[i]);
        if (current_val < min_val) {
            min_val = current_val;
        }
    }
    return min_val;
}

// MAX(array) -> number
BasicValue builtin_max(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    NUMERIC_REDUCTION_BOILERPLATE(builtin_max, 15);
    double max_val = to_double(arr_ptr->data[0]);
    for (size_t i = 1; i < arr_ptr->data.size(); ++i) {
        double current_val = to_double(arr_ptr->data[i]);
        if (current_val > max_val) {
            max_val = current_val;
        }
    }
    return max_val;
}

// Helper macro for boolean reduction functions
#define BOOLEAN_REDUCTION_BOILERPLATE(function_name, error_code_on_mismatch) \
    if (args.size() != 1) { \
        Error::set(8, vm.runtime_current_line); \
        return false; \
    } \
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) { \
        Error::set(15, vm.runtime_current_line); \
        return false; \
    } \
    const auto& arr_ptr = std::get<std::shared_ptr<Array>>(args[0]); \
    if (!arr_ptr) return false;

// ANY(array) -> boolean
BasicValue builtin_any(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    BOOLEAN_REDUCTION_BOILERPLATE(builtin_any, 15);
    if (arr_ptr->data.empty()) return false; // ANY of an empty set is false
    for (const auto& val : arr_ptr->data) {
        if (to_bool(val)) {
            return true; // Short-circuit
        }
    }
    return false;
}

// ALL(array) -> boolean
BasicValue builtin_all(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    BOOLEAN_REDUCTION_BOILERPLATE(builtin_all, 15);
    if (arr_ptr->data.empty()) return true; // ALL of an empty set is true
    for (const auto& val : arr_ptr->data) {
        if (!to_bool(val)) {
            return false; // Short-circuit
        }
    }
    return true;
}

// IOTA(N) -> vector
// Generates a vector of numbers from 1 to N.
BasicValue builtin_iota(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line); // Wrong number of arguments
        return {}; // Return an empty BasicValue
    }
    int count = static_cast<int>(to_double(args[0]));
    if (count < 0) count = 0;

    auto new_array_ptr = std::make_shared<Array>();
    new_array_ptr->shape = { (size_t)count };
    new_array_ptr->data.reserve(count);

    for (int i = 1; i <= count; ++i) {
        new_array_ptr->data.push_back(static_cast<double>(i));
    }

    return new_array_ptr;
}

// RESHAPE(source_array, shape_vector) -> array
// Creates a new array with a new shape from the data of a source array.
BasicValue builtin_reshape(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line);
        return {};
    }

    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0]) || !std::holds_alternative<std::shared_ptr<Array>>(args[1])) {
        Error::set(15, vm.runtime_current_line); // Type Mismatch
        return {};
    }

    const auto& source_array_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    const auto& shape_vector_ptr = std::get<std::shared_ptr<Array>>(args[1]);

    if (!source_array_ptr || !shape_vector_ptr) return {}; // Null pointers

    // Create the new shape from the shape_vector
    std::vector<size_t> new_shape;
    for (const auto& val : shape_vector_ptr->data) {
        new_shape.push_back(static_cast<size_t>(to_double(val)));
    }

    auto new_array_ptr = std::make_shared<Array>();
    new_array_ptr->shape = new_shape;
    size_t new_total_size = new_array_ptr->size();
    new_array_ptr->data.reserve(new_total_size);

    // APL's reshape cycles through the source data if needed.
    if (source_array_ptr->data.empty()) {
        new_array_ptr->data.assign(new_total_size, 0.0); // Fill with default if source is empty
    }
    else {
        for (size_t i = 0; i < new_total_size; ++i) {
            new_array_ptr->data.push_back(source_array_ptr->data[i % source_array_ptr->data.size()]);
        }
    }

    return new_array_ptr;
}

// REVERSE(array) -> array
// Reverses the elements of an array along its last dimension.
BasicValue builtin_reverse(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line);
        return {};
    }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        Error::set(15, vm.runtime_current_line);
        return {};
    }
    const auto& source_array_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    if (!source_array_ptr || source_array_ptr->data.empty()) return source_array_ptr;

    auto new_array_ptr = std::make_shared<Array>();
    new_array_ptr->shape = source_array_ptr->shape;

    size_t last_dim_size = source_array_ptr->shape.back();
    size_t num_slices = source_array_ptr->data.size() / last_dim_size;

    for (size_t i = 0; i < num_slices; ++i) {
        size_t slice_start = i * last_dim_size;
        // Get a copy of the slice to reverse
        std::vector<BasicValue> slice(
            source_array_ptr->data.begin() + slice_start,
            source_array_ptr->data.begin() + slice_start + last_dim_size
        );
        // Reverse it
        std::reverse(slice.begin(), slice.end());
        // Insert the reversed slice into the new data
        new_array_ptr->data.insert(new_array_ptr->data.end(), slice.begin(), slice.end());
    }

    return new_array_ptr;
}

// SLICE(matrix, dimension, index) -> vector
// Extracts a slice (row or column) from a 2D matrix.
BasicValue builtin_slice(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 3) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) { Error::set(15, vm.runtime_current_line); return {}; }

    const auto& matrix_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    int dimension = static_cast<int>(to_double(args[1]));
    int index = static_cast<int>(to_double(args[2]));

    if (!matrix_ptr || matrix_ptr->shape.size() != 2) {
        Error::set(15, vm.runtime_current_line); // Must be a 2D matrix
        return {};
    }

    size_t rows = matrix_ptr->shape[0];
    size_t cols = matrix_ptr->shape[1];
    auto result_ptr = std::make_shared<Array>();

    if (dimension == 0) { // Slice a row
        if (index < 0 || (size_t)index >= rows) { Error::set(10, vm.runtime_current_line); return {}; } // Index out of bounds

        result_ptr->shape = { cols };
        size_t start_pos = index * cols;
        result_ptr->data.assign(matrix_ptr->data.begin() + start_pos, matrix_ptr->data.begin() + start_pos + cols);
    }
    else if (dimension == 1) { // Slice a column
        if (index < 0 || (size_t)index >= cols) { Error::set(10, vm.runtime_current_line); return {}; } // Index out of bounds

        result_ptr->shape = { rows };
        result_ptr->data.reserve(rows);
        for (size_t r = 0; r < rows; ++r) {
            result_ptr->data.push_back(matrix_ptr->data[r * cols + index]);
        }
    }
    else {
        Error::set(1, vm.runtime_current_line); // Invalid dimension
        return {};
    }

    return result_ptr;
}

// TRANSPOSE(matrix) -> matrix
// Transposes a 2D matrix.
BasicValue builtin_transpose(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line);
        return {};
    }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        Error::set(15, vm.runtime_current_line);
        return {};
    }
    const auto& source_array_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    if (!source_array_ptr) return source_array_ptr;

    if (source_array_ptr->shape.size() != 2) {
        Error::set(15, vm.runtime_current_line); // Or a more specific "Invalid rank for transpose" error
        return {};
    }

    size_t rows = source_array_ptr->shape[0];
    size_t cols = source_array_ptr->shape[1];

    auto new_array_ptr = std::make_shared<Array>();
    new_array_ptr->shape = { cols, rows }; // New shape is inverted
    new_array_ptr->data.resize(rows * cols);

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            // New position (c, r) gets data from old position (r, c)
            new_array_ptr->data[c * rows + r] = source_array_ptr->data[r * cols + c];
        }
    }

    return new_array_ptr;
}

// MATMUL(matrixA, matrixB) -> matrix
// Performs standard linear algebra matrix multiplication.
BasicValue builtin_matmul(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0]) || !std::holds_alternative<std::shared_ptr<Array>>(args[1])) {
        Error::set(15, vm.runtime_current_line); return {};
    }

    const auto& a_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    const auto& b_ptr = std::get<std::shared_ptr<Array>>(args[1]);

    if (!a_ptr || !b_ptr || a_ptr->shape.size() != 2 || b_ptr->shape.size() != 2) {
        Error::set(15, vm.runtime_current_line); // Must be matrices
        return {};
    }

    size_t rows_a = a_ptr->shape[0];
    size_t cols_a = a_ptr->shape[1];
    size_t rows_b = b_ptr->shape[0];
    size_t cols_b = b_ptr->shape[1];

    if (cols_a != rows_b) {
        Error::set(15, vm.runtime_current_line); // Inner dimensions must match
        return {};
    }

    auto result_ptr = std::make_shared<Array>();
    result_ptr->shape = { rows_a, cols_b };
    result_ptr->data.resize(rows_a * cols_b);

    for (size_t r = 0; r < rows_a; ++r) {
        for (size_t c = 0; c < cols_b; ++c) {
            double dot_product = 0.0;
            for (size_t i = 0; i < cols_a; ++i) { // cols_a is the common dimension
                dot_product += to_double(a_ptr->data[r * cols_a + i]) * to_double(b_ptr->data[i * cols_b + c]);
            }
            result_ptr->data[r * cols_b + c] = dot_product;
        }
    }
    return result_ptr;
}

// OUTER(arrayA, arrayB, operator_string) -> array
// Creates a combination table by applying an operator to all pairs of elements.
BasicValue builtin_outer(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 3) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0]) || !std::holds_alternative<std::shared_ptr<Array>>(args[1])) {
        Error::set(15, vm.runtime_current_line); return {};
    }
    if (!std::holds_alternative<std::string>(args[2])) {
        Error::set(15, vm.runtime_current_line); return {};
    }

    const auto& a_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    const auto& b_ptr = std::get<std::shared_ptr<Array>>(args[1]);
    const std::string op = std::get<std::string>(args[2]);

    if (!a_ptr || !b_ptr) return {};

    auto result_ptr = std::make_shared<Array>();
    result_ptr->shape = a_ptr->shape; // Start with shape of A
    result_ptr->shape.insert(result_ptr->shape.end(), b_ptr->shape.begin(), b_ptr->shape.end()); // Append shape of B
    result_ptr->data.reserve(a_ptr->data.size() * b_ptr->data.size());

    for (const auto& val_a : a_ptr->data) {
        for (const auto& val_b : b_ptr->data) {
            double num_a = to_double(val_a);
            double num_b = to_double(val_b);

            if (op == "+") result_ptr->data.push_back(num_a + num_b);
            else if (op == "-") result_ptr->data.push_back(num_a - num_b);
            else if (op == "*") result_ptr->data.push_back(num_a * num_b);
            else if (op == "/") {
                if (num_b == 0.0) { Error::set(2, vm.runtime_current_line); return {}; }
                result_ptr->data.push_back(num_a / num_b);
            }
            else if (op == "=") result_ptr->data.push_back(num_a == num_b);
            else if (op == ">") result_ptr->data.push_back(num_a > num_b);
            else if (op == "<") result_ptr->data.push_back(num_a < num_b);
            // Add other operators as needed...
            else { Error::set(1, vm.runtime_current_line); return {}; } // Invalid operator
        }
    }
    return result_ptr;
}

// --- Slicing and Sorting Functions ---

// TAKE(N, array) -> vector
// Takes the first N elements (or last N if N is negative) from an array.
BasicValue builtin_take(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[1])) { Error::set(15, vm.runtime_current_line); return {}; }

    int count = static_cast<int>(to_double(args[0]));
    const auto& arr_ptr = std::get<std::shared_ptr<Array>>(args[1]);
    if (!arr_ptr) return {};

    if (count == 0) {
        auto result_ptr = std::make_shared<Array>();
        result_ptr->shape = { 0 };
        return result_ptr;
    }

    auto result_ptr = std::make_shared<Array>();
    if (count > 0) { // Take from start
        size_t num_to_take = std::min((size_t)count, arr_ptr->data.size());
        result_ptr->shape = { num_to_take };
        result_ptr->data.assign(arr_ptr->data.begin(), arr_ptr->data.begin() + num_to_take);
    }
    else { // Take from end
        size_t num_to_take = std::min((size_t)(-count), arr_ptr->data.size());
        result_ptr->shape = { num_to_take };
        result_ptr->data.assign(arr_ptr->data.end() - num_to_take, arr_ptr->data.end());
    }

    return result_ptr;
}

// DROP(N, array) -> vector
// Drops the first N elements (or last N if N is negative) from an array.
BasicValue builtin_drop(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[1])) { Error::set(15, vm.runtime_current_line); return {}; }

    int count = static_cast<int>(to_double(args[0]));
    const auto& arr_ptr = std::get<std::shared_ptr<Array>>(args[1]);
    if (!arr_ptr) return {};

    if (count == 0) return arr_ptr; // Return a copy of the original

    auto result_ptr = std::make_shared<Array>();
    if (count > 0) { // Drop from start
        size_t num_to_drop = std::min((size_t)count, arr_ptr->data.size());
        result_ptr->shape = { arr_ptr->data.size() - num_to_drop };
        result_ptr->data.assign(arr_ptr->data.begin() + num_to_drop, arr_ptr->data.end());
    }
    else { // Drop from end
        size_t num_to_drop = std::min((size_t)(-count), arr_ptr->data.size());
        result_ptr->shape = { arr_ptr->data.size() - num_to_drop };
        result_ptr->data.assign(arr_ptr->data.begin(), arr_ptr->data.end() - num_to_drop);
    }

    return result_ptr;
}

// GRADE(vector) -> vector
// Returns the indices that would sort the vector in ascending order.
BasicValue builtin_grade(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) { Error::set(15, vm.runtime_current_line); return {}; }

    const auto& arr_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    if (!arr_ptr) return {};

    // Create a vector of pairs, storing the original index with each value.
    std::vector<std::pair<BasicValue, size_t>> indexed_values;
    indexed_values.reserve(arr_ptr->data.size());
    for (size_t i = 0; i < arr_ptr->data.size(); ++i) {
        indexed_values.push_back({ arr_ptr->data[i], i });
    }

    // Sort this vector of pairs based on the values.
    std::sort(indexed_values.begin(), indexed_values.end(),
        [](const auto& a, const auto& b) {
            // This comparison logic can be expanded to handle strings, etc.
            // For now, it compares numerically.
            return to_double(a.first) < to_double(b.first);
        }
    );

    // Create a new result array containing just the sorted original indices.
    auto result_ptr = std::make_shared<Array>();
    result_ptr->shape = arr_ptr->shape;
    result_ptr->data.reserve(indexed_values.size());
    for (const auto& pair : indexed_values) {
        // APL is often 1-based, but 0-based is more common in modern languages.
        // We will stick to 0-based indices.
        result_ptr->data.push_back(static_cast<double>(pair.second));
    }

    return result_ptr;
}

// DIFF(array1, array2) -> array
// Returns a new array containing elements that are in array1 but not in array2.
BasicValue builtin_diff(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0]) || !std::holds_alternative<std::shared_ptr<Array>>(args[1])) {
        Error::set(15, vm.runtime_current_line); return {};
    }

    const auto& a_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    const auto& b_ptr = std::get<std::shared_ptr<Array>>(args[1]);
    if (!a_ptr || !b_ptr) return {};

    // 1. Create a hash set from the second array for fast lookups.
    //    We will store the string representation of each value to handle all types.
    std::unordered_set<std::string> exclusion_set;
    for (const auto& val : b_ptr->data) {
        exclusion_set.insert(to_string(val));
    }

    // 2. Iterate through the first array. If an element is NOT in the exclusion set,
    //    add it to our result.
    auto result_ptr = std::make_shared<Array>();
    for (const auto& val : a_ptr->data) {
        if (exclusion_set.find(to_string(val)) == exclusion_set.end()) {
            result_ptr->data.push_back(val);
        }
    }

    // 3. Set the shape of the resulting vector.
    result_ptr->shape = { result_ptr->data.size() };
    return result_ptr;
}

// In BuiltinFunctions.cpp, add this new function

// APPEND(array, value) -> array
// Appends a value or another array to an array, returning a new array.
BasicValue builtin_append(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) { Error::set(8, vm.runtime_current_line); return {}; }
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[0])) {
        Error::set(15, vm.runtime_current_line); // First argument must be an array
        return {};
    }

    const auto& source_array_ptr = std::get<std::shared_ptr<Array>>(args[0]);
    const BasicValue& value_to_add = args[1];

    if (!source_array_ptr) return {};

    auto result_ptr = std::make_shared<Array>();

    // 1. Copy the data from the original source array.
    result_ptr->data = source_array_ptr->data;

    // 2. Check if the value to add is also an array.
    if (std::holds_alternative<std::shared_ptr<Array>>(value_to_add)) {
        // If so, append all its elements (flattening it).
        const auto& other_array_ptr = std::get<std::shared_ptr<Array>>(value_to_add);
        if (other_array_ptr) {
            result_ptr->data.insert(result_ptr->data.end(), other_array_ptr->data.begin(), other_array_ptr->data.end());
        }
    }
    else {
        // Otherwise, just append the single scalar value.
        result_ptr->data.push_back(value_to_add);
    }

    // 3. The result of APPEND is always a flat 1D vector.
    result_ptr->shape = { result_ptr->data.size() };
    return result_ptr;
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
// CVDATE(string_expression) -> DateTime
// Parses a string like "YYYY-MM-DD" into a DateTime object.
BasicValue builtin_cvdate(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line); // Wrong number of arguments
        return false; // Return boolean false on error
    }

    std::string date_str = to_string(args[0]);
    std::tm tm = {};
    std::stringstream ss(date_str);

    // Try to parse the date in "YYYY-MM-DD" format.
    // Check for leftover characters by peeking.
    ss >> std::get_time(&tm, "%Y-%m-%d");

    if (ss.fail() || ss.peek() != EOF) {
        // If parsing failed or there's extra junk in the string.
        Error::set(15, vm.runtime_current_line); // Type Mismatch is a suitable error
        return false; // Return boolean false on error
    }

    // Convert std::tm to time_t, then to a time_point
    time_t time = std::mktime(&tm);
    if (time == -1) {
        // mktime can fail if the tm struct is invalid
        Error::set(15, vm.runtime_current_line);
        return false;
    }
    auto time_point = std::chrono::system_clock::from_time_t(time);

    // Return the new DateTime object
    return DateTime{ time_point };
}


// --- Procedures ---
BasicValue builtin_cls(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    // This function is now context-aware.
#ifdef SDL3
    if (vm.graphics_system.is_initialized) {
        // If graphics are on, CLS clears the graphics window.
        Uint8 r = 0, g = 0, b = 0;
        if (args.size() == 3) {
            r = static_cast<Uint8>(to_double(args[0]));
            g = static_cast<Uint8>(to_double(args[1]));
            b = static_cast<Uint8>(to_double(args[2]));
        }
        vm.graphics_system.clear_screen(r, g, b);
        vm.graphics_system.update_screen(); // CLS should be immediate
    }
    else {
        // Otherwise, it clears the text console.
        TextIO::clearScreen();
    }
#else
    TextIO::clearScreen();
#endif
    return false;
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

// --- High-Performance File I/O Functions ---

// TXTREADER$(filename$) -> string$
// Reads the entire content of a text file into a single string.
BasicValue builtin_txtreader_str(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 1) {
        Error::set(8, vm.runtime_current_line);
        return std::string("");
    }
    std::string filename = to_string(args[0]);
    std::ifstream infile(filename);

    if (!infile) {
        Error::set(6, vm.runtime_current_line); // File not found
        return std::string("");
    }

    // Read the whole file into a stringstream buffer, then into a string.
    std::stringstream buffer;
    buffer << infile.rdbuf();
    return buffer.str();
}


// CSVREADER(filename$, [delimiter$], [has_header_bool]) -> array
// Reads a delimited file (like CSV) into a 2D array of numbers.
BasicValue builtin_csvreader(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.empty() || args.size() > 3) {
        Error::set(8, vm.runtime_current_line);
        return {};
    }

    // --- 1. Parse Arguments ---
    std::string filename = to_string(args[0]);
    char delimiter = ',';
    bool has_header = false;

    if (args.size() > 1) {
        std::string delim_str = to_string(args[1]);
        if (!delim_str.empty()) {
            delimiter = delim_str[0];
        }
    }
    if (args.size() > 2) {
        has_header = to_bool(args[2]);
    }

    // --- 2. Open File ---
    std::ifstream infile(filename);
    if (!infile) {
        Error::set(6, vm.runtime_current_line); // File not found
        return {};
    }

    // --- 3. Read and Parse ---
    std::vector<BasicValue> flat_data;
    size_t rows = 0;
    size_t cols = 0;
    std::string line;

    // Handle header row if specified
    if (has_header && std::getline(infile, line)) {
        // Just consume the line and do nothing with it.
    }

    // Loop through the rest of the file
    while (std::getline(infile, line)) {
        rows++;
        std::stringstream line_stream(line);
        std::string cell;
        size_t current_cols = 0;

        while (std::getline(line_stream, cell, delimiter)) {
            current_cols++;
            try {
                // Try to convert each cell to a number.
                flat_data.push_back(std::stod(cell));
            }
            catch (const std::invalid_argument&) {
                // If conversion fails, store 0.0, a common practice.
                flat_data.push_back(0.0);
            }
        }

        // --- 4. Determine Shape and Validate ---
        if (rows == 1) {
            // This is the first data row, so it determines the number of columns.
            cols = current_cols;
        }
        else {
            // For all subsequent rows, verify they have the correct number of columns.
            if (current_cols != cols) {
                Error::set(15, vm.runtime_current_line); // Type Mismatch (or a new "Invalid file format" error)
                return {};
            }
        }
    }

    // --- 5. Create and Return the Array ---
    auto result_ptr = std::make_shared<Array>();
    result_ptr->shape = { rows, cols };
    result_ptr->data = flat_data;
    return result_ptr;
}

// --- NEW: High-Performance File I/O Writers ---

// TXTWRITER filename$, content$
// Writes the content of a string variable to a text file.
BasicValue builtin_txtwriter(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() != 2) {
        Error::set(8, vm.runtime_current_line);
        return false;
    }

    std::string filename = to_string(args[0]);
    std::string content = to_string(args[1]);

    std::ofstream outfile(filename);
    if (!outfile) {
        Error::set(12, vm.runtime_current_line); // File I/O Error
        return false;
    }

    outfile << content;
    return false; // Procedures return a dummy value
}

// CSVWRITER filename$, array, [delimiter$], [header_array]
// Writes a 2D array to a CSV file, with an optional header row.
BasicValue builtin_csvwriter(NeReLaBasic& vm, const std::vector<BasicValue>& args) {
    if (args.size() < 2 || args.size() > 4) {
        Error::set(8, vm.runtime_current_line);
        return false;
    }

    // 1. Parse Arguments
    std::string filename = to_string(args[0]);
    if (!std::holds_alternative<std::shared_ptr<Array>>(args[1])) {
        Error::set(15, vm.runtime_current_line); // Second arg must be an array
        return false;
    }
    const auto& array_ptr = std::get<std::shared_ptr<Array>>(args[1]);

    char delimiter = ',';
    if (args.size() >= 3) {
        std::string delim_str = to_string(args[2]);
        if (!delim_str.empty()) {
            delimiter = delim_str[0];
        }
    }

    // 2. Validate Array Shape
    if (!array_ptr || array_ptr->shape.size() != 2) {
        Error::set(15, vm.runtime_current_line); // Must be a 2D matrix
        return false;
    }

    // 3. Open File for Writing
    std::ofstream outfile(filename);
    if (!outfile) {
        Error::set(12, vm.runtime_current_line); // File I/O Error
        return false;
    }

    // Handle Optional Header Array ---
    if (args.size() == 4) {
        if (!std::holds_alternative<std::shared_ptr<Array>>(args[3])) {
            Error::set(15, vm.runtime_current_line); // Fourth arg must be an array
            return false;
        }
        const auto& header_ptr = std::get<std::shared_ptr<Array>>(args[3]);
        if (header_ptr) {
            for (size_t i = 0; i < header_ptr->data.size(); ++i) {
                outfile << to_string(header_ptr->data[i]);
                if (i < header_ptr->data.size() - 1) {
                    outfile << delimiter;
                }
            }
            outfile << '\n'; // End the header line
        }
    }

    // 4. Write Data 
    size_t rows = array_ptr->shape[0];
    size_t cols = array_ptr->shape[1];

    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            const BasicValue& val = array_ptr->data[r * cols + c];
            outfile << to_string(val);
            if (c < cols - 1) {
                outfile << delimiter;
            }
        }
        outfile << '\n';
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
    register_func("SPLIT", 2, builtin_split);
    
    // --- Register Math Functions ---
    register_func("SIN", 1, builtin_sin);
    register_func("COS", 1, builtin_cos);
    register_func("TAN", 1, builtin_tan);
    register_func("SQR", 1, builtin_sqr);
    register_func("RND", 1, builtin_rnd);

    // --- Register APL-style Array Functions ---
    register_func("IOTA", 1, builtin_iota);
    register_func("RESHAPE", -1, builtin_reshape);
    register_func("REVERSE", 1, builtin_reverse);
    register_func("TRANSPOSE", 1, builtin_transpose);
    register_func("SUM", 1, builtin_sum);
    register_func("PRODUCT", 1, builtin_product);
    register_func("MIN", 1, builtin_min);
    register_func("MAX", 1, builtin_max);
    register_func("ANY", 1, builtin_any);
    register_func("ALL", 1, builtin_all);
    register_func("MATMUL", 2, builtin_matmul);
    register_func("OUTER", 3, builtin_outer);
    register_func("TAKE", 2, builtin_take);
    register_func("DROP", 2, builtin_drop);
    register_func("GRADE", 1, builtin_grade);
    register_func("SLICE", 3, builtin_slice);
    register_func("DIFF", 2, builtin_diff);
    register_func("APPEND", 2, builtin_append);


    // --- Register Time Functions ---
    register_func("TICK", 0, builtin_tick);
    register_func("NOW", 0, builtin_now);
    register_func("DATE$", 0, builtin_date_str);
    register_func("TIME$", 0, builtin_time_str);
    register_func("DATEADD", 3, builtin_dateadd);
    register_func("CVDATE", 1, builtin_cvdate);

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
#ifdef SDL3
    register_proc("SCREEN", -1, builtin_screen);
    register_proc("PSET", -1, builtin_pset); 
    register_proc("SCREENFLIP", 0, builtin_screenflip);
    register_proc("LINE", -1, builtin_line);     // <-- ADD THIS
    register_proc("RECT", -1, builtin_rect);     // <-- ADD THIS
    register_proc("CIRCLE", -1, builtin_circle); // <-- ADD THIS
#endif
    register_proc("CLS", -1, builtin_cls);
    register_proc("LOCATE", 2, builtin_locate);
    register_proc("SLEEP", 1, builtin_sleep);   
    register_proc("CURSOR", 1, builtin_cursor);

    register_proc("DIR", -1, builtin_dir);  // -1 for optional argument
    register_proc("CD", 1, builtin_cd);
    register_proc("PWD", 0, builtin_pwd);
    register_proc("COLOR", 2, builtin_color);
    register_proc("MKDIR", 1, builtin_mkdir); 
    register_proc("KILL", 1, builtin_kill);   

    register_func("CSVREADER", -1, builtin_csvreader); // -1 for optional args
    register_func("TXTREADER$", 1, builtin_txtreader_str);
    register_proc("TXTWRITER", 2, builtin_txtwriter);
    register_proc("CSVWRITER", -1, builtin_csvwriter); // -1 for optional delimiter

}

