#pragma once
#include <variant>
#include <string>
#include <chrono>

// An enum to represent the declared type of a variable.
enum class DataType {
    DEFAULT, // Original behavior, can hold any type
    BOOL,
    INTEGER,
    DOUBLE,
    STRING,
    DATETIME
};

// A struct to hold our date/time value, based on std::chrono.
struct DateTime {
    std::chrono::system_clock::time_point time_point;

    // Default constructor initializes to the current time.
    DateTime() : time_point(std::chrono::system_clock::now()) {}

    // Constructor from a time_point.
    DateTime(const std::chrono::system_clock::time_point& tp) : time_point(tp) {}

    bool operator==(const DateTime&) const = default;
};

// This struct represents a "pointer" or "reference" to a function.
// We just store the function's name.
struct FunctionRef {
    std::string name;
    // This lets std::variant compare it if needed.
    bool operator==(const FunctionRef&) const = default;
};

// BasicValue is a universal container for any value in our language.
// It can hold either a boolean or a floating-point number.
using BasicValue = std::variant<bool, double, std::string, FunctionRef, int, DateTime>;

//==============================================================================
// HELPER FUNCTIONS
// We define them here as 'inline' so they can be used across
// multiple .cpp files without causing linker errors.
//==============================================================================

// Helper to convert a BasicValue to a double for math operations.
// This is called "coercion". It treats booleans as 1.0 or 0.0.
inline double to_double(const BasicValue& val) {
    if (std::holds_alternative<double>(val)) {
        return std::get<double>(val);
    }
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val) ? 1.0 : 0.0;
    }
    return 0.0;
}

// Helper to convert a BasicValue to a boolean for logic operations.
// It treats any non-zero number as true.
inline bool to_bool(const BasicValue& val) {
    if (std::holds_alternative<bool>(val)) {
        return std::get<bool>(val);
    }
    if (std::holds_alternative<double>(val)) {
        return std::get<double>(val) != 0.0;
    }
    return false;
}

